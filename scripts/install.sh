#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"

VCAT_INSTALL_DIR="${VCAT_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/vcat}"
LOCAL_BIN_DIR="${VCAT_BIN_DIR:-$HOME/.local/bin}"
# Logs live in the XDG state dir, outside the install tree, so they survive
# uninstall. The marker tells the launcher to export VCAT_LOG_DIR.
VCAT_LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/vcat"
VCAT_VERSION=$(tr -d '[:space:]' < ${SCRIPT_DIR}/VERSION)

if [ "${SCRIPT_DIR}" = "${VCAT_INSTALL_DIR}" ]; then
    echo "error: refusing to install the bundle onto itself (${VCAT_INSTALL_DIR})" >&2
    exit 1
fi

for f in vcat plugins uninstall.sh; do
    if [ ! -e "${SCRIPT_DIR}/${f}" ]; then
        echo "error: ${SCRIPT_DIR} does not look like a vcat bundle (missing ${f})" >&2
        exit 1
    fi
done

echo "Installing vcat to ${VCAT_INSTALL_DIR}"

rm -rf "${VCAT_INSTALL_DIR}"
mkdir -p "${VCAT_INSTALL_DIR}"
cp -r ${SCRIPT_DIR}/* "${VCAT_INSTALL_DIR}/" 
rm "${VCAT_INSTALL_DIR}/install.sh"

echo "Creating log dir"

mkdir -p "${VCAT_LOG_DIR}/logs"
touch "${VCAT_INSTALL_DIR}/.installed"

echo "Creating vcat symlink"

mkdir -p "${LOCAL_BIN_DIR}"
ln -sfn "${VCAT_INSTALL_DIR}/vcat" "${LOCAL_BIN_DIR}/vcat"

echo "linked ${LOCAL_BIN_DIR}/vcat -> ${VCAT_INSTALL_DIR}/vcat"

case ":${PATH}:" in
    *":${LOCAL_BIN_DIR}:"*) ;;
    *) echo "note: ${LOCAL_BIN_DIR} is not in your PATH; add this to your shell profile:"
       echo "  export PATH=\"${LOCAL_BIN_DIR}:\$PATH\"" ;;
esac


echo ""
echo '                                  /$$    '
echo '⠀⠀⠀⠀⠀                            | $$    '
echo '⠀⠀/$$    /$$ /$$$$$$$  /$$$$$$  /$$$$$$  '
echo ' |  $$  /$$//$$_____/ |____  $$|_  $$_/  '
echo ' ⠀\  $$/$$/| $$        /$$$$$$$  | $$    '
echo '⠀⠀⠀\  $$$/ | $$       /$$__  $$  | $$ /$$'
echo ' ⠀⠀ \  $/  |  $$$$$$$|  $$$$$$$  |  $$$$/'
echo '     \_/    \_______/ \_______/   \___/  '
echo ""
echo "               v${VCAT_VERSION}"
echo ""
echo "run 'vcat --help' to get started."
echo "logs.......... : ${VCAT_LOG_DIR}/logs/"
echo "to uninstall.. : cd ${VCAT_INSTALL_DIR} && ./uninstall.sh"
