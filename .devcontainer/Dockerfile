# vcat development container (Ubuntu 24.04).
#
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

USER ubuntu
