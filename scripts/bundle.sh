#!/usr/bin/env bash

ARCH=${1:-x86}
INSTALL_PREFIX=${2}
BUILD_DIR=${3}
BIN_DIR=${BUILD_DIR}/bin
VERSION=$(sed 's/^[[:space:]]*//; s/[[:space:]]*$//' "${BIN_DIR}/VERSION")

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "Bundling vcat v${VERSION}"
echo "== ${BIN_DIR} =="
tree ${BIN_DIR}
echo "================"

mkdir -p "${INSTALL_PREFIX}-${VERSION}"
mv "${BUILD_DIR}/bin/*" "${INSTALL_PREFIX}-${VERSION}"
tar -czvf "${INSTALL_PREFIX}-${VERSION}.tar.gz" "${INSTALL_PREFIX}-${VERSION}"

echo "Done"
