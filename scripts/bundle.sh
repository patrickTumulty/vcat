#!/usr/bin/env bash

BUILD_DIR="$1"
ARCH=${2:-x86}
BIN_DIR=${BUILD_DIR}/bin
VERSION=$(sed 's/^[[:space:]]*//; s/[[:space:]]*$//' "${BIN_DIR}/VERSION")

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "Bundling vcat v$VERSION"

tree $BIN_DIR

mv $BUILD_DIR/bin "vcat-linux-$ARCH-$VERSION"
tar -czvf "vcat-linux-$ARCH-$VERSION.tar.gz" "vcat-linux-$ARCH-$VERSION"

echo "Done"
