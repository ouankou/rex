ARG BASE_IMAGE=ghcr.io/ouankou/rex:base
ARG LLVM_VERSION=22

FROM ${BASE_IMAGE} AS rex-base

LABEL org.opencontainers.image.source="https://github.com/ouankou/rex"

ARG LLVM_VERSION
ENV LLVM_VERSION=${LLVM_VERSION}
RUN test "${LLVM_VERSION}" = "22"
ENV REX_ROOT=/opt/rex
ENV REX_SOURCE_DIR=${REX_ROOT}/src
ENV REX_BUILD_DIR=${REX_SOURCE_DIR}/build
ENV REX_INSTALL_DIR=${REX_ROOT}/install
ENV PATH=${REX_INSTALL_DIR}/bin:/usr/lib/llvm-${LLVM_VERSION}/bin:${PATH}
ENV CMAKE_PREFIX_PATH=/usr/lib/llvm-${LLVM_VERSION}
ENV LLVM_DIR=/usr/lib/llvm-${LLVM_VERSION}/lib/cmake/llvm
ENV CC=clang-${LLVM_VERSION}
ENV CXX=clang++-${LLVM_VERSION}
ENV FC=flang-${LLVM_VERSION}
ENV LD_LIBRARY_PATH=${REX_INSTALL_DIR}/lib:/usr/lib/llvm-${LLVM_VERSION}/lib

FROM rex-base AS builder

ARG REX_BUILD_NUM_JOBS=4
ARG REX_ENABLE_VALGRIND=0

# CMake registers the Memcheck-only tests only when Valgrind is present at
# configure time.  Install it in the builder as well as the final test image
# so the copied build tree and its runtime tools have one exact contract.
RUN set -eux; \
    case "${REX_ENABLE_VALGRIND}" in \
      0) ;; \
      1) \
        apt-get update; \
        apt-get install -y --no-install-recommends valgrind; \
        rm -rf /var/lib/apt/lists/*; \
        ;; \
      *) echo "REX_ENABLE_VALGRIND must be 0 or 1" >&2; exit 2 ;; \
    esac

WORKDIR ${REX_SOURCE_DIR}
COPY . ${REX_SOURCE_DIR}

RUN NUM_JOBS="${REX_BUILD_NUM_JOBS}" ./build-rex.sh "${REX_INSTALL_DIR}" Debug

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

ARG REX_ENABLE_VALGRIND=0
RUN set -eux; \
    case "${REX_ENABLE_VALGRIND}" in \
      0) ;; \
      1) \
        apt-get update; \
        apt-get install -y --no-install-recommends valgrind; \
        rm -rf /var/lib/apt/lists/*; \
        ;; \
      *) echo "REX_ENABLE_VALGRIND must be 0 or 1" >&2; exit 2 ;; \
    esac

COPY --from=builder ${REX_SOURCE_DIR} ${REX_SOURCE_DIR}

WORKDIR ${REX_BUILD_DIR}

FROM runtime
