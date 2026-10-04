# vcat development container (Ubuntu 24.04).
#
# x86_64 GStreamer (tools, runtime plugins, dev libraries) is installed from
# apt exactly as on a normal desktop; gst-inspect/gst-discoverer and the full
# plugin set work out of the box there. The :arm64 plugin packages
# (plugins-good/bad/libav) provide the aarch64 plugins that scripts/bundle.sh
# copies into arm64 bundles for elements outside gst-plugins-base.
#
# The arm64 side is cross-built from GStreamer source with Meson and installed
# to /opt/gst/arm64, separate from the apt (amd64) install. The Ubuntu arm64
# dev packages (libgstreamer-plugins-base1.0-dev etc.) cannot be co-installed
# with their amd64 counterparts (not Multi-Arch: same), so building them from
# source avoids that whole apt problem while still giving vcat the
# libgstreamer-1.0 / libgstapp-1.0 / libgstvideo-1.0 libraries needed to link
# for aarch64.
FROM ubuntu:24.04

USER root

# Enable the arm64 architecture. Foreign architectures are only served by
# ports.ubuntu.com, so each source line is pinned to its own architecture:
# amd64 for archive/security, arm64 for ports. Without the pin, apt tries to
# fetch binary-arm64 from archive/security, which return 404.
RUN dpkg --add-architecture arm64 \
    && rm -f /etc/apt/sources.list.d/ubuntu.sources \
    && printf '%s\n' \
        'deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ noble main universe restricted multiverse' \
        'deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ noble-updates main universe restricted multiverse' \
        'deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ noble-backports main universe restricted multiverse' \
        'deb [arch=amd64] http://security.ubuntu.com/ubuntu/ noble-security main universe restricted multiverse' \
        > /etc/apt/sources.list.d/ubuntu.list \
    && printf '%s\n' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ noble main universe' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ noble-updates main universe' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ noble-backports main universe' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ noble-security main universe' \
        > /etc/apt/sources.list.d/arm64.list \
    && apt-get update \
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

# Build the arm64 GStreamer libraries from source so vcat can be cross
# compiled. libglib2.0-dev:arm64 depends on python3:any; the x86 python3
# installed in the apt step above anchors that dependency to amd64, so apt
# never pulls python3:arm64 (whose postinst cannot run on the x86 host).
#
# We build the gstreamer core and gst-plugins-base modules from the monorepo
# at a release tag, with the aarch64 toolchain, and install the shared
# libraries into /opt/gst/arm64.
RUN git clone --depth 1 --branch 1.24.13 https://gitlab.freedesktop.org/gstreamer/gstreamer.git /opt/src/gst \
    && printf '%s\n' \
        '[binaries]' \
        "c = '/usr/bin/aarch64-linux-gnu-gcc'" \
        "cpp = '/usr/bin/aarch64-linux-gnu-g++'" \
        "ar = '/usr/bin/aarch64-linux-gnu-ar'" \
        "strip = '/usr/bin/aarch64-linux-gnu-strip'" \
        "pkg-config = '/usr/bin/aarch64-linux-gnu-pkg-config'" \
        '[host_machine]' \
        "system = 'linux'" \
        "cpu_family = 'aarch64'" \
        "cpu = 'aarch64'" \
        "endian = 'little'" \
        > /opt/src/gst/cross-arm64.txt \
    && cd /opt/src/gst/subprojects/gstreamer \
    && PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig \
        meson setup _build --cross-file /opt/src/gst/cross-arm64.txt \
            --prefix /opt/gst/arm64 --libdir lib \
            -Dtools=disabled -Dbenchmarks=disabled -Dexamples=disabled \
            -Dtests=disabled -Ddoc=disabled -Dintrospection=disabled \
    && meson install -C _build \
    && cd /opt/src/gst/subprojects/gst-plugins-base \
    && PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig \
        PKG_CONFIG_PATH=/opt/gst/arm64/lib/pkgconfig \
        meson setup _build --cross-file /opt/src/gst/cross-arm64.txt \
            --prefix /opt/gst/arm64 --libdir lib \
            -Dtools=disabled -Dexamples=disabled -Dtests=disabled \
            -Dintrospection=disabled -Dorc=disabled -Dgl=disabled \
            -Dx11=disabled -Ddrm=disabled \
    && meson install -C _build \
    && rm -rf /opt/src/gst

USER ubuntu
