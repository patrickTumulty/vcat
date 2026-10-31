#!/bin/bash
#
# send_test_video.sh - Consolidated test video streamer
#
# Sends a video stream with ffmpeg to exercise vcat's video-source plugins.
#
#   Source:     built-in test pattern, or any input file
#   Codec:      h264 or h265
#   Transport:  UDP MPEG-TS, RTP, RTP MPEG-TS, or RTSP (local test server)
#
# Examples:
#   ./send_test_video.sh                            # test pattern, h265, UDP MPEG-TS -> 127.0.0.1:5000
#   ./send_test_video.sh --codec h264               # test pattern, h264, UDP MPEG-TS
#   ./send_test_video.sh --transport rtp-ts --port 5004  # test pattern, h265, RTP MPEG-TS
#   ./send_test_video.sh --transport rtsp           # local RTSP server for rtsp-video-source
#   ./send_test_video.sh --file movie.mp4           # file source instead of test pattern
#   ./send_test_video.sh --file movie.mp4 --no-loop # play once instead of looping

set -e

SOURCE=test
HOST=127.0.0.1
PORT=5000
PORT_SET=0
CODEC=h265
TRANSPORT=udp
RESOLUTION=1280x720
FPS=30
BITRATE=5000k
LOOP=1

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Send a video stream with ffmpeg, to exercise vcat's video-source plugins.

  Source:     built-in test pattern (default) or an input file
  Codec:      h265 (default) or h264
  Transport:  UDP MPEG-TS (default), RTP, RTP MPEG-TS, or RTSP

Options:
  -s, --source test|FILE   "test" for the built-in test pattern, or a path to a
                           media file (same as --file). (default: test)
  -f, --file FILE          Use FILE as the video source (overrides --source test).
  -c, --codec CODEC        h264 or h265 (default: h265)
  -t, --transport MUX      udp, rtp, rtp-ts, or rtsp (default: udp)
                             udp    MPEG-TS over UDP        -> udp-video-source
                             rtp    bare codec over RTP     -> rtp-video-source
                             rtp-ts MPEG-TS over RTP        -> rtp-video-source
                             rtsp   RTSP test pattern server-> rtsp-video-source
      --host HOST          Destination host / bind address (default: 127.0.0.1)
  -p, --port PORT          Destination port (default: 5000, rtsp: 8554)
      --resolution WxH     Test pattern resolution, e.g. 1920x1080, 1280x720,
                           512x512 (default: 1280x720). Files are unchanged.
      --fps N              Frame rate (default: 30)
      --bitrate RATE       Video bitrate (default: 5000k)
      --no-loop            Don't loop the input file (looping is the default)
  -h, --help               Show this help

Receiving:
  ffprobe "udp://127.0.0.1:5000?listen=1"
  For RTP transports, ffmpeg prints the SDP explaining the stream on startup;
  save it to stream.sdp and probe with:
    ffprobe -protocol_whitelist file,udp,rtp -i stream.sdp
  For RTSP, the script starts a local GstRTSPServer-based test server
  (tests/rtsp_test_server, not built by default; see serve_rtsp() below) and
  prints the vcat command to watch each mount.

Examples:
  $(basename "$0")
  $(basename "$0") --codec h264
  $(basename "$0") --transport rtp --port 5004
  $(basename "$0") --transport rtp-ts --port 5004
  $(basename "$0") --transport rtsp --port 8554
  $(basename "$0") --host 192.168.1.20 --port 5001
  $(basename "$0") --file video.mp4 --codec h264 --no-loop
  $(basename "$0") --file video.mp4
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -s|--source)  SOURCE="$2"; shift 2 ;;
        -f|--file)    SOURCE="$2"; shift 2 ;;
        -c|--codec)   CODEC="$2"; shift 2 ;;
        -t|--transport) TRANSPORT="$2"; shift 2 ;;
        --host)       HOST="$2"; shift 2 ;;
        -p|--port)    PORT="$2"; PORT_SET=1; shift 2 ;;
        --resolution) RESOLUTION="$2"; shift 2 ;;
        --fps)        FPS="$2"; shift 2 ;;
        --bitrate)    BITRATE="$2"; shift 2 ;;
        --no-loop)    LOOP=0; shift ;;
        -h|--help)    usage; exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

# ---- Validate ----------------------------------------------------------------

case "$CODEC" in
    h264) VENC=(libx264 -preset ultrafast -tune zerolatency) ;;
    h265) VENC=(libx265 -preset ultrafast -tune zerolatency) ;;
    *)
        echo "Error: codec must be h264 or h265 (got '$CODEC')" >&2
        exit 1
        ;;
esac

