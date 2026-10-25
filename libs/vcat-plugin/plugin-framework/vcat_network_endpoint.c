
#include "vcat_network_endpoint.h"

#include <stdio.h>
#include <string.h>

bool vcatNetworkEndpointParseUri(VcatNetworkEndpoint *endpoint, const char *uri, const char *scheme)
{
    ipv4_address_t ip;
    int port;

    // "<scheme>://<ipv4>:<port>"; the scheme is joined into the scanf format.
    char fmt[sizeof("://%hhu.%hhu.%hhu.%hhu:%d") + 32];
    snprintf(fmt, sizeof(fmt), "%s%s", scheme, "://%hhu.%hhu.%hhu.%hhu:%d");

    if (sscanf(uri, fmt, &ip.o3, &ip.o2, &ip.o1, &ip.o0, &port) == 5 && port > 0)
    {
        *endpoint = (VcatNetworkEndpoint){.ip = ip, .port = port};
        return true;
    }

    snprintf(fmt, sizeof(fmt), "%s%s", scheme, "://localhost:%d");
    if (sscanf(uri, fmt, &port) == 1 && port > 0)
    {
        ip.o3 = 127;
        ip.o2 = 0;
        ip.o1 = 0;
        ip.o0 = 1;
        *endpoint = (VcatNetworkEndpoint){.ip = ip, .port = port};
        return true;
    }

    return false;
}
