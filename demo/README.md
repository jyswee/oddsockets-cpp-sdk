# OddSockets C++ SDK - Live Two-Client Demo

An honest, end-to-end regression that runs entirely through the **real**
OddSockets platform: Manager -> Worker discovery over HTTP, a genuine Socket.IO
(Engine.IO v4) connection per client over a WebSocket, and live broadcast
fan-out between two **separate** connections. No mocks, no local echo, no
simulated round-trips.

## What it proves

Two independent clients, `alice` and `bob`, each open their own Socket.IO
connection to a worker. Everything below crosses the wire between them.

### Scenario 1 - core pub/sub

- `alice` subscribes to `demo-core-<nonce>`.
- `bob` publishes a message on a second connection, tagged with a
  `marker=<nonce>` that lives **only inside the payload** (never in the
  channel name), so a match proves the real payload crossed the wire rather
  than an echo of the channel it arrived on.
- `alice` receives it through her subscription handler.

### Scenario 2 - enhanced (Slack-like) events

- Both clients subscribe to `demo-enh-<nonce>`.
- `alice` registers public `on("user_typing")` + `on("reaction_added")`
  listeners.
- `bob` fires `enhanced().startTyping()` and `enhanced().addReaction()`.
- `alice` receives both broadcasts on her public `on()` surface, with the
  real server payloads (reaction counts, typing user, timestamps).

## Run it

Build from the repo **root** so the SDK source is in the Docker context, then
run with a free API key (see the SDK README for how to get one):

```bash
docker build -f demo/Dockerfile -t oddsockets-cpp-demo .
docker run --rm -e ODDSOCKETS_API_KEY="ak_..." oddsockets-cpp-demo
```

The SDK is compiled from source at image-build time, so a broken SDK fails the
build. Exit codes:

- `0` - all scenarios verified green
- `1` - missing API key / setup error
- `2` - a scenario timed out (broadcast never arrived)

## Sample output

A redacted capture of a passing run lives in [PROOF.txt](PROOF.txt). The key
lines:

```
[PASS] alice received bob's message across separate connections
[PASS] alice received user_typing AND reaction_added from bob
OK - all scenarios verified live through the OddSockets platform
```
