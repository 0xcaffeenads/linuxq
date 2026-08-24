#!/usr/bin/env bash
# Machine hash'ini zincirdeki DApp kontratina yazar.
# PRIVATE_KEY bu scripte HER ZAMAN environment variable olarak gelir,
# asla CLI argumani / dosya olarak gecirilmez (process listesinde gorunmesin diye).
set -euo pipefail

: "${PRIVATE_KEY:?PRIVATE_KEY env degiskeni set edilmeli (GitHub Secrets'tan gelir)}"
: "${RPC_URL:?RPC_URL env degiskeni set edilmeli}"
: "${CONTRACT_ADDRESS:?CONTRACT_ADDRESS env degiskeni set edilmeli}"

HASH_FILE="${1:-out/machine-hash.txt}"
if [ ! -f "$HASH_FILE" ]; then
    echo "Hata: $HASH_FILE bulunamadi" >&2
    exit 1
fi

MACHINE_HASH="$(tail -n1 "$HASH_FILE" | tr -d '[:space:]')"
echo ">> Zincire yazilacak hash: $MACHINE_HASH"
echo ">> Kontrat: $CONTRACT_ADDRESS  |  RPC: ${RPC_URL%%\?*}"

# foundry (cast) ile ornek cagri - kendi kontrat ABI'nize gore fonksiyon adini
# ve parametreleri degistirin.
cast send "$CONTRACT_ADDRESS" \
    "setMachineHash(bytes32)" \
    "$MACHINE_HASH" \
    --rpc-url "$RPC_URL" \
    --private-key "$PRIVATE_KEY"

echo ">> Deploy tamamlandi."
