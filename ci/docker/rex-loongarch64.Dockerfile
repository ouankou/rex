FROM ghcr.io/loong64/debian:sid
LABEL org.opencontainers.image.source="https://github.com/ouankou/rex"

ARG LLVM_VERSION=22
ENV DEBIAN_FRONTEND=noninteractive

RUN test "${LLVM_VERSION}" = "22" \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
      antlr4 \
      bison \
      build-essential \
      ca-certificates \
      cmake \
      clang-${LLVM_VERSION} \
      curl \
      flang-${LLVM_VERSION} \
      flex \
      g++ \
      gdb \
      ghostscript \
      git \
      graphviz \
      lld-${LLVM_VERSION} \
      libantlr4-runtime-dev \
      libclang-${LLVM_VERSION}-dev \
      libclang-cpp${LLVM_VERSION}-dev \
      libclang-rt-${LLVM_VERSION}-dev \
      libflang-${LLVM_VERSION}-dev \
      libhpdf-dev \
      libomp-${LLVM_VERSION}-dev \
      libtool \
      lldb-${LLVM_VERSION} \
      llvm-${LLVM_VERSION} \
      llvm-${LLVM_VERSION}-dev \
      mold \
      openjdk-25-jdk-headless \
      pkg-config \
    && rm -rf /var/lib/apt/lists/*

ENV LLVM_VERSION=${LLVM_VERSION}
