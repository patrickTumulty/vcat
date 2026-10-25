
#ifndef VCAT_NETWORK_ENDPOINT_H
#define VCAT_NETWORK_ENDPOINT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief IPv4 address, e.g. 127.0.0.1
 *
 * octets[0] is the most significant octet ("127" in "127.0.0.1").
 */
typedef union
{
    uint8_t octets[4];
    uint32_t value;
} VcatIpv4Address;

/**
 * @brief an IPv4 address and UDP port pair, e.g. the endpoint a network
 * source binds or joins
 */
typedef struct
{
    VcatIpv4Address address;
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

/**
 * @brief format the address as "a.b.c.d" into @p buffer
 *
 * @return @p buffer for convenience
 */
char *vcatIpv4AddressFormat(const VcatIpv4Address *address, char *buffer, size_t size);

#endif // VCAT_NETWORK_ENDPOINT_H
