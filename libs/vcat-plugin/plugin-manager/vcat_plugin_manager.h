
#ifndef VCAT_PLUGIN_MANAGER_H
#define VCAT_PLUGIN_MANAGER_H

#include "crox_types.h"
#include "vcat_plugin_api.h"


Rc vcatPluginManagerInit(void);

Rc vcatPluginManagerFree(void);

Rc vcatPluginManagerLoadPlugins(void);

Rc vcatPluginManagerUnloadPlugins(void);

int vcatPluginManagerGetLoadedPluginsCount();
Rc vcatPluginManagerGetLoadedPlugins(VCatPluginHandle *handlesArray, int *handlesArrayLen, const int handlesArrayMaxLen);

#endif // VCAT_PLUGIN_MANAGER_H
