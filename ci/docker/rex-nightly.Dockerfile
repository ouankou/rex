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
ARG REX_ENABLE_UNINITIALIZED_FIELD_TESTS=
ARG REX_ENABLE_X86_SIMD_TESTS=OFF

# CMake enables Valgrind-backed tests only when both the executable and headers
# are present at configure time.  Install both in selected development builders
# and their final test images so the copied test registry remains executable.
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

# Only the development image selects the cross-target SIMD test suite.  On a
# non-x86 host its builder must provide the corresponding target sysroot before
# CMake registers those tests.
RUN set -eux; \
    case "${REX_ENABLE_X86_SIMD_TESTS}" in \
      OFF) ;; \
      ON) \
        case "$(dpkg --print-architecture)" in \
          amd64) ;; \
          *) \
            apt-get update; \
            apt-get install -y --no-install-recommends libc6-dev-amd64-cross; \
            test -f /usr/x86_64-linux-gnu/include/stdlib.h; \
            test -f /usr/x86_64-linux-gnu/include/bits/libc-header-start.h; \
            rm -rf /var/lib/apt/lists/*; \
            ;; \
        esac \
        ;; \
      *) echo "REX_ENABLE_X86_SIMD_TESTS must be ON or OFF" >&2; exit 2 ;; \
    esac

WORKDIR ${REX_SOURCE_DIR}
COPY . ${REX_SOURCE_DIR}

RUN set -eux; \
    case "${REX_ENABLE_UNINITIALIZED_FIELD_TESTS}" in \
      ""|OFF|ON) ;; \
      *) echo "REX_ENABLE_UNINITIALIZED_FIELD_TESTS must be ON or OFF" >&2; exit 2 ;; \
    esac; \
    simd_sysroot=; \
    if [ "${REX_ENABLE_X86_SIMD_TESTS}" = ON ] && \
       [ "$(dpkg --print-architecture)" != amd64 ]; then \
      simd_sysroot=/usr/x86_64-linux-gnu; \
    fi; \
    NUM_JOBS="${REX_BUILD_NUM_JOBS}" \
      REX_ENABLE_UNINITIALIZED_FIELD_TESTS="${REX_ENABLE_UNINITIALIZED_FIELD_TESTS}" \
      REX_ENABLE_X86_SIMD_TESTS="${REX_ENABLE_X86_SIMD_TESTS}" \
      REX_X86_SIMD_TEST_SYSROOT="${simd_sysroot}" \
      ./build-rex.sh "${REX_INSTALL_DIR}" Debug

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
ARG REX_ENABLE_X86_SIMD_TESTS=OFF
RUN set -eux; \
    packages=; \
    case "${REX_ENABLE_VALGRIND}" in \
      0) ;; \
      1) packages="${packages} valgrind" ;; \
      *) echo "REX_ENABLE_VALGRIND must be 0 or 1" >&2; exit 2 ;; \
    esac; \
    case "${REX_ENABLE_X86_SIMD_TESTS}" in \
      OFF) ;; \
      ON) \
        if [ "$(dpkg --print-architecture)" != amd64 ]; then \
          packages="${packages} libc6-dev-amd64-cross"; \
        fi \
        ;; \
      *) echo "REX_ENABLE_X86_SIMD_TESTS must be ON or OFF" >&2; exit 2 ;; \
    esac; \
    if [ -n "${packages}" ]; then \
      apt-get update; \
      apt-get install -y --no-install-recommends ${packages}; \
      rm -rf /var/lib/apt/lists/*; \
    fi; \
    if [ "${REX_ENABLE_X86_SIMD_TESTS}" = ON ] && \
       [ "$(dpkg --print-architecture)" != amd64 ]; then \
      test -f /usr/x86_64-linux-gnu/include/stdlib.h; \
      test -f /usr/x86_64-linux-gnu/include/bits/libc-header-start.h; \
    fi

COPY --from=builder ${REX_SOURCE_DIR} ${REX_SOURCE_DIR}

WORKDIR ${REX_BUILD_DIR}

FROM runtime
