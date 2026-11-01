
#ifndef VCAT_PLUGIN_TYPES_H
#define VCAT_PLUGIN_TYPES_H

/**
 * @brief fixed element name of the bin a video source publishes.
 *
 * The host relies on the bin from videoSourceGetSourceBin() carrying this name, so a
 * source creates its top bin as gst_bin_new(VCAT_VIDEO_SOURCE_BIN_NAME).
 */
#define VCAT_VIDEO_SOURCE_BIN_NAME "videosrcbin"

/**
 * @brief the kind of interface a plugin implements.
 *
 * A plugin exports its value through the required symbol vcatPluginGetType()
 * (see VCAT_DEFINE_PLUGIN_TYPE); vcatPluginLoad() uses it to pick which of the
 * type-specific interfaces to resolve.
 */
typedef enum
{
    /** Unknown/no interface; a plugin must not report this. */
    PLUGIN_TYPE_NONE = 0,
    /** The plugin implements the video source interface in vcat_plugin_api.h. */
    PLUGIN_TYPE_VIDEO_SOURCE = 1
} PluginType;

/**
 * @brief result of a video source's command line processing.
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

/**
 * @brief export the plugin's display name required by every plugin.
 *
 * Defines the symbol vcatPluginGetName(); the plugin manager uses it in log
 * lines and to report load failures.
 */
#define VCAT_DEFINE_PLUGIN_NAME(NAME) \
    const char *vcatPluginGetName()   \
    {                                 \
        return (NAME);                \
    }

/**
 * @brief export the plugin's type -- a required symbol in every plugin.
 *
 * Defines the symbol vcatPluginGetType() (see PluginType); without it the
 * plugin cannot state which interface it implements and fails to load.
 */
#define VCAT_DEFINE_PLUGIN_TYPE(TYPE) \
    PluginType vcatPluginGetType()    \
    {                                 \
        return (TYPE);                \
    }

/**
 * @brief export the plugin's version string.
 *
 * Optional: the host does not query it directly; sources print it themselves
 * (e.g. in "--version" handling).
 */
#define VCAT_DEFINE_PLUGIN_VERSION(VERSION) \
    const char *vcatPluginGetVersion()      \
    {                                       \
        return (VERSION);                   \
    }

/**
 * @brief define the source's URI help string, shown in the main help under "Sources:".
 *
 * Used by the host to match "<source>" tokens on the command line: the token
 * either matches the URI scheme prefix (e.g. "udp" for "udp://<host>:<port>")
 * or is a full URI on that scheme (e.g. "udp://127.0.0.1:1234"). The string up
 * to the first "://" (or the whole string for sources without one) must
 * therefore name the source uniquely.
 */
#define VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP(URI_HELP) \
    const char *videoSourceGetUriHelp()                    \
    {                                                      \
        return (URI_HELP);                                 \
    }

/**
 * @brief define the source's one-line description, shown next to the URI help in the main help.
 */
#define VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION(DESCRIPTION) \
    const char *videoSourceGetDescription()                      \
    {                                                            \
        return (DESCRIPTION);                                    \
    }

/**
 * @brief fail the calling function (return RC_ERROR) when a GST element is NULL.
 *
 * Logs "<MSG>: GST Element <E> is NULL" with the element expression spelled out,
 * then returns RC_ERROR from the calling function. Use it right after creating
 * pipeline elements to exit early on failure with a pointed message.
 */
#define VERIFY_ELEMENT(E, MSG)                               \
    if (!(E))                                                \
    {                                                        \
        lfLogError("%s: GST Element %s is NULL", (MSG), #E); \
        return RC_ERROR;                                     \
    }

#endif // VCAT_PLUGIN_TYPES_H
