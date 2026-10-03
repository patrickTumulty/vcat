#!/bin/bash

set -e

PORT=1234
FILE=""

usage() {
    echo "Usage: $0 [-p PORT] FILE"
    echo
    echo "Options:"
    echo "  -p PORT    UDP port (default: 1234)"
    echo "  -h         Show this help"
    echo
    echo "Example:"
    echo "  $0 -p 5000 video.mp4"
}

while getopts ":p:h" opt; do
    case "$opt" in
        p)
            PORT="$OPTARG"
            ;;
        h)
            usage
            exit 0
            ;;
        \?)
            echo "Unknown option: -$OPTARG" >&2
            usage >&2
            exit 1
            ;;
        :)
            echo "Option -$OPTARG requires an argument." >&2
            usage >&2
            exit 1
            ;;
    esac
done

shift $((OPTIND - 1))

if [[ $# -ne 1 ]]; then
    echo "Error: exactly one input file is required." >&2
    usage >&2
    exit 1
fi

FILE="$1"

if [[ ! -f "$FILE" ]]; then
    echo "Error: file not found: $FILE" >&2
    exit 1
fi

echo "Streaming: $FILE"
echo "UDP port: $PORT"

ffmpeg -re \
    -stream_loop -1 \
    -i "$FILE" \
    -c copy \
    -bsf:v h264_mp4toannexb \
    -f mpegts \
    "udp://127.0.0.1:${PORT}?pkt_size=1316"

