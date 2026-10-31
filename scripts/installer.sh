#!/bin/bash

REPO="patricktumulty/vcat"
RELEASE_JSON=$(mktemp)

if ! command -v curl >/dev/null 2>&1; then
    echo "Error: curl is required but not installed."
    if command -v apt-get >/dev/null 2>&1; then
        echo "Install it with: sudo apt-get install curl"
    elif command -v dnf >/dev/null 2>&1; then
        echo "Install it with: sudo dnf install curl"
    elif command -v pacman >/dev/null 2>&1; then
        echo "Install it with: sudo pacman -S curl"
    else
        echo "See your distribution's package manager for installation instructions."
    fi
    exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "Error: jq is required but not installed."
    if command -v apt-get >/dev/null 2>&1; then
        echo "Install it with: sudo apt-get install jq"
    elif command -v dnf >/dev/null 2>&1; then
        echo "Install it with: sudo dnf install jq"
    elif command -v pacman >/dev/null 2>&1; then
        echo "Install it with: sudo pacman -S jq"
    else
        echo "See https://jqlang.github.io/jq/download/ for installation instructions."
    fi
    exit 1
fi

echoBold() {
    echo -e "\033[1m${1}\033[0m"
}

curl -fsSL "https://api.github.com/repos/patricktumulty/vcat/releases/latest" -o "$RELEASE_JSON"

VERSION=$(jq -r '.tag_name' "$RELEASE_JSON")
PUBLISHED=$(jq -r '.published_at' "$RELEASE_JSON")
RELEASE_LINK=$(jq -r '.html_url' "$RELEASE_JSON")

localInstall() {
    EXPECTED_ARTIFACT_PREFIX=UNKNOWN
    LOCAL_ARCH=$(uname -m)

    echo "Detected local machine architecture: ${LOCAL_ARCH}"

    case "$LOCAL_ARCH" in
        x86_64|amd64)
            EXPECTED_ARTIFACT_PREFIX=vcat-linux-x86_64
            ;;
        aarch64|arm64)
            EXPECTED_ARTIFACT_PREFIX=vcat-linux-arm64
            ;;
        *)
            echo "Error: Unsupported architecture '$ARCH'. Only x86_64 and arm64 are allowed." >&2
            exit 1
            ;;
    esac

    ARTIFACT_URL=$(jq -r ".assets[] | select(.name | startswith(\"${EXPECTER_ARTIFACT_PREFIX}\")) | .browser_download_url" "$RELEASE_JSON" | head -n 1)
    echo "Downloading ${ARTIFACT_URL}"
    curl -fL "${ARTIFACT_URL}" -o /tmp/vcat.tar.gz
    TMP_DIR=$(mktemp -d)
    pushd /tmp
    mkdir vcat
    tar -xf vcat.tar.gz -C ${TMP_DIR}
    find ${TMP_DIR} -type f -name "install.sh" -exec bash {} \;
    rm -rf ${TMP_DIR}
    rm vcat.tar.gz
    popd
}

remoteInstall() {
    echo "Remote install!!"
}

echo ""
echoBold "Welcome to vcat!"
echo ""
echo " Latest Version : v${VERSION}"
echo "      Published : $(date -d ${PUBLISHED})"
echo "    Release URL : ${RELEASE_LINK}"
echo ""
echoBold "Installation Options:"
echo ""
echo "  1) Install on this machine? (default)"
echo "  2) Install on a remote machine over SSH?"
echo ""

read -p "Enter install option (or ENTER for default): " INSTALL_OPTION 
if [ -z "$INSTALL_OPTION" ]; then
    INSTALL_OPTION=1
fi
case "$INSTALL_OPTION" in
    1)
        localInstall
        ;;
    2)
        remoteInstall
        ;;
    *)
        echo "Error: unrecognized install option '$INSTALL_OPTION'" >&2
        exit 1
        ;;
esac

rm ${RELEASE_JSON}



