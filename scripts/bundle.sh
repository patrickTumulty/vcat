#!/usr/bin/env bash

set -e

ARCH=${1:-x86_64}
BUILD_DIR=${2:-build}
BIN_DIR=${BUILD_DIR}/bin
VERSION=$(tr -d '[:space:]' < "${BIN_DIR}/VERSION")

BUNDLE_DIR_PATH="artifacts/vcat-linux-${ARCH}-${VERSION}"

rm -rf "${BUNDLE_DIR_PATH}"
mkdir -p "${BUNDLE_DIR_PATH}"
mv "${BUILD_DIR}"/bin/* "${BUNDLE_DIR_PATH}"

echo "Bundled vcat files - v${VERSION}"
echo "== ${BUNDLE_DIR_PATH} =="
tree "${BUNDLE_DIR_PATH}"
echo "================"

tar -czvf "${BUNDLE_DIR_PATH}.tar.gz" -C "$(dirname "${BUNDLE_DIR_PATH}")" "$(basename "${BUNDLE_DIR_PATH}")"

echo "Done"
