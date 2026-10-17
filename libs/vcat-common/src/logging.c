
#include "logging.h"
#include "logging_facade.h"
#include "zlog.h"
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define VCAT_LOG_CONF "vcat.zlog.conf"

static bool g_initialized = false;
static zlog_category_t *g_category = NULL;

/**
 * @brief Resolves the path of the log file's directory from the environment.
 *
 * Installed bundles export VCAT_LOG_DIR (to ~/.local/state/vcat) from the launcher, so their
 * logs land in $VCAT_LOG_DIR/log/vcat.log and survive reinstalls. Development builds leave the
 * variable unset; the configuration's %E(VCAT_LOG_DIR) would then expand to nothing, so this
 * defaults the variable to "." and keeps a working-directory relative log. Directory creation
 * is best effort: zlog reports real failures of its own.
 */
static void resolveLogDir(void)
{
    const char *logDir = getenv("VCAT_LOG_DIR");
    if (logDir != NULL && *logDir != '\0')
    {
        return;
    }

    (void)setenv("VCAT_LOG_DIR", ".", 1);

    char dir[PATH_MAX];
    (void)snprintf(dir, sizeof(dir), "%s/log", ".");
    (void)mkdir(dir, 0755);
}

/**
 * @brief Finds the shipped configuration file.
 *
 * Looks next to the executable first (where the bundle keeps it), then in the
 * working directory. Returning NULL hands the decision to zlog, which applies
 * its own ZLOG_CONF_PATH fallback.
 *
 * @return A configuration path, or NULL when none can be located.
 */
static const char *resolveConfPath(void)
{
    static char confPath[PATH_MAX];

    char exe[PATH_MAX];
    const ssize_t exeLen = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (exeLen > 0)
    {
        exe[exeLen] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash != NULL)
        {
            (void)snprintf(confPath, sizeof(confPath), "%.*s/%s", (int)(slash - exe), exe, VCAT_LOG_CONF);
            if (access(confPath, R_OK) == 0)
            {
                return confPath;
            }
        }
    }

    if (access(VCAT_LOG_CONF, R_OK) == 0)
    {
        return VCAT_LOG_CONF;
    }

    return NULL;
}

/**
 * @brief The host sink every log flows through.
 *
 * Called for the host's own lfLog* calls (registered below) and, through the
 * plugin manager, for every plugin's facade. Formats the message, decorates it
 * with the source location and hands it to zlog.
 *
 * Called from plugins too, potentially on GStreamer streaming threads. `args`
 * is owned by the caller: read it (vsnprintf) but never va_end it.
 */
void vcatHostLog(LfLogLevel level, const char *file, int fileLen, int line, const char *msg, va_list args)
{
    if (!g_initialized)
    {
        return;
    }

    char text[1024];
    (void)vsnprintf(text, sizeof(text), msg, args);

    // Keep the last 20 characters of the file name, as before: the interesting
    // part of a path is the file, not the directories above it.
    char filePart[21] = "?";
    if (file != NULL && fileLen > 0)
    {
        const int keep = fileLen > 20 ? 20 : fileLen;
        memcpy(filePart, file + fileLen - keep, keep);
        filePart[keep] = '\0';
    }

    char framed[1100];
    (void)snprintf(framed, sizeof(framed), "[%s:%d] %s", filePart, line, text);

    int zlogLevel = ZLOG_LEVEL_INFO;
    switch (level)
    {
    case LF_LEVEL_DEBUG:
        zlogLevel = ZLOG_LEVEL_DEBUG;
        break;
    case LF_LEVEL_INFO:
    case LF_LEVEL_NOTICE:
        zlogLevel = ZLOG_LEVEL_INFO;
        break;
    case LF_LEVEL_WARN:
        zlogLevel = ZLOG_LEVEL_WARN;
        break;
    case LF_LEVEL_ERROR:
        zlogLevel = ZLOG_LEVEL_ERROR;
        break;
    case LF_LEVEL_FATAL:
        zlogLevel = ZLOG_LEVEL_FATAL;
        break;
    }

    // The facade's pattern carries no file, function or line of its own, so
    // zlog is handed empty ones; the framed message is the whole payload.
    zlog(g_category, "", 0, "", 0, 0, zlogLevel, "%s", framed);
}

void vcatLogInit(void)
{
    if (g_initialized)
    {
        return;
    }

    resolveLogDir();

    const char *confPath = resolveConfPath();
    if (zlog_init(confPath) != 0)
    {
        fprintf(stderr, "vcat: no usable zlog configuration (tried %s); logging falls back to printf\n",
                confPath != NULL ? confPath : "ZLOG_CONF_PATH");
        return;
    }

    g_initialized = true;
    g_category = zlog_get_category("vcat");

    // The host's copy of the facade logs through the same sink the plugins are
    // given, so host and plugin logs end up in one place.
    lfRegister(vcatHostLog);

    lfLogInfo("Logging initialized");
}

void vcatLogShutdown(void)
{
    if (!g_initialized)
    {
        return;
    }

    // Unregister first: after zlog_fini() the sink could no longer write, and
    // the facade's printf fallback would corrupt whatever owns the terminal.
    lfRegister(NULL);
    g_initialized = false;
    g_category = NULL;
    zlog_fini();
}
