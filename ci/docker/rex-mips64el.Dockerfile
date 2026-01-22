ARG AOSC_CONTAINER_URL=https://releases.aosc.io/os-loongson3/container/aosc-os_container_20251206_loongson3.tar.xz

FROM debian:bookworm AS aosc-rootfs
ARG AOSC_CONTAINER_URL

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      ca-certificates \
      curl \
      xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /aosc-rootfs \
    && curl -fsSL "${AOSC_CONTAINER_URL}" -o /tmp/aosc-rootfs.tar.xz \
    && tar -xJf /tmp/aosc-rootfs.tar.xz -C /aosc-rootfs \
    && rm /tmp/aosc-rootfs.tar.xz

FROM scratch
COPY --from=aosc-rootfs /aosc-rootfs/ /

SHELL ["/usr/bin/sh", "-c"]

ARG LLVM_VERSION=21
ENV LLVM_VERSION=${LLVM_VERSION}

RUN oma refresh \
    && oma upgrade --autoremove -y \
    && oma install -y --no-install-recommends --no-install-suggests \
      antlr4-cpp-runtime \
      bison \
      cmake \
      curl \
      flex \
      gcc \
      ghostscript \
      git \
      graphviz \
      libharu \
      libtool \
      llvm-${LLVM_VERSION} \
      llvm-runtime-${LLVM_VERSION} \
      make \
      ninja \
      openjdk-21 \
      pkg-config \
    && oma clean

ENV PATH=/usr/lib/llvm-${LLVM_VERSION}/bin:${PATH}
ENV CMAKE_PREFIX_PATH=/usr/lib/llvm-${LLVM_VERSION}
ENV LLVM_DIR=/usr/lib/llvm-${LLVM_VERSION}
