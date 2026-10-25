
# vcat development container (Ubuntu 24.04).
#
# x86_64 GStreamer (tools, runtime plugins, dev libraries) is installed from
# apt exactly as on a normal desktop; gst-inspect/gst-discoverer and the full
# plugin set work out of the box there. The :arm64 plugin packages
# (plugins-good/bad/libav) provide the aarch64 plugins that scripts/bundle.sh
# copies into arm64 bundles for elements outside gst-plugins-base.
#
# The arm64 side is cross-built from GStreamer source with Meson and installed
# to /opt/gst/arm64, separate from the apt (amd64) install. Ubuntu's arm64 dev
# packages cannot be co-installed with their amd64 counterparts, so building
# them from source avoids that apt problem while providing the libraries
# needed to link vcat for aarch64.

FROM ubuntu:24.04

# Bump this version whenever the Dockerfile changes. CI uses it to tag the
# image in GHCR (devcontainer:v<VERSION>) and devcontainer.json pins that
# tag, so bumping it is what gets the new image to developer machines.
LABEL org.opencontainers.image.version="1.0.0"

ARG GSTREAMER_VERSION=1.24.13

USER root

# Configure multiarch repositories. Foreign architectures are served by
# ports.ubuntu.com, so keep amd64 and arm64 repositories architecture-pinned.
RUN dpkg --add-architecture arm64 \
    && rm -f /etc/apt/sources.list.d/ubuntu.sources \
    && cat > /etc/apt/sources.list.d/ubuntu-amd64.sources <<'EOF'
Types: deb
URIs: http://archive.ubuntu.com/ubuntu
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Architectures: amd64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://security.ubuntu.com/ubuntu
Suites: noble-security
Components: main universe restricted multiverse
Architectures: amd64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF

RUN cat > /etc/apt/sources.list.d/ubuntu-arm64.sources <<'EOF'
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports
Suites: noble noble-updates noble-backports noble-security
Components: main universe
Architectures: arm64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF

# Install build tools, GStreamer, debugging tools, networking utilities,
# and general development dependencies.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        crossbuild-essential-arm64 \
        ca-certificates \
        cmake \
        ninja-build \
        meson \
        bison \
        flex \
        gettext \
        python3 \
        pkg-config \
        gdb \
        git \
        git-lfs \
        clang \
        valgrind \
        gstreamer1.0-tools \
        gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-good:arm64 \
        gstreamer1.0-plugins-bad \
        gstreamer1.0-plugins-bad:arm64 \
        gstreamer1.0-plugins-ugly \
        gstreamer1.0-plugins-base-apps \
        gstreamer1.0-libav \
        gstreamer1.0-libav:arm64 \
        libgstreamer1.0-dev \
        libgstreamer-plugins-base1.0-dev \
        libglib2.0-dev \
        libglib2.0-dev:arm64 \
        libncurses-dev \
        libncurses-dev:arm64 \
        libva-dev \
        libvdpau-dev \
        nvidia-cuda-toolkit-gcc \
        iputils-ping \
        iproute2 \
        dnsutils \
        tcpdump \
        tshark \
        ethtool \
        lsof \
        strace \
        socat \
        netcat-openbsd \
        ffmpeg \
        curl \
        mtr \
        net-tools \
        tree \
        jq \
        less \
        file \
        unzip \
        zip \
        openssh-server \
        sudo \
    && rm -rf /var/lib/apt/lists/* \
    && echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu

# Build the arm64 GStreamer libraries from source.
#
# libglib2.0-dev:arm64 depends on python3:any. The native x86 Python
# installation anchors that dependency to amd64, avoiding the need to
# execute arm64 package post-installation scripts on the x86 host.
#
# Build GStreamer core and gst-plugins-base from the monorepo using the
# aarch64 cross-compiler. Install the resulting libraries under /opt/gst/arm64
# so they remain separate from the native apt installation.
RUN git clone --depth 1 \
        --branch "${GSTREAMER_VERSION}" \
        https://gitlab.freedesktop.org/gstreamer/gstreamer.git \
        /opt/src/gst \
    && cat > /opt/src/gst/cross-arm64.txt <<'EOF'
[binaries]
c = '/usr/bin/aarch64-linux-gnu-gcc'
cpp = '/usr/bin/aarch64-linux-gnu-g++'
ar = '/usr/bin/aarch64-linux-gnu-ar'
strip = '/usr/bin/aarch64-linux-gnu-strip'
pkg-config = '/usr/bin/aarch64-linux-gnu-pkg-config'

[host_machine]
system = 'linux'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
EOF

RUN cd /opt/src/gst/subprojects/gstreamer \
    && PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig \
       meson setup _build \
           --cross-file /opt/src/gst/cross-arm64.txt \
           --prefix /opt/gst/arm64 \
           --libdir lib \
           -Dtools=disabled \
           -Dbenchmarks=disabled \
           -Dexamples=disabled \
           -Dtests=disabled \
           -Ddoc=disabled \
           -Dintrospection=disabled \
    && meson install -C _build \
    && cd /opt/src/gst/subprojects/gst-plugins-base \
    && PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig \
       PKG_CONFIG_PATH=/opt/gst/arm64/lib/pkgconfig \
       meson setup _build \
           --cross-file /opt/src/gst/cross-arm64.txt \
           --prefix /opt/gst/arm64 \
           --libdir lib \
           -Dtools=disabled \
           -Dexamples=disabled \
           -Dtests=disabled \
           -Dintrospection=disabled \
           -Dorc=disabled \
           -Dgl=disabled \
           -Dx11=disabled \
           -Ddrm=disabled \
    && meson install -C _build \
    && rm -rf /opt/src/gst

# Configure the development user.
RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu

USER ubuntu
