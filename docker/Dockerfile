FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        binutils-mingw-w64-x86-64 \
        cmake \
        g++ \
        git \
        g++-mingw-w64-x86-64-posix \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY docker/build.sh /usr/local/bin/imetoggle-build

CMD ["sleep", "infinity"]