case "$TRANSPORT" in
    rtsp)
        # Served by tests/rtsp_test_server (see serve_rtsp below); the test
        # server always serves both mounts, --codec and --bitrate don't apply.
        ;;
    udp)
        MUX="mpegts"
        URL="udp://${HOST}:${PORT}?pkt_size=1316"
        ;;
    rtp)
        MUX="rtp"
        URL="rtp://${HOST}:${PORT}"
        ;;
    rtp-ts)
        MUX="rtp_mpegts"
        URL="rtp://${HOST}:${PORT}"
        ;;
    *)
        echo "Error: transport must be udp, rtp, rtp-ts, or rtsp (got '$TRANSPORT')" >&2
        exit 1
        ;;
esac

if [[ "$TRANSPORT" == "rtsp" && "$SOURCE" != "test" ]]; then
    echo "Error: --file/--source is not supported with --transport rtsp (the RTSP test server serves the test pattern)" >&2
    exit 1
fi

# ---- Build input -------------------------------------------------------------

if [[ "$SOURCE" == "test" ]]; then
    INPUT=(-re -f lavfi -i "testsrc2=size=${RESOLUTION}:rate=${FPS}")
    LABEL="test pattern ${RESOLUTION} @ ${FPS} fps"
else
    if [[ ! -f "$SOURCE" ]]; then
        echo "Error: file not found: $SOURCE" >&2
        exit 1
    fi
    INPUT=(-re -i "$SOURCE")
    if [[ $LOOP -eq 1 ]]; then
        INPUT=(-stream_loop -1 "${INPUT[@]}")
    fi
    LABEL="file ${SOURCE}"
fi

# ---- Go ----------------------------------------------------------------------

# Serve RTSP with tests/rtsp_test_server. Unlike the ffmpeg transports there is
# no sender process to pair: a small server (also built by this repo, excluded
# from the default build) runs in the background and vcat connects to it.
serve_rtsp() {
    local script_root
    script_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

    # Locate a configured CMake build tree: the repo default, or the caller's
    # own build directory.
    local build_dir=""
    local candidate
    for candidate in "$script_root/build" "$PWD/build"; do
        if [[ -f "$candidate/CMakeCache.txt" ]]; then
            build_dir="$candidate"
            break
        fi
    done

    if [[ -z "$build_dir" ]]; then
        echo "Error: no configured CMake build directory (expected <repo>/build)" >&2
        echo "Configure the project, then build the test server with:" >&2
        echo "  cmake --build build --target rtsp-test-server" >&2
        exit 1
    fi

    local server_bin="$build_dir/tests/rtsp-test-server"

    if [[ ! -x "$server_bin" ]]; then
        echo "RTSP test server not built yet - building it..."
        if ! cmake --build "$build_dir" --target rtsp-test-server; then
            echo "Error: could not build rtsp-test-server." >&2
            echo "Install libgstrtspserver-1.0-dev, re-run cmake, and try:" >&2
            echo "  cmake --build $build_dir --target rtsp-test-server" >&2
            exit 1
        fi
    fi

    echo "Starting RTSP test server..."
    # The child's output goes to a log file: its "listening on" line is the
    # readiness signal (printed only after a verified bind), and on failure
    # (e.g. the port is already taken) its error is shown to the user.
    local child_log
    child_log="$(mktemp -t rtsp-test-server.XXXXXX)"

    "$server_bin" --port "$PORT" --host "$HOST" --resolution "$RESOLUTION" --fps "$FPS" \
        > "$child_log" 2>&1 &
    local server_pid=$!

    trap 'kill "$server_pid" 2>/dev/null; wait "$server_pid" 2>/dev/null; rm -f "$child_log"' EXIT

    local ready=0
    local i
    for i in $(seq 1 50); do
        if grep -q "listening on" "$child_log"; then
            ready=1
            break
        fi

        if ! kill -0 "$server_pid" 2>/dev/null; then
            echo "Error: RTSP test server exited early (is port ${PORT} already in use?)" >&2
            cat "$child_log" >&2
            exit 1
        fi
        sleep 0.2
    done

    if (( ready == 0 )); then
        echo "Error: RTSP test server did not become ready on ${HOST}:${PORT}" >&2
        kill "$server_pid" 2>/dev/null
        exit 1
    fi

    cat "$child_log"
    echo "RTSP test server ready (PID ${server_pid}). Ctrl-c to stop it."
    wait "$server_pid" || true
}

if [[ "$TRANSPORT" == "rtsp" && "$PORT_SET" -eq 0 ]]; then
    PORT=8554
fi

if [[ "$TRANSPORT" == "rtsp" ]]; then
    serve_rtsp
    exit 0
fi

echo "Streaming: ${LABEL}"
echo "Codec:     ${CODEC} @ ${BITRATE}"
echo "Transport: ${TRANSPORT} -> ${URL}"
if [[ "$TRANSPORT" == "rtp" || "$TRANSPORT" == "rtp-ts" ]]; then
    echo "SDP (printed by ffmpeg below) describes the RTP stream."
fi

exec ffmpeg -hide_banner -loglevel info "${INPUT[@]}" \
    -c:v "${VENC[@]}" -b:v "${BITRATE}" -pix_fmt yuv420p -an \
    -f "${MUX}" "${URL}"
