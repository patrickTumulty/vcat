
#ifndef VCAT_PLUGIN_MANAGER_H
#define VCAT_PLUGIN_MANAGER_H

#include "common_types.h"
#include "vcat_plugin_api.h"

// TODO: Documentation

Rc vcatPluginManagerInit();

Rc vcatPluginManagerFree();

Rc vcatPluginManagerLoadPlugins();

Rc vcatPluginManagerUnloadPlugins();

int vcatPluginManagerGetLoadedPluginsCount();

Rc vcatPluginManagerGetLoadedPlugins(VCatPluginHandle *handlesArray, int *handlesArrayLen, const int handlesArraySize);

#endif // VCAT_PLUGIN_MANAGER_H
