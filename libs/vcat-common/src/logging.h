
#ifndef VCAT_LOGGING_H
#define VCAT_LOGGING_H

#include "crox_types.h"

/**
 * @brief Host logging backend.
 *
 * All vcat code logs through the crox logging facade (logging_facade.h and its
 * lfLog* macros) -- the same facade the plugins use -- so host and plugin logs
 * share one path. This header only owns the backend: it sets zlog up and
 * registers the host sink with the facade. The sink itself (vcatHostLog) is a
 * link-time contract with the plugin manager, which injects it into every
 * loaded plugin; like the manager's use of it, it is deliberately not declared
 * in any header.
 */
void vcatLogInit(void);

/**
 * @brief Logging cleanup.
 */
void vcatLogShutdown(void);

/**
 * @brief Prints the previous run's log file to stdout, one line at a time.
 *
 * Where the host keeps one log per run, the newest file from an earlier run
 * wins; where runs share one log, the previous run's entries are sliced out
 * of it.
 *
 * @return RC_OK when a log was printed, RC_ERROR when there is nothing to
 *         print or the log cannot be read, RC_NOT_INITIALIZED when logging
 *         has not set up its directories.
 */
rc_t vcatLogPrintLastRun(void);

#endif // VCAT_LOGGING_H
