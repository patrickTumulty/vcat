#!/usr/bin/env bash
set -e

# Resolve symlinks (e.g. ~/.local/bin/vcat -> the install dir) so the bundle
# is found relative to the real location of this launcher.
DIR="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" && pwd -P)"

# Installed bundles log to the XDG state dir (marked at install time). Dev
# builds leave VCAT_LOG_DIR unset so the log lands in the working directory.
if [ -z "${VCAT_LOG_DIR:-}" ] && [ -f "$DIR/.installed" ]; then
    export VCAT_LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/vcat"
fi

export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

export GST_PLUGIN_PATH="$DIR/lib/gstreamer-1.0"
export GST_PLUGIN_SYSTEM_PATH=""

CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/vcat"
mkdir -p "$CACHE"
export GST_REGISTRY_1_0="$CACHE/gstreamer-registry.bin"

exec "$DIR/bin/vcat" "$@"

