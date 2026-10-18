#!/usr/bin/env bash
set -euo pipefail

SRC="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" && pwd -P)"

DEST="${VCAT_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/vcat}"
BINDIR="${VCAT_BIN_DIR:-$HOME/.local/bin}"

if [ "${SRC}" = "${DEST}" ]; then
    echo "error: refusing to install the bundle onto itself (${DEST})" >&2
    exit 1
fi

for f in vcat lib plugins uninstall.sh; do
    if [ ! -e "${SRC}/${f}" ]; then
        echo "error: ${SRC} does not look like a vcat bundle (missing ${f})" >&2
        exit 1
    fi
done

echo "Installing vcat to ${DEST}"

rm -rf "${DEST}"
mkdir -p "${DEST}"

rsync -av --exclude='install.sh' "${SRC}/" "${DEST}/"

# Logs live in the XDG state dir, outside the install tree, so they survive
# uninstall. The marker tells the launcher to export VCAT_LOG_DIR.
LOGDIR="${XDG_STATE_HOME:-$HOME/.local/state}/vcat"
mkdir -p "${LOGDIR}/logs"
touch "${DEST}/.installed"

mkdir -p "${BINDIR}"
ln -sfn "${DEST}/vcat" "${BINDIR}/vcat"
echo "linked ${BINDIR}/vcat -> ${DEST}/vcat"

case ":${PATH}:" in
    *":${BINDIR}:"*) ;;
    *) echo "note: ${BINDIR} is not in your PATH; add this to your shell profile:"
       echo "  export PATH=\"${BINDIR}:\$PATH\"" ;;
esac

echo "done. run 'vcat' (or ${DEST}/vcat) to get started."
echo "logs:     ${LOGDIR}/logs/"
echo "to uninstall: cd ${DEST} && ./uninstall.sh"
