
#include "gstreamer_init.h"
#include "logging.h"
#include "logging_facade.h"
#include <gst/gst.h>

/**
 * @brief Maps a GStreamer debug level onto the facade's levels.
 *
 * The facade starts at debug, so GStreamer's chatty trace-level categories
 * share the debug threshold.
 */
static LfLogLevel toFacadeLevel(GstDebugLevel level)
{
    switch (level)
    {
    case GST_LEVEL_ERROR:
        return LF_LEVEL_ERROR;
    case GST_LEVEL_WARNING:
    case GST_LEVEL_FIXME:
        return LF_LEVEL_WARN;
    case GST_LEVEL_INFO:
        return LF_LEVEL_INFO;
    case GST_LEVEL_DEBUG:
        return LF_LEVEL_DEBUG;
    case GST_LEVEL_NONE:
    case GST_LEVEL_LOG:
    case GST_LEVEL_TRACE:
    case GST_LEVEL_MEMDUMP:
    default:
        return LF_LEVEL_DEBUG;
    }
}

static void gstLogToFacade(GstDebugCategory *category,
                           GstDebugLevel level,
                           const gchar *file,
                           const gchar *function,
                           gint line,
                           GObject *object,
                           GstDebugMessage *message,
                           gpointer userData)
{
    (void)file;
    (void)function;
    (void)line;
    (void)object;
    (void)userData;

    const gchar *text = gst_debug_message_get(message);
    const gchar *name = category ? gst_debug_category_get_name(category) : "unknown";

    lfLog(toFacadeLevel(level), "gstreamer", (int)sizeof("gstreamer") - 1, 0, "[%s] %s", name == NULL ? "NULL" : name,
          text == NULL ? "NULL" : text);
}

void vcatInitGStreamer(void)
{
    lfLogInfo("Initializing gstreamer");

    gst_init(NULL, NULL);

    // Only warnings and errors are interesting, the rest drowns out the app's own log lines.
    gst_debug_set_default_threshold(GST_LEVEL_WARNING);

    gst_debug_remove_log_function(gst_debug_log_default);
    gst_debug_add_log_function(gstLogToFacade, NULL, NULL);
}
