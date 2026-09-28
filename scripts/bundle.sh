#!/usr/bin/env bash
#
# bundle.sh - assemble a self-contained vcat bundle.
#
# The bundle is a directory that can be copied as-is onto a target machine:
#
#   <prefix>/bin/vcat
#   <prefix>/lib/*.so*                    dependencies (NEEDED closure)
#   <prefix>/lib/gstreamer-1.0/*.so       plugins used by vcat's pipeline
#   <prefix>/vcat                         launcher setting LD_LIBRARY_PATH etc.
#   <prefix>/install.sh                   install to ~/.local/share/vcat (+PATH link)
#   <prefix>/uninstall.sh                 remove an installation
#   <prefix>/VERSION                      build identifier
#
# Base system libraries (the dynamic loader, libc, libm, libstdc++, libgcc)
# are intentionally left to the target OS.
#
# Usage: bundle.sh ARCH [INSTALL_PREFIX] [BUILD_DIR]
#   ARCH           native | arm64         (default: native)
#   INSTALL_PREFIX destination directory  (default: dist/)
#   BUILD_DIR      directory holding the vcat binary (default: build/)
#
# Examples:
#   scripts/bundle.sh native
#   scripts/bundle.sh arm64 dist/vcat-arm64 build/arm64
set -euo pipefail

ARCH=${1:-native}
PREFIX=${2:-dist}
BUILD_DIR=${3:-build}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

case "${ARCH}" in
    native) TRIPLET="x86_64-linux-gnu" ;;
    arm64)  TRIPLET="aarch64-linux-gnu" ;;
    *) echo "error: unknown ARCH '${ARCH}' (expected 'native' or 'arm64')" >&2; exit 1 ;;
esac

case "${BUILD_DIR}" in
    /*) BUILD="${BUILD_DIR}" ;;
    *)  BUILD="${ROOT}/${BUILD_DIR}" ;;
esac
BIN="${BUILD}/vcat"
if [ ! -f "${BIN}" ]; then
    echo "error: ${BIN} not found; run 'make build ARCH=${ARCH}' first" >&2
    exit 1
fi

# Directories searched for shared libraries (in order).
add_if_present() {
    local -n target="$1"
    local dir="$2" existing_dir
    [ -d "${dir}" ] || return 0
    for existing_dir in "${target[@]}"; do
        [ "${existing_dir}" = "${dir}" ] && return 0
    done
    target+=("${dir}")
}
add_if_present SEARCH_DIRS "/lib/${TRIPLET}"
add_if_present SEARCH_DIRS "/usr/lib/${TRIPLET}"
[ "${ARCH}" = "arm64" ] && add_if_present SEARCH_DIRS "/opt/gst/arm64/lib"

# The arm64 source build's plugins come first for arm64; otherwise the
# architecture's apt plugin directory is used.
if [ "${ARCH}" = "arm64" ]; then
    add_if_present PLUGIN_DIRS "/opt/gst/arm64/lib/gstreamer-1.0"
fi
add_if_present PLUGIN_DIRS "/usr/lib/${TRIPLET}/gstreamer-1.0"

# Libraries the target OS provides; never bundled.
SKIP_GLOBS=( 'ld-linux-*.so*' 'libc.so.6' 'libm.so.6' 'libstdc++.so.6' 'libgcc_s.so.1' )

is_skipped() {
    local name="$1" glob
    for glob in "${SKIP_GLOBS[@]}"; do
        if [[ "${name}" == ${glob} ]]; then
            return 0
        fi
    done
    return 1
}

# Plugin modules behind the elements vcat instantiates:
#   udpsrc, tsdemux, h265parse, nvh265dec, videoconvert, capsfilter, appsink
CURATED=( coreelements app videoconvertscale udp mpegtsdemux videoparsersbad nvcodec )

needed_names() {
    readelf -d "$1" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\].*/\1/p'
}

resolve() {
    local soname="$1" dir
    for dir in "${SEARCH_DIRS[@]}"; do
        if [ -f "${dir}/${soname}" ]; then
            printf '%s\n' "${dir}/${soname}"
            return 0
        fi
    done
    return 1
}

LIBDIR="${PREFIX}/lib"
declare -A COPIED

# copy_deps <elf-file> - copy the transitive NEEDED closure of <elf-file>
# into ${LIBDIR} (recursion is cut off by COPIED).
copy_deps() {
    local file="$1" name depfile
    while read -r name; do
        [ -n "${name}" ] || continue
        is_skipped "${name}" && continue
        [ -n "${COPIED[${name}]+_}" ] && continue
        COPIED[${name}]=1
        if depfile="$(resolve "${name}")"; then
            cp -L "${depfile}" "${LIBDIR}/${name}"
            copy_deps "${LIBDIR}/${name}"
        else
            echo "warning: could not resolve '${name}' (needed by ${file})" >&2
        fi
    done < <(needed_names "${file}")
}

rm -rf "${PREFIX}"
mkdir -p "${PREFIX}/bin" "${LIBDIR}/gstreamer-1.0"

cp "${BIN}" "${PREFIX}/bin/vcat"
copy_deps "${BIN}"

for plugin in "${CURATED[@]}"; do
    src=""
    for dir in "${PLUGIN_DIRS[@]}"; do
        candidate="${dir}/libgst${plugin}.so"
        if [ -f "${candidate}" ]; then
            src="${candidate}"
            break
        fi
    done
    if [ -z "${src}" ]; then
        echo "warning: plugin 'libgst${plugin}.so' not found for arch '${ARCH}'" >&2
        continue
    fi
    cp -L "${src}" "${LIBDIR}/gstreamer-1.0/libgst${plugin}.so"
    copy_deps "${LIBDIR}/gstreamer-1.0/libgst${plugin}.so"
done

cp "${ROOT}/scripts/run.sh" "${PREFIX}/vcat"
cp "${ROOT}/scripts/install.sh" "${ROOT}/scripts/uninstall.sh" "${PREFIX}/"
chmod +x "${PREFIX}/vcat" "${PREFIX}/install.sh" "${PREFIX}/uninstall.sh"

VERSION="$(git -C "${ROOT}" describe --always --dirty 2>/dev/null || date -u +%Y%m%dT%H%M%SZ)"
printf '%s\n' "${VERSION}" > "${PREFIX}/VERSION"

echo "bundle: ${PREFIX}"
echo "  binary:     $(file -b "${PREFIX}/bin/vcat" | cut -d, -f1-2)"
echo "  libraries:  $(find "${LIBDIR}" -maxdepth 1 -name '*.so*' | wc -l)"
echo "  plugins:    $(find "${LIBDIR}/gstreamer-1.0" -maxdepth 1 -name '*.so' | wc -l)"
