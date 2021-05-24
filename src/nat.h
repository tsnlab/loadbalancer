#ifndef __NAT_H__
#define __NAT_H__

#include "net.h"

#define TIMEOUT_SEC 10

typedef struct nat_map {
    net_tuple * net_tuples;
    port_hash ports;
} nat_map;

void make_nat(nat_map * nat);

uint16_t get_bind_port(port_hash ports, uint8_t proto);
void release_port(port_hash ports, uint8_t proto, uint16_t port);

// FIXME: Cannot use const to pkt due to sglib's problem
net_tuple * outbound_map(nat_map * nat, /*const*/ net_tuple * pkt);
net_tuple * inbound_map(nat_map * nat, /*const*/ net_tuple * pkt);

void cleanup_maps(nat_map * nat);

#endif
