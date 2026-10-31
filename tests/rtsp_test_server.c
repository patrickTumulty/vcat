/**
 * @file rtsp_test_server.c
 *
 * @brief Tiny local RTSP server for exercising vcat's rtsp-video-source plugin
 * without a third-party media server.
 *
 * Serves GStreamer test patterns at fixed mount points:
 *   /test  H264 (rtsp-video-source default path)
 *   /h265  H265
 *
 * This is a test-only helper, not part of the shipped application: the target
 * is EXCLUDE_FROM_ALL and lands in <build>/tests (never <build>/bin). Build it
 * explicitly with:
 *   cmake --build <build-dir> --target rtsp-test-server
 *
 * scripts/send_test_video.sh --transport rtsp launches it automatically.
 */

#include <gst/rtsp-server/rtsp-server.h>

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RTSP_TEST_SERVER_DEFAULT_PORT 8554
#define RTSP_TEST_SERVER_DEFAULT_RESOLUTION "1280x720"
#define RTSP_TEST_SERVER_DEFAULT_FPS 30

/**
 * Encoders able to serve a live test pattern, in preference order. The launch
 * string appends props verbatim after the element name; availability of the
 * element decides which entry wins.
 */
static const char *kEncoderH264[] = {
    "x264enc tune=zerolatency key-int-max=30 speed-preset=ultrafast",
    "nvh264enc",
    "openh264enc",
};

static const char *kEncoderH265[] = {
    "x265enc tune=zerolatency key-int-max=30 speed-preset=ultrafast",
    "nvh265enc",
};

static void rtspTestServerPrintHelp()
{
    printf("Usage: rtsp-test-server [OPTIONS]\n"
           "\n"
           "Serve GStreamer test patterns over RTSP for testing\n"
           "vcat's rtsp-video-source plugin. Mount points:\n"
           "  /test   H264 test pattern\n"
           "  /h265   H265 test pattern\n"
           "\n"
           "Options:\n"
           "  -h, --help             Show this help\n"
           "  -p, --port PORT        TCP port to listen on (default 8554)\n"
           "      --host ADDR        Address to bind (default: all interfaces)\n"
           "      --resolution WxH   Test pattern resolution (default 1280x720)\n"
           "      --fps N            Test pattern frame rate (default 30)\n");
}

/**
 * @brief look up the first encoder candidate whose GStreamer element exists
 *
 * @param[in] candidates NULL-terminated launch fragments ("element props...")
 *
 * @return the first available candidate, or NULL when none is installed
 */
static const char *rtspTestServerPickEncoder(const char *const *candidates)
{
    for (size_t i = 0; candidates[i] != NULL; i++)
    {
        char element[64] = "";
        if (sscanf(candidates[i], " %63s", element) != 1)
        {
            continue;
        }

        if (gst_element_factory_find(element) != NULL)
        {
            return candidates[i];
        }
    }

    return NULL;
}

/**
 * @brief register one mount point serving <encoder> at <path>
 *
 * @return true when the factory was registered
 */
static bool rtspTestServerAddMount(GstRTSPMountPoints *mounts,
                                   const char *path,
                                   const char *encoder,
                                   const char *payload,
                                   unsigned int width,
                                   unsigned int height,
                                   unsigned int fps)
{
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    gchar *launch = g_strdup_printf("( videotestsrc is-live=true ! "
                                    "video/x-raw,format=I420,width=%u,height=%u,framerate=%u/1 ! "
                                    "%s ! %s config-interval=1 name=pay0 pt=96 )",
                                    width, height, fps, encoder, payload);

    gst_rtsp_media_factory_set_launch(factory, launch);
    // Share one GStreamer pipeline between all clients: the test server only
    // ever feeds one vcat instance at a time, and sharing keeps startup fast.
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    gst_rtsp_mount_points_add_factory(mounts, path, factory);

    g_free(launch);
    return true;
}

/**
 * @brief parse a "<width>x<height>" resolution string
 */
static bool rtspTestServerParseResolution(const char *spec, unsigned int *outWidth, unsigned int *outHeight)
{
    const char *x = strchr(spec, 'x');

    if (x == NULL || x == spec)
    {
        return false;
    }

    char widthText[16] = "";
    size_t widthLen = (size_t)(x - spec);

    if (widthLen >= sizeof(widthText) || x[1] == '\0')
    {
        return false;
    }

    memcpy(widthText, spec, widthLen);

    char *endPtr;
    long width = strtol(widthText, &endPtr, 10);
    if (widthText[0] == '\0' || *endPtr != '\0' || width <= 0 || width > 8192)
    {
        return false;
    }

    long height = strtol(x + 1, &endPtr, 10);
    if (endPtr == x + 1 || *endPtr != '\0' || height <= 0 || height > 8192)
    {
        return false;
    }

    *outWidth = (unsigned int)width;
    *outHeight = (unsigned int)height;
    return true;
}

