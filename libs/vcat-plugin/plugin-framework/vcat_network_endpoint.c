
#include "vcat_network_endpoint.h"
#include "ipv4_address.h"
#include <stdio.h>

bool vcatNetworkEndpointParseUri(VcatNetworkEndpoint *endpoint, const char *uri, const char *scheme)
{
    ipv4_address_t ip;
    int port;

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
        ip = ipv4_localhost;
        *endpoint = (VcatNetworkEndpoint){.ip = ip, .port = port};
        return true;
    }

    return false;
}
