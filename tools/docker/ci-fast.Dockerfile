FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    clang \
    clang-format \
    cmake \
    g++ \
    git \
    liberfa-dev \
    ninja-build \
    pkg-config \
    python3 \
    python3-yaml \
    wcslib-dev \
    && rm -rf /var/lib/apt/lists/*
