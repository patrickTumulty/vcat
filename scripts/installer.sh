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

resolveArtifactPrefix() {
    case "$1" in
        x86_64|amd64)
            echo "vcat-linux-x86_64"
            ;;
        aarch64|arm64)
            echo "vcat-linux-arm64"
            ;;
        *)
            echo "Error: Unsupported architecture '$1'. Only x86_64 and arm64 are allowed." >&2
            exit 1
            ;;
    esac
}

resolveArtifactURL() {
    ARTIFACT_URL=$(jq -r ".assets[] | select(.name | startswith(\"${EXPECTED_ARTIFACT_PREFIX}\")) | .browser_download_url" "$RELEASE_JSON" | head -n 1)

    if [ -z "$ARTIFACT_URL" ]; then
        echo "Error: no release artifact found for '${EXPECTED_ARTIFACT_PREFIX}' (v${VERSION})." >&2
        exit 1
    fi
}

localInstall() {
    LOCAL_ARCH=$(uname -m)

    echo "Detected local machine architecture: ${LOCAL_ARCH}"

    EXPECTED_ARTIFACT_PREFIX=$(resolveArtifactPrefix "${LOCAL_ARCH}")
    resolveArtifactURL
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
    echo ""
    echoBold "Remote install selected."
    echo "(Assuming you already have SSH access to the remote machine.)"
    echo ""

    read -p "Remote host (user@hostname): " REMOTE_HOST
    if [ -z "${REMOTE_HOST}" ]; then
        echo "Error: remote host is required." >&2
        exit 1
    fi

    read -p "SSH port (or ENTER for port 22): " REMOTE_PORT
    REMOTE_PORT=${REMOTE_PORT:-22}

    SSH_OPTS=(-p "${REMOTE_PORT}" -o "ConnectTimeout=10")

    echo "Connecting to ${REMOTE_HOST}..."
    if ! REMOTE_ARCH=$(ssh "${SSH_OPTS[@]}" "${REMOTE_HOST}" 'uname -m'); then
        echo "Error: could not reach '${REMOTE_HOST}' over SSH on port ${REMOTE_PORT}." >&2
        exit 1
    fi

    echo "Detected remote machine architecture: ${REMOTE_ARCH}"

    EXPECTED_ARTIFACT_PREFIX=$(resolveArtifactPrefix "${REMOTE_ARCH}")
    resolveArtifactURL

    echo "Downloading ${ARTIFACT_URL}"
    LOCAL_TAR=$(mktemp)
    trap 'rm -f "${LOCAL_TAR}"' EXIT
    if ! curl -fL "${ARTIFACT_URL}" -o "${LOCAL_TAR}"; then
        echo "Error: download failed." >&2
        exit 1
    fi

    echo "Creating temp dir on ${REMOTE_HOST}..."
    if ! REMOTE_TMP=$(ssh "${SSH_OPTS[@]}" "${REMOTE_HOST}" 'mktemp -d'); then
        echo "Error: could not create temp dir on ${REMOTE_HOST}." >&2
        exit 1
    fi

    echo "Uploading bundle to ${REMOTE_HOST}..."
    if ! scp -P "${REMOTE_PORT}" "${LOCAL_TAR}" "${REMOTE_HOST}:${REMOTE_TMP}/vcat.tar.gz"; then
        echo "Error: scp failed." >&2
        rm -f "${LOCAL_TAR}"
        ssh "${SSH_OPTS[@]}" "${REMOTE_HOST}" "rm -rf '${REMOTE_TMP}'"
        exit 1
    fi
    rm -f "${LOCAL_TAR}"

    echo "Untarring and running installer on ${REMOTE_HOST}..."
    if ! ssh "${SSH_OPTS[@]}" "${REMOTE_HOST}" \
        "tar -xf '${REMOTE_TMP}/vcat.tar.gz' -C '${REMOTE_TMP}' \
        && find '${REMOTE_TMP}' -type f -name 'install.sh' -exec bash {} \\;"; then
        echo "Error: installation failed on ${REMOTE_HOST}." >&2
        ssh "${SSH_OPTS[@]}" "${REMOTE_HOST}" "rm -rf '${REMOTE_TMP}'"
        exit 1
    fi

    echo "Cleaning up..."
    ssh "${SSH_OPTS[@]}" "${REMOTE_HOST}" "rm -rf '${REMOTE_TMP}'"

    echo ""
    echo "Remote install complete on ${REMOTE_HOST} (v${VERSION})."
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

INSTALL_OPTION=1

read -p "Enter install option (or ENTER for default): " INSTALL_OPTION </dev/tty

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



