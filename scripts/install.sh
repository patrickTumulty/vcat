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

echo "Installing vcat..."

rm -rf "${VCAT_INSTALL_DIR}"
mkdir -p "${VCAT_INSTALL_DIR}"
cp -r ${SCRIPT_DIR}/* "${VCAT_INSTALL_DIR}/" 
rm "${VCAT_INSTALL_DIR}/install.sh"

echo "Creating log dir..."

mkdir -p "${VCAT_LOG_DIR}/logs"
touch "${VCAT_INSTALL_DIR}/.installed"

echo "Creating vcat symlink..."

mkdir -p "${LOCAL_BIN_DIR}"
ln -sfn "${VCAT_INSTALL_DIR}/vcat" "${LOCAL_BIN_DIR}/vcat"

echo "linked ${LOCAL_BIN_DIR}/vcat -> ${VCAT_INSTALL_DIR}/vcat"

RESET="\033[0m"
BOLD="\033[1m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"

case ":${PATH}:" in
    *":${LOCAL_BIN_DIR}:"*) ;;
    *) 
        echo ""
        echo -e "${YELLOW}${BOLD}NOTE:${RESET} ${CYAN}${LOCAL_BIN_DIR}${RESET} is not in your PATH; add this to your shell profile:"
        echo "  export PATH=\"${LOCAL_BIN_DIR}:\$PATH\"" 
        echo ""
       ;;
esac

echo '                                  /$$    '
echo '⠀⠀⠀⠀⠀                            | $$    '
echo '⠀⠀/$$    /$$ /$$$$$$$  /$$$$$$  /$$$$$$  '
echo ' |  $$  /$$//$$_____/ |____  $$|_  $$_/  '
echo ' ⠀\  $$/$$/| $$        /$$$$$$$  | $$    '
echo '⠀⠀⠀\  $$$/ | $$       /$$__  $$  | $$ /$$'
echo ' ⠀⠀ \  $/  |  $$$$$$$|  $$$$$$$  |  $$$$/'
echo '     \_/    \_______/ \_______/   \___/  '
echo ""
echo "Successfully installed vcat!"
echo ""
echo "run 'vcat --help' to get started."
echo ""
echo "  Installed Version : v${VCAT_VERSION}"
echo "           Logs Dir : ${VCAT_LOG_DIR}/logs/"
echo "        Install Dir : ${VCAT_INSTALL_DIR}"
echo "   Uninstall Script : cd ${VCAT_INSTALL_DIR} && ./uninstall.sh"
echo ""
