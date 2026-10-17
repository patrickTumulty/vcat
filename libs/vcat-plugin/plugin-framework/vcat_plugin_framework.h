
#ifndef VCAT_PLUGIN_TYPES_H
#define VCAT_PLUGIN_TYPES_H

typedef enum
{
    PLUGIN_TYPE_NONE = 0,
    PLUGIN_TYPE_VIDEO_SOURCE = 1
} PluginType;

/**
 * @brief result of a video source's command line processing
 */
typedef enum
{
    /** Args were processed; initialization may proceed. */
    ARG_PARSE_RESULT_SUCCESS = 0,
    /** Args were processed and the host should exit immediately (e.g. help was printed). */
    ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY = 1,
    /** Args were invalid; the host should exit with an error. The source has printed its own message. */
    ARG_PARSE_RESULT_ERROR = 2,
} InputArgsResult;

#define VCAT_DEFINE_PLUGIN_NAME(NAME) \
    const char *vcatPluginGetName()   \
    {                                 \
        return (NAME);                \
    }

#define VCAT_DEFINE_PLUGIN_TYPE(TYPE) \
    PluginType vcatPluginGetType()    \
    {                                 \
        return (TYPE);                \
    }

#define VCAT_DEFINE_PLUGIN_VERSION(VERSION) \
    const char *vcatPluginGetVersion()      \
    {                                       \
        return (VERSION);                   \
    }

/**
 * Defines the source's URI help string, shown in the main help under "Sources:",
 * and used by the host to match "<source>" tokens on the command line. The token
 * either matches the URI scheme prefix (e.g. "udp" for "udp://<host>:<port>") or
 * is a full URI on that scheme (e.g. "udp://127.0.0.1:1234").
 */
#define VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP(URI_HELP) \
    const char *videoSourceGetUriHelp()                    \
    {                                                      \
        return (URI_HELP);                                 \
    }

#define VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION(DESCRIPTION) \
    const char *videoSourceGetDescription()                      \
    {                                                            \
        return (DESCRIPTION);                                    \
    }

#define VERIFY_ELEMENT(E, MSG)                               \
    if (!(E))                                                \
    {                                                        \
        lfLogError("%s: GST Element %s is NULL", (MSG), #E); \
        return RC_ERROR;                                     \
    }

#endif // VCAT_PLUGIN_TYPES_H
