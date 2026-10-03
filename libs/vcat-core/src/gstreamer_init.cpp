
#include "gstreamer_init.hpp"
#include "logging.hpp"
#include <format>
#include <gst/gst.h>

namespace
{

logging::Level toLoggingLevel(GstDebugLevel level)
{
    switch (level)
    {
    case GST_LEVEL_ERROR:
        return logging::Level::ERROR;
    case GST_LEVEL_WARNING:
    case GST_LEVEL_FIXME:
        return logging::Level::WARN;
    case GST_LEVEL_INFO:
        return logging::Level::INFO;
    case GST_LEVEL_DEBUG:
        return logging::Level::DEBUG;
    case GST_LEVEL_NONE:
    case GST_LEVEL_LOG:
    case GST_LEVEL_TRACE:
    case GST_LEVEL_MEMDUMP:
    default:
        return logging::Level::TRACE;
    }
}

void gstLogToLogging(GstDebugCategory *category, GstDebugLevel level, const gchar *file, const gchar *function,
                     gint line, GObject *object, GstDebugMessage *message, gpointer userData)
{
    (void)file;
    (void)function;
    (void)line;
    (void)object;
    (void)userData;

    const gchar *text = gst_debug_message_get(message);
    const gchar *name = category ? gst_debug_category_get_name(category) : "unknown";

    logging::logWrite(toLoggingLevel(level),                       //
                      std::format("[{}] {}",                       //
                                  name == nullptr ? "NULL" : name, //
                                  text == nullptr ? "NULL" : text));
}

} // namespace

void initGStreamer()
{
    logging::info("Initializing gstreamer");

    gst_init(nullptr, nullptr);

    // Only warnings and errors are interesting, the rest drowns out the app's own log lines.
    gst_debug_set_default_threshold(GST_LEVEL_WARNING);

    gst_debug_remove_log_function(gst_debug_log_default);
    gst_debug_add_log_function(gstLogToLogging, nullptr, nullptr);
}
