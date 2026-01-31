#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 <ghcr_owner> [image_name] [tag]" >&2
  echo "Example: $0 ouankou rex base" >&2
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

OWNER="$1"
IMAGE_NAME="${2:-rex}"
TAG="${3:-base}"
IMAGE="ghcr.io/${OWNER}/${IMAGE_NAME}"

BUILDER="${BUILDER:-rex-multi}"
if ! docker buildx inspect "${BUILDER}" >/dev/null 2>&1; then
  docker buildx create --name "${BUILDER}" --driver docker-container --use
else
  docker buildx use "${BUILDER}"
fi

docker buildx inspect --bootstrap >/dev/null

LOONG_META="$(mktemp)"
RISCV_META="$(mktemp)"
cleanup() {
  rm -f "${LOONG_META}" "${RISCV_META}"
}
trap cleanup EXIT

docker buildx build \
  --platform linux/loong64 \
  -f ci/docker/rex-loongarch64.Dockerfile \
  --output=type=registry,name="${IMAGE}",push-by-digest=true \
  --provenance=false \
  --metadata-file "${LOONG_META}" \
  .

docker buildx build \
  --platform linux/riscv64 \
  -f ci/docker/rex-riscv64.Dockerfile \
  --output=type=registry,name="${IMAGE}",push-by-digest=true \
  --provenance=false \
  --metadata-file "${RISCV_META}" \
  .

LOONG_DIGEST="$(python3 - <<'PY' "${LOONG_META}"
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    data = json.load(handle)
print(data["containerimage.digest"])
PY
)"

RISCV_DIGEST="$(python3 - <<'PY' "${RISCV_META}"
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    data = json.load(handle)
print(data["containerimage.digest"])
PY
)"

docker buildx imagetools create -t "${IMAGE}:${TAG}" \
  "${IMAGE}@${LOONG_DIGEST}" \
  "${IMAGE}@${RISCV_DIGEST}"

echo "Pushed multi-arch image: ${IMAGE}:${TAG}"
echo "  loong64: ${LOONG_DIGEST}"
echo "  riscv64: ${RISCV_DIGEST}"
