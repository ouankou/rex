ARG BASE_IMAGE=ghcr.io/ouankou/rex:base
ARG LLVM_VERSION=22
ARG REX_LINKER=lld

FROM ${BASE_IMAGE} AS rex-base

LABEL org.opencontainers.image.source="https://github.com/ouankou/rex"

ARG LLVM_VERSION
ARG REX_LINKER
ENV LLVM_VERSION=${LLVM_VERSION}
ENV REX_LINKER=${REX_LINKER}
ENV REX_ROOT=/opt/rex
ENV REX_SOURCE_DIR=${REX_ROOT}/src
ENV REX_BUILD_DIR=${REX_SOURCE_DIR}/build
ENV REX_INSTALL_DIR=${REX_ROOT}/install
ENV PATH=${REX_INSTALL_DIR}/bin:/usr/lib/llvm-${LLVM_VERSION}/bin:${PATH}
ENV CMAKE_PREFIX_PATH=/usr/lib/llvm-${LLVM_VERSION}
ENV LLVM_DIR=/usr/lib/llvm-${LLVM_VERSION}
ENV CC=clang-${LLVM_VERSION}
ENV CXX=clang++-${LLVM_VERSION}
ENV FC=flang-${LLVM_VERSION}
ENV LD_LIBRARY_PATH=${REX_INSTALL_DIR}/lib:/usr/lib/llvm-${LLVM_VERSION}/lib

FROM rex-base AS builder

WORKDIR ${REX_SOURCE_DIR}
COPY . ${REX_SOURCE_DIR}

RUN set -eux; \
    ./build-rex.sh "${REX_INSTALL_DIR}" Debug; \
    if [ "${REX_LINKER}" = "lld" ]; then \
      grep -Eq '^CMAKE_LINKER:FILEPATH=.*/(ld\.)?lld(-[0-9]+)?$' "${REX_BUILD_DIR}/CMakeCache.txt"; \
      if [ -f "${REX_BUILD_DIR}/build.ninja" ]; then \
        grep -Eq -- '-fuse-ld=.*/(ld\.)?lld(-[0-9]+)?' "${REX_BUILD_DIR}/build.ninja"; \
      else \
        grep -REq --include=link.txt -- '-fuse-ld=.*/(ld\.)?lld(-[0-9]+)?' "${REX_BUILD_DIR}"; \
      fi; \
    fi

FROM rex-base AS runtime

COPY --from=builder ${REX_INSTALL_DIR} ${REX_INSTALL_DIR}

RUN set -eux; \
    JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")"; \
    printf '%s\n' \
      "${REX_INSTALL_DIR}/lib" \
      "/usr/lib/llvm-${LLVM_VERSION}/lib" \
      "${JAVA_HOME}/lib/server" \
      > /etc/ld.so.conf.d/rex.conf; \
    ldconfig

WORKDIR /

FROM runtime AS test

COPY --from=builder ${REX_SOURCE_DIR} ${REX_SOURCE_DIR}

WORKDIR ${REX_BUILD_DIR}

FROM runtime
