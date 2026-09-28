#!/usr/bin/env bash
#
# install.sh - install a vcat bundle to a per-user location.
#
# Run from inside an extracted bundle directory:
#   tar xzf vcat-linux-arm64.tar.gz && cd vcat-linux-arm64 && ./install.sh
#
# The bundle is copied to ${VCAT_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/vcat}
# and a launcher symlink is created at ${VCAT_BIN_DIR:-$HOME/.local/bin}/vcat.
# Remove everything later with ./uninstall.sh from the install directory.
set -euo pipefail

SRC="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" && pwd -P)"

DEST="${VCAT_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/vcat}"
BINDIR="${VCAT_BIN_DIR:-$HOME/.local/bin}"

if [ "${SRC}" = "${DEST}" ]; then
    echo "error: refusing to install the bundle onto itself (${DEST})" >&2
    exit 1
fi
for f in bin/vcat lib vcat uninstall.sh; do
    if [ ! -e "${SRC}/${f}" ]; then
        echo "error: ${SRC} does not look like a vcat bundle (missing ${f})" >&2
        exit 1
    fi
done

echo "installing vcat to ${DEST}"
rm -rf "${DEST}"
mkdir -p "${DEST}"
cp -a "${SRC}/bin" "${SRC}/lib" "${SRC}/vcat" "${SRC}/uninstall.sh" "${DEST}/"
[ -f "${SRC}/VERSION" ] && cp -a "${SRC}/VERSION" "${DEST}/VERSION"

# Logs live in the XDG state dir, outside the install tree, so they survive
# uninstall. The marker tells the launcher to export VCAT_LOG_DIR.
LOGDIR="${XDG_STATE_HOME:-$HOME/.local/state}/vcat"
mkdir -p "${LOGDIR}/log"
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
echo "logs:     ${LOGDIR}/log/vcat.log"
echo "to uninstall: cd ${DEST} && ./uninstall.sh"
