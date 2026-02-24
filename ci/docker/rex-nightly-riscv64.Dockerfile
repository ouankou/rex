ARG BASE_IMAGE=ghcr.io/ouankou/rex:base
FROM ${BASE_IMAGE}

LABEL org.opencontainers.image.source="https://github.com/ouankou/rex"

ARG LLVM_VERSION=21
ENV LLVM_VERSION=${LLVM_VERSION}
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

WORKDIR ${REX_SOURCE_DIR}
COPY . ${REX_SOURCE_DIR}

RUN cmake -S "${REX_SOURCE_DIR}" -B "${REX_BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_INSTALL_PREFIX="${REX_INSTALL_DIR}" \
      -DENABLE-C=ON \
      -DENABLE-FORTRAN=ON \
      -DENABLE-FORTRAN-FLANG=ON \
      -DCMAKE_CXX_STANDARD=17 \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    && cmake --build "${REX_BUILD_DIR}" -j"$(nproc)" \
    && cmake --install "${REX_BUILD_DIR}"

RUN set -eux; \
    JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")"; \
    printf '%s\n' \
      "${REX_INSTALL_DIR}/lib" \
      "/usr/lib/llvm-${LLVM_VERSION}/lib" \
      "${JAVA_HOME}/lib/server" \
      > /etc/ld.so.conf.d/rex.conf; \
    ldconfig

WORKDIR ${REX_BUILD_DIR}
