#!/usr/bin/env bash
# rootfs -> ext2 image -> cartesi-machine ile hash hesapla
set -euo pipefail

ROOTFS_DIR="${1:-out/rootfs}"
IMAGE_OUT="${2:-out/rootfs.ext2}"
IMAGE_SIZE_MB="${IMAGE_SIZE_MB:-64}"

if [ ! -d "$ROOTFS_DIR" ]; then
    echo "Hata: $ROOTFS_DIR bulunamadi. Once Docker build adimini calistirin." >&2
    exit 1
fi

echo ">> ext2 image olusturuluyor (${IMAGE_SIZE_MB}MB): $IMAGE_OUT"
truncate -s "${IMAGE_SIZE_MB}M" "$IMAGE_OUT"
mkfs.ext2 -F -d "$ROOTFS_DIR" "$IMAGE_OUT" >/dev/null

echo ">> Cartesi machine hash hesaplaniyor"
# cartesi-machine CLI, resmi Cartesi kernel image'i ile birlikte kurulu olmali
# (bkz: https://github.com/cartesi/machine-emulator)
if command -v cartesi-machine >/dev/null 2>&1; then
    cartesi-machine \
        --flash-drive="label:root,filename:${IMAGE_OUT}" \
        --final-hash \
        -- /bin/true | tee out/machine-hash.txt
else
    echo "Uyari: cartesi-machine CLI bulunamadi, hash hesaplama atlandi." >&2
fi

echo ">> Bitti. Image: $IMAGE_OUT"
