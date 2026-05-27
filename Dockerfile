FROM debian:bookworm-slim

# Install build tools and dev utilities
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    gcc-12 \
    g++-12 \
    clang-format \
    clang-tidy \
    gdb \
    valgrind \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set gcc-12 as default
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100

WORKDIR /workspace