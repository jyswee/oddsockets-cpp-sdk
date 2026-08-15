# OddSockets C++ SDK

Official C++ SDK for OddSockets real-time messaging platform. Modern C++17, async, header-only option. libcurl + libwebsockets.

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Quick Start

```cpp
#include <oddsockets/OddSockets.hpp>

oddsockets::Config config;
config.apiKey = "YOUR_API_KEY";
config.userId = "my-agent";

oddsockets::OddSockets client(config);
auto channel = client.channel("my-channel");
channel->subscribe([](const std::string& msg) { std::cout << msg << std::endl; });
channel->publish("{\"text\":\"Hello from C++\"}");
```

## Enhanced Features

Enhanced (Slack-like) events layer on top of the core pub/sub. The **send** side
lives on `client.enhanced()`; each call is fire-and-forget (`void`) and emits a real
worker event over the live socket. The matching **broadcast** arrives on the raw
`client.on("<event>", ...)` surface as a JSON payload string, so any subscriber in
the channel room can react.

```cpp
#include <oddsockets/OddSockets.hpp>
#include <iostream>

oddsockets::Config config;
config.apiKey = "YOUR_API_KEY";
config.userId = "alice";
config.autoConnect = false;

oddsockets::OddSockets client(config);
client.connect().get();

auto channel = client.channel("room-42");
channel->subscribe([](const std::string&) {}).get();

// Receive-path: enhanced broadcasts arrive on the raw on() surface as JSON strings
client.on("user_typing",    [](const std::string& payload) { std::cout << "typing: "   << payload << "\n"; });
client.on("reaction_added", [](const std::string& payload) { std::cout << "reaction: " << payload << "\n"; });

// Send-path: fire enhanced actions over the live socket
client.enhanced().startTyping("alice", "room-42");
client.enhanced().addReaction("msg-1", "room-42", ":thumbsup:", "alice", "Alice");
```

### Event surface

| Area | Send (`client.enhanced()`) | Broadcast (`client.on(...)`) |
|---|---|---|
| **Typing** | `startTyping(userId, channel)` · `stopTyping(userId, channel)` | `user_typing` · `user_stopped_typing` |
| **Reactions** | `addReaction(messageId, channel, emoji, userId, userName)` · `removeReaction(messageId, channel, emoji, userId)` | `reaction_added` · `reaction_removed` |

The C++ enhanced surface is deliberately focused on typing and reactions. Any other
worker event your channel emits is still available directly on `client.on("<event>", ...)`.

## Get a Free API Key

```bash
curl -X POST https://oddsockets.com/api/agent-signup \
  -H "Content-Type: application/json" \
  -d '{"email": "you@example.com", "agentName": "my-agent", "platform": "cpp"}'
curl -X POST https://oddsockets.com/api/agent-signup/verify \
  -H "Content-Type: application/json" \
  -d '{"email": "you@example.com", "code": "123456", "agentName": "my-agent"}'
```

## Plans

| | Free | Starter | Pro |
|---|---|---|---|
| **Price** | $0/mo | $49.99/mo | $299/mo |
| **MAU** | 100 | 1,000 | 50,000 |
| **Concurrent connections** | 50 | 1,000 | Unlimited |
| **Messages/day** | 10,000 | 4,320,000 | Unlimited |
| **Channels** | 10 | Unlimited | Unlimited |
| **Storage** | 100MB (24h) | 50GB (6 months) | Unlimited |

## Get Accredited

<a href="https://tyga.games/accreditation"><img src="https://prodmedia.tyga.host/public/tyga.cloud/landing/tyga.games/tygagames-black-words.svg" alt="tyga.games accreditation" height="44"></a>

Prove you can build and operate real-time features on OddSockets — channels, presence, pub/sub, delivery guarantees and production liveops — on the stack itself. Three tiers (**TCU / TCA / TCP**), certified through **tyga.games** and delivered on ClassaaS.

[**Get accredited on tyga.games →**](https://tyga.games/accreditation)

## Support

- [Documentation](https://docs.oddsockets.com/sdks/cpp)
- [Issue Tracker](https://github.com/jyswee/oddsockets-cpp-sdk/issues)
- [Email Support](mailto:support@oddsockets.com)

## License

MIT License - Copyright (c) 2026 Joe Wee, Tyga.Cloud Ltd. See [LICENSE](LICENSE) for details.
