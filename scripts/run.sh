#!/usr/bin/env bash
set -e

DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

export GST_PLUGIN_PATH="$DIR/lib/gstreamer-1.0"
export GST_PLUGIN_SYSTEM_PATH=""

CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/vcat"
mkdir -p "$CACHE"
export GST_REGISTRY_1_0="$CACHE/gstreamer-registry.bin"

exec "$DIR/bin/vcat" "$@"

