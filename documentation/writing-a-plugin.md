# Writing a vcat Plugin

> This guide lives in vcat's tree — see it on GitHub at
> `documentation/writing-a-plugin.md`, or relative to the repository root when
> you pulled vcat in as a submodule.

A plugin is a shared library (`.so`) that `vcat` discovers and loads at startup. This
document walks through writing your own video source plugin.

**Suggested reading first:** [`libs/vcat-plugin/plugin-framework/vcat_plugin_framework.h`](../libs/vcat-plugin/plugin-framework/vcat_plugin_framework.h)
(the exported macros and callbacks) and
[`libs/vcat-plugin/plugin-api/vcat_plugin_api.h`](../libs/vcat-plugin/plugin-api/vcat_plugin_api.h)
(the full video source interface and its call order).

## Recommended setup: vcat as a submodule + the template plugin

The easiest way to develop a plugin is a small project of your own that pulls in vcat as a
git submodule and builds the template plugin out of the box:

```bash
mkdir my-plugin && cd my-plugin && git init
git submodule add https://github.com/patrickTumulty/vcat extern/vcat
git submodule update --init --recursive   # pulls vcat's own externs (zlog, crox, munit)
cp -r extern/vcat/plugins/template-video-source plugins/my-video-source
```

The build itself is driven by everything in vcat's CMake tree — your top-level
`CMakeLists.txt` just adds both to the build:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my-plugin LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Brings in the vcat tree: defines the vcat-plugin-framework target your plugin
# links against, and also builds the vcat binary itself so you can test right away.
add_subdirectory(extern/vcat)

# Your plugin. See the comments in plugins/my-video-source/CMakeLists.txt.
add_subdirectory(plugins/my-video-source)
```

Prerequisites: `cmake >= 3.20`, `pkg-config`, and the GStreamer 1.0 development packages
(`gstreamer-1.0`, `gstreamer-app-1.0`, `gstreamer-video-1.0`, `gstreamer-rtp-1.0`, e.g.
`libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev` on Debian/Ubuntu). The test source
also uses `videotestsrc` from `gstreamer1.0-plugins-good`.

Build it:

```bash
cmake -S . -B build
cmake --build build
```

This produces `build/plugins/<name>/lib<name>.so` **and** a fresh vcat binary under
`build/bin/vcat` that you can use to test immediately.

## Wiring up the template

The template (`plugins/template-video-source/`) is deliberately a skeleton — its callbacks
are stubs, so there is no real pipeline yet; the interesting work is yours. To make it
yours:

1. **Rename everything**: the CMake `project()`/target, the source file, and every
   "template" string.
2. **Pick your source token**: `VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("template")` —
   this is what users type on the command line (`vcat <source> [OPTIONS]`), and it is what
   the host matches against in `videoSourceProcessInputArgs()` (`argv[0]`). Everything
   before the first `://` must be unique across all loaded plugins, since it doubles as
   the URI scheme (e.g. `udp://1.2.3.4:5000`).
3. **Implement the five callbacks** (see the header docs in
   `vcat_plugin_framework.h` and the guided comments in the template source):

   | Callback                      | Purpose                                                            |
   |-------------------------------|--------------------------------------------------------------------|
   | `videoSourceProcessInputArgs` | Parse `<source> [OPTIONS]`; help/version, errors                    |
   | `videoSourceInitialize`       | Build your GStreamer bin (name it `VCAT_VIDEO_SOURCE_BIN_NAME`)     |
   | `videoSourceCleanup`          | Release your resources at shutdown                                  |
   | `videoSourceGetSourceElement` | The last element of your chain (the link point)                     |
   | `videoSourceGetSourceBin`     | Your complete pipeline fragment                                     |

   All five must be exported or the plugin is refused at load time.

For a complete working reference — a real `videotestsrc` pipeline — see
[`plugins/test-video-source/test_video_source.c`](../plugins/test-video-source/test_video_source.c).
The other in-tree plugins (`udp-video-source`, `rtsp-video-source`, ...) show URI parsing
and more realistic pipelines.

## Where the plugin goes at runtime

vcat does **not** enforce where your build tree places the plugin — organize your build
output however you like. What matters is copying the built `.so` into one of the plugin
directories vcat scans at startup:

```bash
cp build/plugins/my-video-source/libmy-video-source.so ~/.config/vcat/plugins/
vcat my-source --help
```

Use `~/.config/vcat/plugins` (recommended — it survives installs and upgrades) or the
`plugins` directory that ships next to the vcat binary.

> **Warning:** never put your plugins in the vcat install directory itself — the install
> script (`install.sh`) wipes the entire install directory (`rm -rf`) whenever vcat is
> installed or upgraded, so anything you place there will be deleted. `~/.config/vcat/plugins`
> survives reinstalls and upgrades.
