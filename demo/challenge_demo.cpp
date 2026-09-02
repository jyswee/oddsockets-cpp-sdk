// OddSockets C++ SDK - two-client CHALLENGE regression (honest, live QA)
//
// alice + bob: distinct userId, SAME apiKey (shared owner scope). Both subscribe
// to 'lobby'. Everything runs through the REAL platform: Manager -> Worker
// discovery over HTTP, a genuine Socket.IO connection per client, live broadcast
// fan-out between two SEPARATE connections. No mocks, no local echo.
//
// Covers worker v1.2 challenge/leaderboard/achievement/invite surface via
// EnhancedFeatures. Room broadcasts arrive wrapped {version,type,identity,
// challengeId,data:{...}}; directed invite/reply/cancel are FLAT. on() handlers
// receive raw JSON strings; we substring/field-check.
//
// Exit: 0 all assertions PASS, 2 one or more FAILED, 1 setup/connect error.

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "oddsockets/OddSockets.hpp"
#include "oddsockets/EnhancedFeatures.hpp"

using namespace oddsockets;
using namespace std::chrono_literals;

namespace {

std::mutex g_logMu;
void logline(const std::string& s) {
    std::lock_guard<std::mutex> lk(g_logMu);
    std::cout << s << "\n";
}

std::string randHex() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llx", static_cast<unsigned long long>(dist(gen)));
    return std::string(buf);
}

// A captured event. Keeps EVERY payload seen for a named event so a busy shared
// room (other tenants under the same owner scope also broadcast into 'lobby')
// cannot mask the specific frame we are asserting on. seen = "at least one".
struct Capture {
    std::atomic<bool> seen{false};
    std::mutex mu;
    std::string payload;                 // most-recent payload
    std::vector<std::string> all;        // every payload observed
    void set(const std::string& p) {
        { std::lock_guard<std::mutex> lk(mu); payload = p; all.push_back(p); }
        seen = true;
    }
    std::string get() { std::lock_guard<std::mutex> lk(mu); return payload; }
    // True once ANY captured payload contains needle. Robust against interleaved
    // frames from a shared room, unlike a single-slot "last payload" check.
    bool waitForNeedle(const std::string& needle, int seconds) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lk(mu);
                for (auto& p : all) if (p.find(needle) != std::string::npos) return true;
            }
            std::this_thread::sleep_for(50ms);
        }
        std::lock_guard<std::mutex> lk(mu);
        for (auto& p : all) if (p.find(needle) != std::string::npos) return true;
        return false;
    }
};

bool waitFor(std::atomic<bool>& flag, int seconds) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (flag.load()) return true;
        std::this_thread::sleep_for(50ms);
    }
    return flag.load();
}

