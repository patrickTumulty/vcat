#!/bin/bash
#
# send_test_video.sh - Consolidated test video streamer
#
# Sends a video stream with ffmpeg to exercise vcat's video-source plugins.
#
#   Source:     built-in test pattern, or any input file
#   Codec:      h264 or h265
#   Transport:  UDP MPEG-TS, RTP, or RTP MPEG-TS
#
# Examples:
#   ./send_test_video.sh                            # test pattern, h265, UDP MPEG-TS -> 127.0.0.1:5000
#   ./send_test_video.sh --codec h264               # test pattern, h264, UDP MPEG-TS
#   ./send_test_video.sh --transport rtp-ts --port 5004  # test pattern, h265, RTP MPEG-TS
#   ./send_test_video.sh --file movie.mp4           # file source instead of test pattern
#   ./send_test_video.sh --file movie.mp4 --no-loop # play once instead of looping

set -e

SOURCE=test
HOST=127.0.0.1
PORT=5000
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
  Transport:  UDP MPEG-TS (default), RTP, or RTP MPEG-TS

Options:
  -s, --source test|FILE   "test" for the built-in test pattern, or a path to a
                           media file (same as --file). (default: test)
  -f, --file FILE          Use FILE as the video source (overrides --source test).
  -c, --codec CODEC        h264 or h265 (default: h265)
  -t, --transport MUX      udp, rtp, or rtp-ts (default: udp)
                             udp    MPEG-TS over UDP        -> udp-video-source
                             rtp    bare codec over RTP     -> rtp-video-source
                             rtp-ts MPEG-TS over RTP        -> rtp-video-source
      --host HOST          Destination host (default: 127.0.0.1)
  -p, --port PORT          Destination port (default: 5000)
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

Examples:
  $(basename "$0")
  $(basename "$0") --codec h264
  $(basename "$0") --transport rtp --port 5004
  $(basename "$0") --transport rtp-ts --port 5004
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
        -p|--port)    PORT="$2"; shift 2 ;;
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
        echo "Error: transport must be udp, rtp, or rtp-ts (got '$TRANSPORT')" >&2
        exit 1
        ;;
esac

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

echo "Streaming: ${LABEL}"
echo "Codec:     ${CODEC} @ ${BITRATE}"
echo "Transport: ${TRANSPORT} -> ${URL}"
if [[ "$TRANSPORT" == "rtp" || "$TRANSPORT" == "rtp-ts" ]]; then
    echo "SDP (printed by ffmpeg below) describes the RTP stream."
fi

exec ffmpeg -hide_banner -loglevel info "${INPUT[@]}" \
    -c:v "${VENC[@]}" -b:v "${BITRATE}" -pix_fmt yuv420p -an \
    -f "${MUX}" "${URL}"
