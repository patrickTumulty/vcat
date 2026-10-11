#include "vcat_plugin_api.h"
#include "vcat_plugin_manager.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    printf("init\n");
    vcatPluginManagerInit();

    printf("load\n");
    Rc rc = vcatPluginManagerLoadPlugins();
    printf("load: %d\n", rc);

    VCatPluginHandle plugins[5];
    int pluginsCount = 0;
    printf("getting plugins\n");
    vcatPluginManagerGetLoadedPlugins(plugins, &pluginsCount, 5);
    printf("getting plugins: count=%d\n", pluginsCount);

    for (int i = 0; i < pluginsCount; i++)
    {
        const char *name = vcatPluginGetName(plugins[i]);
        printf("* '%s'\n", name);
    }

    printf("unload\n");
    rc = vcatPluginManagerUnloadPlugins();
    printf("unload: %d\n", rc);

    printf("free\n");
    vcatPluginManagerFree();

    return EXIT_SUCCESS;
}
