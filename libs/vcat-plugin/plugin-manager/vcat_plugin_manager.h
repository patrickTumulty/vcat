
#ifndef VCAT_PLUGIN_MANAGER_H
#define VCAT_PLUGIN_MANAGER_H

#include "crox_types.h"
#include "vcat_plugin_api.h"


rc_t vcatPluginManagerInit(void);

rc_t vcatPluginManagerFree(void);

rc_t vcatPluginManagerLoadPlugins(void);

rc_t vcatPluginManagerUnloadPlugins(void);

int vcatPluginManagerGetLoadedPluginsCount();
rc_t vcatPluginManagerGetLoadedPlugins(VCatPluginHandle *handlesArray, int *handlesArrayLen, const int handlesArrayMaxLen);

#endif // VCAT_PLUGIN_MANAGER_H
