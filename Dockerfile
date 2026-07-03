# ── Aurora Linux Build Environment ─────────────────────────────
# Usage:
#   docker build -t aurora-linux-builder .
#   docker run --rm -v $(pwd):/src -w /src aurora-linux-builder \
#       bash scripts/build_linux.sh

FROM ubuntu:22.04 AS builder

LABEL description="Aurora cross-platform build environment (Linux)"
LABEL maintainer="Aurora Team"

ENV DEBIAN_FRONTEND=noninteractive
ENV LLVM_VERSION=19

# System dependencies
RUN apt-get update -qq && apt-get install -y -qq \
    build-essential \
    cmake \
    ninja-build \
    curl \
    wget \
    git \
    pkg-config \
    libcurl4-openssl-dev \
    libgl1-mesa-dev \
    libx11-dev \
    libxext-dev \
    libxrender-dev \
    libxkbcommon-dev \
    libwayland-dev \
    && rm -rf /var/lib/apt/lists/*

# LLVM
RUN wget -q https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh ${LLVM_VERSION} && \
    apt-get install -y -qq \
        liblld-${LLVM_VERSION}-dev \
        libclang-${LLVM_VERSION}-dev \
    && rm -rf /var/lib/apt/lists/* llvm.sh

ENV LLVM_DIR=/usr/lib/llvm-${LLVM_VERSION}/lib/cmake/llvm
ENV PATH=/usr/lib/llvm-${LLVM_VERSION}/bin:${PATH}

WORKDIR /workspace

CMD ["bash"]
