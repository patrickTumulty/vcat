
#include "vcat_network_endpoint.h"

#include <stdio.h>
#include <string.h>

bool vcatNetworkEndpointParseUri(VcatNetworkEndpoint *endpoint, const char *uri, const char *scheme)
{
    VcatIpv4Address address;
    int port;

    // "<scheme>://<ipv4>:<port>"; the scheme is joined into the scanf format.
    char fmt[sizeof("://%hhu.%hhu.%hhu.%hhu:%d") + 32];
    snprintf(fmt, sizeof(fmt), "%s%s", scheme, "://%hhu.%hhu.%hhu.%hhu:%d");

    if (sscanf(uri, fmt, &address.octets[0], &address.octets[1], &address.octets[2], &address.octets[3], &port) == 5 &&
        port > 0)
    {
        *endpoint = (VcatNetworkEndpoint){.address = address, .port = port};
        return true;
    }

    snprintf(fmt, sizeof(fmt), "%s%s", scheme, "://localhost:%d");
    if (sscanf(uri, fmt, &port) == 1 && port > 0)
    {
        memset(address.octets, 0, sizeof(address.octets));
        address.octets[0] = 127;
        *endpoint = (VcatNetworkEndpoint){.address = address, .port = port};
        return true;
    }

    return false;
}

char *vcatIpv4AddressFormat(const VcatIpv4Address *address, char *buffer, size_t size)
{
    snprintf(buffer, size, "%u.%u.%u.%u", address->octets[0], address->octets[1], address->octets[2],
             address->octets[3]);
    return buffer;
}