int main(int argc, char *argv[])
{
    unsigned int port = RTSP_TEST_SERVER_DEFAULT_PORT;
    const char *host = NULL;
    const char *resolution = RTSP_TEST_SERVER_DEFAULT_RESOLUTION;
    unsigned long fps = RTSP_TEST_SERVER_DEFAULT_FPS;

    gst_init(&argc, &argv);

    // Keep startup messages visible even when stdout is redirected (a pipe or
    // file gives printf full buffering otherwise).
    setvbuf(stdout, NULL, _IOLBF, 0);

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
        {
            rtspTestServerPrintHelp();
            return 0;
        }
        else if (strcmp(arg, "-p") == 0 || strcmp(arg, "--port") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "Error: %s expects a value\n", arg);
                return 1;
            }

            char *endPtr;
            unsigned long parsed = strtoul(argv[i], &endPtr, 10);

            if (endPtr == argv[i] || *endPtr != '\0' || parsed == 0 || parsed > USHRT_MAX)
            {
                fprintf(stderr, "Error: invalid port '%s'\n", argv[i]);
                return 1;
            }

            port = (unsigned int)parsed;
        }
        else if (strcmp(arg, "--host") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "Error: %s expects a value\n", arg);
                return 1;
            }

            host = argv[i];
        }
        else if (strcmp(arg, "--resolution") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "Error: %s expects a value\n", arg);
                return 1;
            }

            resolution = argv[i];
        }
        else if (strcmp(arg, "--fps") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "Error: %s expects a value\n", arg);
                return 1;
            }

            char *endPtr;
            fps = strtoul(argv[i], &endPtr, 10);

            if (endPtr == argv[i] || *endPtr != '\0' || fps == 0 || fps > 240)
            {
                fprintf(stderr, "Error: invalid fps '%s'\n", argv[i]);
                return 1;
            }
        }
        else
        {
            fprintf(stderr, "Error: unknown option '%s'\n", arg);
            rtspTestServerPrintHelp();
            return 1;
        }
    }

    unsigned int width = 0;
    unsigned int height = 0;

    if (rtspTestServerParseResolution(resolution, &width, &height) == false)
    {
        fprintf(stderr, "Error: invalid resolution '%s' (expected WxH, e.g. 1280x720)\n", resolution);
        return 1;
    }

    const char *encoderH264 = rtspTestServerPickEncoder(kEncoderH264);
    const char *encoderH265 = rtspTestServerPickEncoder(kEncoderH265);

    if (encoderH264 == NULL && encoderH265 == NULL)
    {
        fprintf(stderr, "Error: no H264 or H265 encoder found "
                        "(x264enc/x265enc or NVENC plugins required)\n");
        return 1;
    }

    GstRTSPServer *server = gst_rtsp_server_new();

    if (host != NULL)
    {
        gst_rtsp_server_set_address(server, host);
    }

    gchar *service = g_strdup_printf("%u", port);
    gst_rtsp_server_set_service(server, service);
    g_free(service);

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

    if (encoderH264 != NULL &&
        rtspTestServerAddMount(mounts, "/test", encoderH264, "rtph264pay", width, height, (unsigned int)fps))
    {
        printf("  H264 test pattern:  rtsp://<host>:%u/test   (encoder: %s)\n", port, encoderH264);
    }

    if (encoderH265 != NULL &&
        rtspTestServerAddMount(mounts, "/h265", encoderH265, "rtph265pay", width, height, (unsigned int)fps))
    {
        printf("  H265 test pattern:  rtsp://<host>:%u/h265   (encoder: %s)\n", port, encoderH265);
    }

    g_object_unref(mounts);

    if (gst_rtsp_server_attach(server, NULL) == 0)
    {
        fprintf(stderr, "Error: failed to attach the RTSP server\n");
        gst_object_unref(server);
        return 1;
    }

    if (gst_rtsp_server_get_bound_port(server) <= 0)
    {
        fprintf(stderr, "Error: failed to bind port %u\n", port);
        gst_object_unref(server);
        return 1;
    }

    printf("RTSP test server listening on %s:%u (ctrl-c to stop)\n", host != NULL ? host : "(all interfaces)", port);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    gst_object_unref(server);
    gst_deinit();
    return 0;
}
