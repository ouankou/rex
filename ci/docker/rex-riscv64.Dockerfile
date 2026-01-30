FROM debian:sid

ARG LLVM_VERSION=21
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
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
      ghostscript \
      git \
      graphviz \
      libantlr4-runtime-dev \
      libclang-${LLVM_VERSION}-dev \
      libclang-cpp${LLVM_VERSION}-dev \
      libflang-${LLVM_VERSION}-dev \
      libhpdf-dev \
      libtool \
      llvm-${LLVM_VERSION} \
      llvm-${LLVM_VERSION}-dev \
      openjdk-25-jdk \
      pkg-config \
    && rm -rf /var/lib/apt/lists/*

ENV LLVM_VERSION=${LLVM_VERSION}
