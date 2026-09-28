#!/usr/bin/env bash
#
# uninstall.sh - remove a vcat installation.
#
# Run from inside the install directory (default ~/.local/share/vcat):
#   cd ~/.local/share/vcat && ./uninstall.sh
#
# Logs (~/.local/state/vcat) survive by default; pass --purge to remove them
# too. It also works from an extracted (not yet installed) bundle directory,
# where it just cleans up any matching launcher symlink and cache.
set -euo pipefail

PURGE=0
for arg in "$@"; do
    case "${arg}" in
        --purge) PURGE=1 ;;
        *) echo "usage: uninstall.sh [--purge]" >&2; exit 2 ;;
    esac
done

DIR="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" && pwd -P)"
BINDIR="${VCAT_BIN_DIR:-$HOME/.local/bin}"
LOGDIR="${XDG_STATE_HOME:-$HOME/.local/state}/vcat"

if [ -L "${BINDIR}/vcat" ] && [ "$(readlink -f -- "${BINDIR}/vcat")" = "${DIR}/vcat" ]; then
    rm -f "${BINDIR}/vcat"
    echo "removed ${BINDIR}/vcat"
fi

CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/vcat"
if [ -d "${CACHE}" ]; then
    rm -rf "${CACHE}"
    echo "removed ${CACHE}"
fi

if [ "${PURGE}" = "1" ]; then
    LOGROOT="${VCAT_LOG_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/vcat}"
    if [ -d "${LOGROOT}" ]; then
        rm -rf "${LOGROOT}"
        echo "removed ${LOGROOT} (logs purged)"
    fi
else
    LOGROOT="${VCAT_LOG_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/vcat}"
    if [ -d "${LOGROOT}" ]; then
        echo "kept logs in ${LOGROOT}/log (use --purge to remove them)"
    fi
fi

if [ ! -e "${DIR}/bin/vcat" ]; then
    echo "note: ${DIR} is not an installed vcat directory; nothing else to remove."
    exit 0
fi

# Unlinking a running script is safe on Linux (bash reads via the open fd),
# so removing the tree that contains this script works.
echo "removing ${DIR}"
rm -rf "${DIR}"
echo "vcat uninstalled."
