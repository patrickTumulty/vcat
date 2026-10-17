
#ifndef VCAT_LOGGING_H
#define VCAT_LOGGING_H

/**
 * Host logging backend.
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

#endif // VCAT_LOGGING_H