// Pull a top-level "key":"value" string field out of a flat JSON object.
// Sufficient for the ack shapes we assert on (challengeId lives at top level).
std::string jsonField(const std::string& obj, const std::string& key) {
    std::string pat = "\"" + key + "\":\"";
    auto pos = obj.find(pat);
    if (pos == std::string::npos) return "";
    pos += pat.size();
    auto end = obj.find('"', pos);
    if (end == std::string::npos) return "";
    return obj.substr(pos, end - pos);
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

std::unique_ptr<OddSockets> connectClient(const std::string& apiKey,
                                          const std::string& userId,
                                          const std::string& managerUrl) {
    Config config;
    config.apiKey = apiKey;
    config.userId = userId;
    if (!managerUrl.empty()) config.managerUrl = managerUrl;
    config.autoConnect = false;
    config.enableLogging = false;
    auto client = std::make_unique<OddSockets>(config);
    if (!client->connect().get()) return nullptr;
    return client;
}

// Assertion bookkeeping.
struct Results {
    std::mutex mu;
    std::vector<std::pair<std::string, bool>> rows;
    void add(const std::string& name, bool ok) {
        std::lock_guard<std::mutex> lk(mu);
        rows.emplace_back(name, ok);
        logline(std::string(ok ? "[PASS] " : "[FAIL] ") + name);
    }
    bool allGreen() {
        std::lock_guard<std::mutex> lk(mu);
        for (auto& r : rows) if (!r.second) return false;
        return true;
    }
    void summary() {
        std::lock_guard<std::mutex> lk(mu);
        int pass = 0;
        for (auto& r : rows) if (r.second) pass++;
        logline("\n=== SUMMARY: " + std::to_string(pass) + "/" +
                std::to_string(rows.size()) + " assertions passed ===");
        for (auto& r : rows)
            logline(std::string("  ") + (r.second ? "PASS " : "FAIL ") + r.first);
    }
};

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    const char* keyEnv = std::getenv("OS_KEY");
    if (!keyEnv || !keyEnv[0]) keyEnv = std::getenv("ODDSOCKETS_API_KEY");
    if (!keyEnv || !keyEnv[0]) {
        std::cerr << "Missing OS_KEY / ODDSOCKETS_API_KEY.\n";
        return 1;
    }
    std::string apiKey(keyEnv);
    const char* mgrEnv = std::getenv("ODDSOCKETS_MANAGER_URL");
    std::string managerUrl = mgrEnv ? mgrEnv : "";
    std::string nonce = randHex();
    std::string chan = "lobby";
    std::string challengeId = "chal-" + nonce;
    std::string achId = "ach-" + nonce;
    // The worker reassigns achievementId (like challengeId), but round-trips the
    // display name verbatim in every broadcast. Use a unique name as the
    // correlation key so shared-room cross-talk can't satisfy the assert.
    std::string achName = "FirstBlood-" + nonce;

    Results R;

    // Inbound captures (registered on alice, the observer of room broadcasts).
    Capture capProgress;      // challenge_progress
    Capture capRankChange;    // leaderboard_rank_change
    Capture capComplete;      // challenge_complete
    Capture capAchUnlock;     // achievement_unlock
    Capture capAchProgress;   // achievement_progress
    // Directed captures on bob (invitee) and alice (inviter).
    Capture capInvited;       // challenge_invited (bob)
    Capture capReplyRecv;     // challenge_reply_received (alice)
    Capture capInviteCancel;  // challenge_invite_cancelled (bob)
    // Acks land on the emitting client.
    Capture capCreateAck;     // challenge_create_success (alice)
    Capture capStandingsAck;  // challenge_standings_success (alice)
    Capture capCompleteAck;   // challenge_complete_success (alice)
    Capture capAchState;      // achievement_state (alice)
    Capture capInviteAck;     // challenge_invite_success (alice)
    Capture capInvitesList;   // challenge_invites (bob)
    Capture capReplyAck;      // challenge_reply_success (bob)
    Capture capCancelAck;     // challenge_invite_cancel_success (alice)
    Capture capErr;           // any error event

    try {
        auto alice = connectClient(apiKey, "alice", managerUrl);
        auto bob = connectClient(apiKey, "bob", managerUrl);
        if (!alice || !bob) {
            std::cerr << "ERROR: could not connect both clients\n";
            return 1;
        }
        auto wa = alice->getWorkerInfo();
        auto wb = bob->getWorkerInfo();
        std::string wida = wa ? wa->workerId : "?";
        std::string widb = wb ? wb->workerId : "?";
        logline("[connect] alice -> " + wida + ", bob -> " + widb);
        logline(std::string("[cross-worker] ") +
                (wida != widb ? "YES (distinct workers, real Redis fan-out)"
                              : "no (same worker) - fan-out still via room"));

        // ---- alice ack + inbound listeners ----
        alice->on("challenge_create_success", [&](const std::string& p){ logline("[alice ack challenge_create_success] " + p); capCreateAck.set(p); });
        alice->on("challenge_standings_success", [&](const std::string& p){ logline("[alice ack challenge_standings_success] " + p); capStandingsAck.set(p); });
        alice->on("challenge_complete_success", [&](const std::string& p){ logline("[alice ack challenge_complete_success] " + p); capCompleteAck.set(p); });
        alice->on("achievement_state", [&](const std::string& p){ logline("[alice ack achievement_state] " + p); capAchState.set(p); });
        alice->on("challenge_invite_success", [&](const std::string& p){ logline("[alice ack challenge_invite_success] " + p); capInviteAck.set(p); });
        alice->on("challenge_invite_cancel_success", [&](const std::string& p){ logline("[alice ack challenge_invite_cancel_success] " + p); capCancelAck.set(p); });
        alice->on("challenge_progress", [&](const std::string& p){ logline("[alice recv challenge_progress] " + p); capProgress.set(p); });
        alice->on("leaderboard_rank_change", [&](const std::string& p){ logline("[alice recv leaderboard_rank_change] " + p); capRankChange.set(p); });
        alice->on("challenge_complete", [&](const std::string& p){ logline("[alice recv challenge_complete] " + p); capComplete.set(p); });
        alice->on("achievement_unlock", [&](const std::string& p){ logline("[alice recv achievement_unlock] " + p); capAchUnlock.set(p); });
        alice->on("achievement_progress", [&](const std::string& p){ logline("[alice recv achievement_progress] " + p); capAchProgress.set(p); });
        alice->on("challenge_reply_received", [&](const std::string& p){ logline("[alice recv challenge_reply_received] " + p); capReplyRecv.set(p); });
        alice->on("error", [&](const std::string& p){ logline("[alice ERROR] " + p); capErr.set(p); });

        // ---- bob listeners (invitee + own acks) ----
        bob->on("challenge_invited", [&](const std::string& p){ logline("[bob recv challenge_invited] " + p); capInvited.set(p); });
        bob->on("challenge_invite_cancelled", [&](const std::string& p){ logline("[bob recv challenge_invite_cancelled] " + p); capInviteCancel.set(p); });
        bob->on("challenge_invites", [&](const std::string& p){ logline("[bob ack challenge_invites] " + p); capInvitesList.set(p); });
        bob->on("challenge_reply_success", [&](const std::string& p){ logline("[bob ack challenge_reply_success] " + p); capReplyAck.set(p); });
        bob->on("error", [&](const std::string& p){ logline("[bob ERROR] " + p); capErr.set(p); });

        // Both subscribe to 'lobby'.
        auto aliceCh = alice->channel(chan);
        auto bobCh = bob->channel(chan);
        aliceCh->subscribe([](const std::string&){}).get();
        bobCh->subscribe([](const std::string&){}).get();
        logline("[alice/bob] subscribed to '" + chan + "'");
        std::this_thread::sleep_for(600ms);

        // Every assertion keys on this run's UNIQUE challengeId/inviteId via
        // waitForNeedle(), so concurrent frames from other clients sharing the
        // 'lobby' room + owner scope cannot mask (or falsely satisfy) a check.

        // ---- 1. createChallenge -> ack ----
        // NOTE: the worker ASSIGNS its own challengeId and returns it in the ack;
        // the caller-supplied challengeId is a create-time hint, not the room key.
        // Capture the worker id and use it for every downstream challenge op, or
        // the room broadcasts (keyed on the real id) would never match.
        logline("\n--- 1. createChallenge ---");
        alice->enhanced().createChallenge(challengeId, "score", true, chan);
        bool createOk = capCreateAck.waitForNeedle("\"challengeId\"", 10);
        std::string realChallengeId = jsonField(capCreateAck.get(), "challengeId");
        if (realChallengeId.empty()) realChallengeId = challengeId; // fallback: echoed id
        logline("[create] sent challengeId=" + challengeId +
                "  worker-assigned=" + realChallengeId);
        R.add("createChallenge ack (challenge_create_success)", createOk);

        // ---- 2. reportProgress -> alice sees challenge_progress + leaderboard_rank_change ----
        logline("\n--- 2. reportProgress (bob) ---");
        std::this_thread::sleep_for(300ms);
        bob->enhanced().reportProgress(realChallengeId, 42, "score", "evt-" + nonce + "-1");
        // Also have alice report so standings have >1 identity.
        alice->enhanced().reportProgress(realChallengeId, 90, "score", "evt-" + nonce + "-2");
        R.add("alice sees challenge_progress after bob progress",
              capProgress.waitForNeedle(realChallengeId, 10));
        R.add("alice sees leaderboard_rank_change after bob progress",
              capRankChange.waitForNeedle(realChallengeId, 10));
        std::this_thread::sleep_for(800ms);

        // ---- 3. getStandings -> ordered w/ yourRank ----
        logline("\n--- 3. getStandings (alice) ---");
        alice->enhanced().getStandings(realChallengeId, 20);
        bool standings = capStandingsAck.waitForNeedle(realChallengeId, 10);
        std::string sp = capStandingsAck.get();
        R.add("getStandings ack w/ standings + yourRank",
              standings && contains(sp, "standings") && contains(sp, "yourRank"));

        // ---- 4. completeChallenge tied/conceded -> ack w/ finalValue + rank ----
        logline("\n--- 4. completeChallenge (tied) ---");
        alice->enhanced().completeChallenge(realChallengeId, "tied", "evt-" + nonce + "-done");
        bool complete = capCompleteAck.waitForNeedle(realChallengeId, 10);
        std::string cp = capCompleteAck.get();
        R.add("completeChallenge tied ack w/ finalValue + rank",
              complete && contains(cp, "finalValue") && contains(cp, "rank"));
        // broadcast challenge_complete to room (keyed on the worker-assigned id)
        R.add("alice sees challenge_complete broadcast",
              capComplete.waitForNeedle(realChallengeId, 8));

        // ---- 5a. unlockAchievement 50 -> achievement_progress in_progress ----
        // Correlate on the unique achName (worker reassigns achievementId).
        logline("\n--- 5. unlockAchievement 50 then 100 ---");
        alice->enhanced().unlockAchievement(achId, achName, "", 50.0, challengeId, chan);
        bool achProg = capAchProgress.waitForNeedle(achName, 10);
        R.add("unlock 50 => achievement_progress in_progress",
              achProg && contains(capAchProgress.get(), "in_progress"));

        // ---- 5b. unlockAchievement 100 -> achievement_unlock unlocked ----
        std::this_thread::sleep_for(400ms);
        alice->enhanced().unlockAchievement(achId, achName, "", 100.0, challengeId, chan);
        bool achUnlock = capAchUnlock.waitForNeedle(achName, 10);
        R.add("unlock 100 => achievement_unlock unlocked",
              achUnlock && contains(capAchUnlock.get(), "unlocked"));
        // Capture the worker-assigned achievementId from the unlock broadcast so
        // getAchievements can be verified against the same record.
        std::string realAchId = jsonField(capAchUnlock.get(), "achievementId");
        logline("[achievement] name=" + achName + " worker-assigned id=" + realAchId);

        // ---- 6. getAchievements reflects unlocked ----
        logline("\n--- 6. getAchievements ---");
        std::this_thread::sleep_for(400ms);
        // Query by the worker-assigned id; assert the ack carries that id as unlocked.
        alice->enhanced().getAchievements(realAchId);
        bool achState = capAchState.waitForNeedle("achievements", 10);
        // Pull the most recent achievement_state that mentions our real id.
        std::string as;
        for (int i = 0; i < 20 && as.empty(); ++i) {
            {
                std::lock_guard<std::mutex> lk(capAchState.mu);
                for (auto it = capAchState.all.rbegin(); it != capAchState.all.rend(); ++it)
                    if (!realAchId.empty() && it->find(realAchId) != std::string::npos) { as = *it; break; }
            }
            if (as.empty()) std::this_thread::sleep_for(300ms);
        }
        R.add("getAchievements reflects unlocked",
              !as.empty() && contains(as, "achievements") && contains(as, "unlocked"));

        // ---- 7. sendChallengeInvite -> invitee (bob) sees challenge_invited from alice ----
        logline("\n--- 7. sendChallengeInvite alice->bob ---");
        std::string inviteId = "inv-" + nonce;
        alice->enhanced().sendChallengeInvite("bob", "match", "", 300, chan, inviteId);
        bool inviteAck = capInviteAck.waitForNeedle(inviteId, 10);
        std::string ia = capInviteAck.get();
        R.add("sendChallengeInvite ack {inviteId,toUserId,status:pending}",
              inviteAck && contains(ia, "inviteId") && contains(ia, "pending"));
        // directed FLAT event; the specific invite must reference alice as inviter
        bool invited = capInvited.waitForNeedle(inviteId, 10);
        R.add("bob (invitee) sees challenge_invited from alice",
              invited && contains(capInvited.get(), "alice"));

        // ---- 8. getChallengeInvites lists it (bob) ----
        logline("\n--- 8. getChallengeInvites (bob) ---");
        std::this_thread::sleep_for(300ms);
        bob->enhanced().getChallengeInvites();
        bool invites = capInvitesList.waitForNeedle(inviteId, 10);
        std::string il = capInvitesList.get();
        R.add("getChallengeInvites lists the pending invite",
              invites && contains(il, "invites"));

        // ---- 9. replyChallengeInvite accept -> inviter (alice) sees challenge_reply_received ----
        logline("\n--- 9. replyChallengeInvite accept (bob) ---");
        bob->enhanced().replyChallengeInvite(inviteId, true);
        R.add("replyChallengeInvite ack (challenge_reply_success)",
              capReplyAck.waitForNeedle(inviteId, 10));
        R.add("alice (inviter) sees challenge_reply_received",
              capReplyRecv.waitForNeedle(inviteId, 10));

        // ---- 10. cancelChallengeInvite -> invitee (bob) sees challenge_invite_cancelled ----
        // Use a second invite so cancel has a live, pending target.
        logline("\n--- 10. cancelChallengeInvite alice->bob ---");
        std::string inviteId2 = "inv2-" + nonce;
        alice->enhanced().sendChallengeInvite("bob", "match", "", 300, chan, inviteId2);
        capInviteAck.waitForNeedle(inviteId2, 8);
        std::this_thread::sleep_for(400ms);
        alice->enhanced().cancelChallengeInvite(inviteId2);
        R.add("cancelChallengeInvite ack (challenge_invite_cancel_success)",
              capCancelAck.waitForNeedle(inviteId2, 10));
        R.add("bob (invitee) sees challenge_invite_cancelled",
              capInviteCancel.waitForNeedle(inviteId2, 10));

        std::this_thread::sleep_for(500ms);
        aliceCh->unsubscribe().get();
        bobCh->unsubscribe().get();
        alice->disconnect();
        bob->disconnect();

        R.summary();
        logline(std::string("[workers] alice=") + wida + " bob=" + widb);
        if (R.allGreen()) {
            logline("\nOK - ALL challenge assertions verified live through OddSockets QA");
            return 0;
        }
        logline("\nFAIL - one or more challenge assertions did not pass");
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
