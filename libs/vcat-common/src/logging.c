
#include "logging.h"
#include "file_utils.h"
#include "logging_facade.h"
#include "zlog.h"
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define VCAT_LOG_CONF "vcat.zlog.conf"
#define VCAT_SHARED_LOG_FILE "vcat.log"

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

    const char *home = getenv("HOME");
    if (home == NULL)
    {
        return;
    }

    char logPath[PATH_MAX];
    snprintf(logPath, sizeof(logPath), "%s/.local/state/vcat/logs", home);

    (void)setenv("VCAT_LOG_DIR", logPath, 1);

    (void)mkdirp(logPath, 0755);
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
static const char *resolveZlogConfPath(void)
{
    static char zlogConfPath[PATH_MAX];

    const char *exePath = resolveExePath();
    if (exePath == NULL)
    {
        printf("Unable to get executable path");
        return VCAT_LOG_CONF;
    }

    (void)snprintf(zlogConfPath, sizeof(zlogConfPath), "%s/%s", exePath, VCAT_LOG_CONF);

    if (access(zlogConfPath, R_OK) == 0)
    {
        return zlogConfPath;
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
 * @param[in] level Facade severity, translated to a zlog level.
 * @param[in] file Source file of the log site.
 * @param[in] fileLen Length of @p file.
 * @param[in] line Source line of the log site.
 * @param[in] msg The message's format string.
 * @param[in] args Variadic payload for @p msg, owned by the caller: read it
 *        (vsnprintf) but never va_end it.
 *
 * Called from plugins too, potentially on GStreamer streaming threads. The
 * facade's pattern carries no file, function or line of its own, so zlog is
 * handed empty ones; the framed message is the whole payload.
 */
void vcatHostLog(LfLogLevel level, const char *file, int fileLen, int line, const char *msg, va_list args)
{
    if (!g_initialized)
    {
        return;
    }

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
    vzlog(g_category, file, fileLen, "", 0, line, zlogLevel, msg, args);
}

void vcatLogInit(void)
{
    if (g_initialized)
    {
        return;
    }

    resolveLogDir();

    time_t rawtime = time(NULL);
    struct tm *timeinfo = localtime(&rawtime);
    char run_id[64];
    strftime(run_id, sizeof(run_id), "%Y-%m-%d_%H-%M-%S", timeinfo);
    setenv("VCAT_RUN_ID", run_id, 1);

    const char *confPath = resolveZlogConfPath();
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

/** Byte offset that reads as "no upper bound" for an entry range. */
#define LOG_OFFSET_UNBOUNDED LONG_MAX

/** Characters in the run id and the log stamp timestamps, "YYYY-MM-DD HH:MM:SS". */
#define VCAT_TIMESTAMP_LEN 19

/** File name scan buffer, comfortably past the timestamp's 19 characters. */
#define VCAT_RUN_ID_LEN 64

/**
 * @brief Extracts the run id from a "vcat.<run-id>.log" file name.
 *
 * The run id is a timestamp ("%Y-%m-%d_%H-%M-%S"), so sorting the names sorts
 * the runs chronologically.
 *
 * @param[in] name Directory entry text.
 * @param[out] runId Output buffer for the extracted id.
 * @param[in] runIdLen Capacity of @p runId.
 *
 * @return RC_OK when the name matches and the id was copied into @p runId,
 *         RC_BAD_INPUT_ARG when the name does not match the pattern.
 */
static Rc extractRunIdFromLogName(const char *name, char *runId, size_t runIdLen)
{
    size_t prefixLen = strlen("vcat.");
    size_t suffixLen = strlen(".log");

    size_t nameLen = strlen(name);
    if (nameLen <= prefixLen + suffixLen ||       //
        strncmp(name, "vcat.", prefixLen) != 0 || //
        strcmp(&name[nameLen - suffixLen], ".log") != 0)
    {
        return RC_BAD_INPUT_ARG;
    }

    const char *idStart = &name[prefixLen];
    size_t idLen = nameLen - prefixLen - suffixLen;
    if (idLen >= runIdLen)
    {
        return RC_BAD_INPUT_ARG;
    }

    memcpy(runId, idStart, idLen);
    runId[idLen] = '\0';
    return RC_OK;
}

/**
 * @brief Finds the newest logged run other than @p currentRunId.
 *
 * Run ids are timestamps, so the lexicographic max is the newest run.
 *
 * @param[in] logDirEntries Open directory holding the per-run log files.
 * @param[in] currentRunId Run id of the run in progress, NULL when unknown.
 * @param[out] lastRunId Output buffer for the newest earlier run's id.
 * @param[in] lastRunIdLen Capacity of @p lastRunId.
 *
 * @return RC_OK when an earlier run's id was copied into @p lastRunId,
 *         RC_ERROR when the directory holds none.
 */
static Rc findNewestPreviousRunId(DIR *logDirEntries, const char *currentRunId, char *lastRunId, size_t lastRunIdLen)
{
    lastRunId[0] = '\0';

    struct dirent *entry;
    while ((entry = readdir(logDirEntries)) != NULL)
    {
        char runId[VCAT_RUN_ID_LEN];
        if (extractRunIdFromLogName(entry->d_name, runId, sizeof(runId)) != RC_OK)
        {
            continue;
        }

        if (currentRunId != NULL && strcmp(runId, currentRunId) == 0)
        {
            continue;
        }

        if (lastRunId[0] == '\0' || strcmp(runId, lastRunId) > 0)
        {
            strcpy(lastRunId, runId);
        }
    }

    return lastRunId[0] == '\0' ? RC_ERROR : RC_OK;
}

/**
 * @brief Prints log lines whose start offset lies below @p untilOffset.
 *
 * Log entries print one line at a time, forwarded as written.
 *
 * @param[in] log Open log file to print from.
 * @param[in] fromOffset Byte offset to start printing from.
 * @param[in] untilOffset Byte offset that ends the range, LOG_OFFSET_UNBOUNDED
 *        for the whole rest of the file.
 */
static void printLogEntriesIn(FILE *log, long fromOffset, long untilOffset)
{
    fseek(log, fromOffset, SEEK_SET);

    char line[4096];
    while (fgets(line, sizeof(line), log) != NULL)
    {
        if (ftell(log) - (long)strlen(line) >= untilOffset)
        {
            return;
        }
        fputs(line, stdout);
    }
}

/**
 * @brief Streams the newest earlier run's log file to stdout.
 *
 * Installed bundles write one file per run ("<log-dir>/vcat.<run-id>.log").
 * The run in progress already has its file, freshly created by vcatLogInit();
 * --last-log is about the run before it.
 *
 * @param[in] logDir Directory holding the per-run log files.
 * @param[in] currentRunId Run id of the run in progress, NULL when unknown.
 *
 * @return RC_OK when a log was printed, RC_ERROR when there is nothing to
 *         print or the directory and file cannot be read.
 */
static Rc printNewestRunLogFile(const char *logDir, const char *currentRunId)
{
    DIR *logDirEntries = opendir(logDir);
    if (logDirEntries == NULL)
    {
        return RC_ERROR;
    }

    char lastRunId[VCAT_RUN_ID_LEN];
    Rc found = findNewestPreviousRunId(logDirEntries, currentRunId, lastRunId, sizeof(lastRunId));
    closedir(logDirEntries);
    if (found != RC_OK)
    {
        return found;
    }

    char logPath[PATH_MAX];
    snprintf(logPath, sizeof(logPath), "%s/vcat.%s.log", logDir, lastRunId);

    FILE *log = fopen(logPath, "r");
    if (log == NULL)
    {
        return RC_ERROR;
    }

    printLogEntriesIn(log, 0, LOG_OFFSET_UNBOUNDED);
    fclose(log);

    return RC_OK;
}

/**
 * @brief Shifts this run's VCAT_RUN_ID into the form the log entry stamps use.
 *
 * vcatLogInit() stamps every run's id as "%Y-%m-%d_%H-%M-%S" while the zlog
 * format spells "%d(%Y-%m-%d %H:%M:%S).%ms"; indexes 10, 13 and 16 are the
 * three separators that differ.
 *
 * @param[out] stamp Output buffer of VCAT_TIMESTAMP_LEN + 1 bytes.
 *
 * @return RC_OK when @p stamp holds this run's "YYYY-MM-DD HH:MM:SS" start,
 *         RC_ERROR when the run id is missing or malformed.
 */
static Rc resolveCurrentRunStartStamp(char stamp[VCAT_TIMESTAMP_LEN + 1])
{
    const char *currentRunId = getenv("VCAT_RUN_ID");
    if (currentRunId == NULL || strlen(currentRunId) != VCAT_TIMESTAMP_LEN)
    {
        return RC_ERROR;
    }

    memcpy(stamp, currentRunId, VCAT_TIMESTAMP_LEN);
    stamp[VCAT_TIMESTAMP_LEN] = '\0';
    stamp[10] = ' ';
    stamp[13] = ':';
    stamp[16] = ':';
    return RC_OK;
}

/**
 * @brief Whether a log line's timestamp has reached @p runStartStamp.
 *
 * Both spell out the same fixed-width "YYYY-MM-DD HH:MM:SS", so a prefix
 * comparison orders the stamp exactly like the run id.
 *
 * @param[in] line One log entry.
 * @param[in] runStartStamp The current run's start timestamp.
 *
 * @return true when the line belongs to the current run or later.
 */
static bool lineAtOrAfterRunStart(const char *line, const char *runStartStamp)
{
    return strncmp(line, runStartStamp, strlen(runStartStamp)) >= 0;
}

/**
 * @brief Whether a log line announces a new run.
 *
 * Every run's first entry is vcatLogInit()'s "Logging initialized".
 *
 * @param[in] line One log entry.
 *
 * @return true when @p line opens a run's entries.
 */
static bool lineStartsNewRun(const char *line)
{
    return strstr(line, "Logging initialized") != NULL;
}

/** Entry offsets splitting a shared log's last run from the rest. */
typedef struct
{
    long previousRunStart;  // "Logging initialized" that opened the last run, -1 when the marker never appeared
    long currentRunStart;   // first entry of the run in progress, -1 when it has none yet
    bool hasEntriesBeforeCurrentRun;
} RunBoundaries;

/**
 * @brief Walks a shared log once, recording where the last run and this run live.
 *
 * The dev configuration appends every run to one rotating "vcat.log", so the
 * last run is bounded above by this run's own first entry (timestamps from
 * this run's start onwards) and below by the previous "Logging initialized".
 *
 * @param[in] log Open shared log file to walk.
 * @param[in] currentRunStartStamp The current run's start timestamp, "" when unknown.
 * @param[out] boundaries Output struct to fill in.
 */
static void findRunBoundaries(FILE *log, const char *currentRunStartStamp, RunBoundaries *boundaries)
{
    boundaries->previousRunStart = -1;
    boundaries->currentRunStart = -1;
    boundaries->hasEntriesBeforeCurrentRun = false;

    while (true)
    {
        long lineStart = ftell(log);
        char line[4096];
        if (fgets(line, sizeof(line), log) == NULL)
        {
            return;
        }

        if (currentRunStartStamp[0] != '\0' && lineAtOrAfterRunStart(line, currentRunStartStamp))
        {
            boundaries->currentRunStart = lineStart;
            return;
        }

        boundaries->hasEntriesBeforeCurrentRun = true;
        if (lineStartsNewRun(line))
        {
            boundaries->previousRunStart = lineStart;
        }
    }
}

/**
 * @brief Streams the previous run's entries out of the shared dev log.
 *
 * When the file predates the marker scheme, the range's floor falls back to
 * the log's top so older entries are not silently skipped.
 *
 * @return RC_OK when a log was printed, RC_ERROR when nothing logged
 *         before this run or the log cannot be read.
 */
static Rc printLastRunFromSharedLog(void)
{
    char currentRunStartStamp[VCAT_TIMESTAMP_LEN + 1] = "";
    resolveCurrentRunStartStamp(currentRunStartStamp);

    FILE *log = fopen(VCAT_SHARED_LOG_FILE, "r");
    if (log == NULL)
    {
        return RC_ERROR;
    }

    RunBoundaries boundaries;
    findRunBoundaries(log, currentRunStartStamp, &boundaries);

    if (!boundaries.hasEntriesBeforeCurrentRun)
    {
        fclose(log);
        return RC_ERROR;
    }

    long fromOffset = boundaries.previousRunStart == -1 ? 0 : boundaries.previousRunStart;
    long untilOffset = boundaries.currentRunStart == -1 ? LOG_OFFSET_UNBOUNDED : boundaries.currentRunStart;

    printLogEntriesIn(log, fromOffset, untilOffset);
    fclose(log);

    return RC_OK;
}

/**
 * @brief Prints the previous run's log file to stdout, one line at a time.
 *
 * Dispatches to whichever logging scheme the host uses: newest per-run log
 * file when the host writes one per run, the previous run's slice of the
 * shared dev log otherwise.
 *
 * @return RC_OK when a log was printed, RC_ERROR when there is nothing to
 *         print or the log cannot be read, RC_NOT_INITIALIZED when logging
 *         has not set up its directories.
 */
Rc vcatLogPrintLastRun(void)
{
    const char *logDir = getenv("VCAT_LOG_DIR");
    const char *currentRunId = getenv("VCAT_RUN_ID");
    if (logDir == NULL || *logDir == '\0')
    {
        return RC_NOT_INITIALIZED;
    }

    // Per-run files first; hosts without them fall back to the shared dev log.
    Rc result = printNewestRunLogFile(logDir, currentRunId);
    if (result == RC_ERROR)
    {
        result = printLastRunFromSharedLog();
    }

    return result;
}
