ARG AOSC_CONTAINER_URL=https://releases.aosc.io/os-loongson3/container/aosc-os_container_20251206_loongson3.tar.xz
ARG OMA_TEST_PACKAGE=antlr4

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
    && if [ ! -e /aosc-rootfs/lib64 ]; then ln -s usr/lib /aosc-rootfs/lib64; fi \
    && if [ ! -e /aosc-rootfs/lib ]; then ln -s usr/lib /aosc-rootfs/lib; fi \
    && sed -i 's/^no_check_dbus = .*/no_check_dbus = true/' /aosc-rootfs/etc/oma.toml \
    && sed -i 's/^check_battery = .*/check_battery = \"ignore\"/' /aosc-rootfs/etc/oma.toml \
    && sed -i 's/^take_wake_lock = .*/take_wake_lock = \"ignore\"/' /aosc-rootfs/etc/oma.toml \
    && rm /tmp/aosc-rootfs.tar.xz

FROM scratch
ARG OMA_TEST_PACKAGE
COPY --from=aosc-rootfs /aosc-rootfs/ /

SHELL ["/usr/bin/sh", "-c"]
ENV QEMU_CPU=Loongson-3A4000

RUN oma refresh \
    && oma install -y --no-install-recommends --no-install-suggests "${OMA_TEST_PACKAGE}" \
    && oma clean
