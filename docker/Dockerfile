# syntax=docker/dockerfile:1
#
# Stage 1: BusyBox'ı RISC-V64 için statik (musl) derle.
# Cartesi machine RISC-V64 Linux üzerinde çalışır.
FROM --platform=linux/amd64 riscv64/alpine:3.19 AS busybox-build

ARG BUSYBOX_VERSION=1.36.1

RUN apk add --no-cache build-base wget tar xz musl-dev linux-headers

WORKDIR /src
RUN wget -q https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2 \
    && tar xjf busybox-${BUSYBOX_VERSION}.tar.bz2 \
    && mv busybox-${BUSYBOX_VERSION} busybox

WORKDIR /src/busybox
# En basit config: hemen hemen tüm applet'leri aç, statik link.
RUN make defconfig \
    && sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config \
    && sed -i 's/^CONFIG_STATIC=n/CONFIG_STATIC=y/' .config \
    && echo "CONFIG_STATIC=y" >> .config \
    && make oldconfig < /dev/null \
    && make -j"$(nproc)" \
    && make install CONFIG_PREFIX=/rootfs

# Stage 1b: echo DApp'i statik derle (aynı toolchain, ekstra bağımlılık yok)
FROM busybox-build AS dapp-build

WORKDIR /src/dapp
COPY dapp/app.c .
RUN gcc -static -O2 -o app app.c

# Stage 2: minimal rootfs iskeletini kur
FROM scratch AS rootfs

COPY --from=busybox-build /rootfs /
COPY --from=dapp-build /src/dapp/app /bin/app

# Gerekli dizinler
COPY rootfs-overlay/ /

# init scriptini çalıştırılabilir yap (COPY --chmod, buildkit gerektirir)
# Eğer buildkit yoksa: RUN chmod +x /init  (bu stage scratch olduğu için
# aşağıdaki final stage'de chmod uygulanıyor)

FROM busybox-build AS finalize
COPY --from=rootfs / /rootfs-final
RUN chmod +x /rootfs-final/init /rootfs-final/bin/app \
    && mkdir -p /rootfs-final/{proc,sys,dev,tmp,mnt}

# Çıktı: rootfs'i ext2 image + cpio.gz olarak paketle
RUN apk add --no-cache e2fsprogs cpio gzip
WORKDIR /rootfs-final
RUN find . | cpio -o -H newc | gzip -9 > /out-initramfs.cpio.gz || true

ENTRYPOINT ["/bin/sh", "-c", "\
  mkdir -p /out && \
  cp -r /rootfs-final /out/rootfs && \
  cp /out-initramfs.cpio.gz /out/ 2>/dev/null; \
  echo 'rootfs hazir: /out/rootfs , initramfs: /out/out-initramfs.cpio.gz'"]
