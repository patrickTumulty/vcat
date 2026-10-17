
#ifndef INPUT_ARGS_H
#define INPUT_ARGS_H

#include "vcat_plugin_api.h"
#include "vcat_plugin_framework.h"

InputArgsResult vcatProcessInputArgs(int argc, char *argv[], VCatPluginHandle *plugins, const int pluginsLen, VCatPluginHandle *targetPlugin);

#endif // INPUT_ARGS_H
