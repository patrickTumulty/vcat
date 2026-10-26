
#ifndef VCAT_NETWORK_ENDPOINT_H
#define VCAT_NETWORK_ENDPOINT_H

#include "crox_types.h"
#include "ipv4_address.h"
#include <stdbool.h>

/**
 * @brief an IPv4 address and UDP port pair, e.g. the endpoint a network
 * source binds or joins
 */
typedef struct
{
    ipv4_address_t ip;
    int port;
} VcatNetworkEndpoint;

/**
 * @brief parse "<scheme>://<ipv4>:<port>" or "<scheme>://localhost:<port>"
 *
 * @param[out] endpoint receives address (localhost => 127.0.0.1) and port
 * @param[in]  uri the source URI token, e.g. "udp://127.0.0.1:1234"
 * @param[in]  scheme expected URI scheme without "://", e.g. "udp"
 *
 * @return true when parsed; @p endpoint is untouched otherwise
 */
bool vcatNetworkEndpointParseUri(VcatNetworkEndpoint *endpoint, const char *uri, const char *scheme);

#endif // VCAT_NETWORK_ENDPOINT_H
