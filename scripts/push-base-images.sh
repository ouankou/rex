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
LLVM_VERSION="${LLVM_VERSION:-22}"
if [[ "${LLVM_VERSION}" != "22" ]]; then
  echo "Error: REX base images are pinned to LLVM/Clang major 22; requested '${LLVM_VERSION}'." >&2
  exit 2
fi

BUILDER="${BUILDER:-rex-multi}"
if ! docker buildx inspect "${BUILDER}" >/dev/null 2>&1; then
  docker buildx create --name "${BUILDER}" --driver docker-container --use
else
  docker buildx use "${BUILDER}"
fi

docker buildx inspect --bootstrap >/dev/null

ARCHES=(amd64 arm64 riscv64 loong64)
declare -A PLATFORMS=(
  [amd64]=linux/amd64
  [arm64]=linux/arm64
  [riscv64]=linux/riscv64
  [loong64]=linux/loong64
)
declare -A DOCKERFILES=(
  [amd64]=ci/docker/rex-amd64.Dockerfile
  [arm64]=ci/docker/rex-arm64.Dockerfile
  [riscv64]=ci/docker/rex-riscv64.Dockerfile
  [loong64]=ci/docker/rex-loongarch64.Dockerfile
)
declare -A DIGESTS=()
METAS=()

cleanup() {
  rm -f "${METAS[@]}"
}
trap cleanup EXIT

for arch in "${ARCHES[@]}"; do
  meta="$(mktemp)"
  METAS+=("${meta}")

  docker buildx build \
    --platform "${PLATFORMS[$arch]}" \
    -f "${DOCKERFILES[$arch]}" \
    --build-arg LLVM_VERSION="${LLVM_VERSION}" \
    --output=type=registry,name="${IMAGE}",push-by-digest=true \
    --provenance=false \
    --metadata-file "${meta}" \
    .

  DIGESTS["${arch}"]="$(python3 - <<'PY' "${meta}"
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    data = json.load(handle)
print(data["containerimage.digest"])
PY
)"
done

refs=()
for arch in "${ARCHES[@]}"; do
  refs+=("${IMAGE}@${DIGESTS[$arch]}")
done

docker buildx imagetools create -t "${IMAGE}:${TAG}" "${refs[@]}"

echo "Pushed multi-arch image: ${IMAGE}:${TAG}"
for arch in "${ARCHES[@]}"; do
  echo "  ${arch}: ${DIGESTS[$arch]}"
done
