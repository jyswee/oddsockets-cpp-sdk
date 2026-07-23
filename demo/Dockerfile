# OddSockets C++ SDK - two-client honest regression demo
#
# Build from the repo ROOT so the SDK source is in context:
#
#   docker build -f demo/Dockerfile -t oddsockets-cpp-demo .
#   docker run --rm -e ODDSOCKETS_API_KEY="ak_your_key_here" oddsockets-cpp-demo
#
# A successful run prints
#   "OK - all scenarios verified live through the OddSockets platform"
# and exits 0. The SDK is compiled from source at image-build time, so a broken
# SDK fails the build.

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
      g++ \
      libc6-dev \
      libwebsockets-dev \
      libcurl4-openssl-dev \
      libssl-dev \
      pkg-config \
      ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /sdk
COPY . /sdk

# Compile the SDK sources together with the demo. libwebsockets carries the
# Engine.IO/Socket.IO frames; libcurl performs manager discovery.
RUN g++ -std=c++17 -O2 -Wall -I include \
      demo/demo.cpp \
      src/Types.cpp \
      src/OddSockets.cpp \
      src/Channel.cpp \
      src/EnhancedFeatures.cpp \
      src/ManagerDiscovery.cpp \
      -o /usr/local/bin/oddsockets-demo \
      $(pkg-config --cflags --libs libwebsockets) \
      -lcurl -lssl -lcrypto -lpthread

CMD ["oddsockets-demo"]
