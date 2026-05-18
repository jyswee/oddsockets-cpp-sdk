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

## Support

- [Documentation](https://docs.oddsockets.com/sdks/cpp)
- [Issue Tracker](https://github.com/jyswee/oddsockets-cpp-sdk/issues)
- [Email Support](mailto:support@oddsockets.com)

## License

MIT License - Copyright (c) 2026 Joe Wee, Tyga.Cloud Ltd. See [LICENSE](LICENSE) for details.
