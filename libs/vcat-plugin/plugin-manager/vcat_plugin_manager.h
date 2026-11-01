
#ifndef VCAT_PLUGIN_MANAGER_H
#define VCAT_PLUGIN_MANAGER_H

#include "crox_types.h"
#include "vcat_plugin_api.h"

/**
 * Plugin manager - discovery and lifetime of all loaded plugins.
 *
 * The manager exists once per process (a global singleton). It scans a fixed
 * "plugins" directory next to the executable, loads every ".so" in it through
 * vcatPluginLoad(), and routes the plugins' logging facade to the host log
 * sink. Host code then works with the VCatPluginHandle values it reports via
 * the plugin API functions.
 *
 * Typical order: vcatPluginManagerInit() -> vcatPluginManagerLoadPlugins() ->
 * vcatPluginManagerGetLoadedPlugins() -> ... use the plugins ... ->
 * vcatPluginManagerUnloadPlugins() / per-source vcatVideoSourceCleanup() ->
 * vcatPluginManagerFree().
 *
 * The manager only scans, loads, and tracks; the host drives the rest through
 * the plugin API. The one exception is vcatPluginManagerUnloadPlugins(), which
 * runs every loaded source's vcatVideoSourceCleanup().
 */

/**
 * @brief Allocate the plugin manager singleton.
 *
 * Idempotent: calling it twice in one process fails with an error. Must be
 * called once before all other vcatPluginManager* functions.
 *
 * @return RC_OK if initialized, RC_ERROR if it is already initialized,
 *         RC_MEM_ALLOC_ERROR on out of memory
 */
Rc vcatPluginManagerInit();

/**
 * @brief Free the plugin manager singleton and its bookkeeping.
 *
 * Does NOT unload the loaded plugins: shared libraries stay mapped until
 * process exit. Call vcatPluginManagerUnloadPlugins() (or vcatPluginUnload()
 * per handle) first if you need plugins torn down.
 *
 * @return RC_OK, also when it was never initialized
 */
Rc vcatPluginManagerFree();

/**
 * @brief Scan the plugins directory and load every plugin it contains.
 *
 * The directory is "<path-of-executable>/plugins". Every regular "*.so" file
 * is loaded with vcatPluginLoad(); files or single libraries that fail to load
 * are skipped with a log line, never abort the scan. At most MAX_PLUGINS
 * (= 10) libraries are kept; further files are ignored and logged as a
 * warning.
 *
 * Also routes each loaded plugin's logging facade to the host sink when both
 * exports are available.
 *
 * @return RC_OK if the scan finished, RC_NOT_INITIALIZED if
 *         vcatPluginManagerInit() was not called yet, RC_ALREADY_DONE if the
 *         scan already ran, RC_ERROR if the plugins directory cannot be opened
 */
Rc vcatPluginManagerLoadPlugins();

/**
 * @brief Run each loaded plugin's cleanup and reset the plugin list.
 *
 * Calls vcatVideoSourceCleanup() on every loaded plugin (logging any failure
 * and continuing) and forgets the handles: treat all handles as invalid
 * afterwards. Note the shared libraries themselves stay mapped until the
 * process exits; vcatPluginUnload() is NOT called here.
 *
 * @return RC_OK on success (also when no plugin was ever loaded)
 * @warning must be called after vcatPluginManagerInit(); crashes otherwise
 */
Rc vcatPluginManagerUnloadPlugins();

/**
 * @brief Get how many plugins the manager keeps loaded.
 *
 * @return the number of loaded plugins (0 when the manager is not initialized)
 */
int vcatPluginManagerGetLoadedPluginsCount();

/**
 * @brief Copy the loaded plugins' handles out for the caller to use.
 *
 * @param[out] handlesArray caller-allocated array of capacity
 *                          handlesArrayMaxLen, filled with plugin handles
 * @param[out] handlesArrayLen set to the total number of loaded plugins, even
 *                             when more than handlesArrayMaxLen could be
 *                             copied: only min(count, handlesArrayMaxLen)
 *                             handles land in handlesArray
 * @param[in] handlesArrayMaxLen capacity of handlesArray in elements (0 is
 *                               rejected)
 * @return RC_OK on success, RC_BAD_INPUT_ARG on NULL array/len handle or zero
 *         capacity, RC_NOT_INITIALIZED when nothing was loaded yet (manager
 *         uninitialized, scan not run, or the scan found no plugins)
 */
Rc vcatPluginManagerGetLoadedPlugins(VCatPluginHandle *handlesArray, int *handlesArrayLen, const int handlesArrayMaxLen);

#endif // VCAT_PLUGIN_MANAGER_H
