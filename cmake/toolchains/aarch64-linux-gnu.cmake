# Cross-compile toolchain for aarch64 on x86_64 using Ubuntu's multiarch
# packages (installed via dpkg --add-architecture arm64). Arm64 headers/libs
# live in the standard multiarch layout on the host (/usr/include for
# arch-independent headers, /usr/lib/aarch64-linux-gnu for libraries and
# pkg-config files), so no separate sysroot is used.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Multiarch lib dir for the target. CMake does not infer CMAKE_LIBRARY_ARCHITECTURE
# for this cross setup, so find_library would miss /usr/lib/aarch64-linux-gnu and
# fail to find plain libraries (e.g. Curses via find_package).
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)

set(CMAKE_C_COMPILER   /usr/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)

set(CMAKE_AR      /usr/bin/aarch64-linux-gnu-ar)
set(CMAKE_RANLIB  /usr/bin/aarch64-linux-gnu-ranlib)
set(CMAKE_STRIP   /usr/bin/aarch64-linux-gnu-strip)
set(CMAKE_OBJCOPY /usr/bin/aarch64-linux-gnu-objcopy)
set(CMAKE_OBJDUMP /usr/bin/aarch64-linux-gnu-objdump)

# Force cross-compilation (host is also Linux) and point pkg-config at the
# aarch64 .pc files so x86 host packages are never picked up. The arm64
# GStreamer built by the devcontainer lives in /opt/gst/arm64 (queried first);
# the Ubuntu aarch64 glib comes from /usr/lib/aarch64-linux-gnu. PKG_CONFIG_PATH
# is cleared so amd64 .pc directories can't leak into the arm64 resolution.
set(CMAKE_CROSSCOMPILING TRUE)
set(ENV{PKG_CONFIG_LIBDIR} "/opt/gst/arm64/lib/pkgconfig:/usr/lib/aarch64-linux-gnu/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "")

# Don't try to execute ARM64 binaries during configure.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)