#ifndef __NAT_H__
#define __NAT_H__

#include <cl/map.h>
#include <cl/list.h>

#include "net.h"

#define TIMEOUT_SEC 10

typedef struct nat_map {
    struct list* net_tuples;
    struct map* ports;
    struct map* port_forwards;
} nat_map;

void make_nat(nat_map* nat);

bool add_port_forward(nat_map* nat, const port_tuple* port, const uint32_t inner_ip, const uint16_t inner_port);
bool remove_port_forward(nat_map* nat, const port_tuple* port, const uint32_t inner_ip, const uint16_t inner_port);

uint16_t get_bind_port(port_tuple_map ports, uint8_t proto);
void release_port(port_tuple_map ports, uint8_t proto, uint16_t port);

net_tuple* outbound_map(nat_map* nat, const net_tuple* pkt);
net_tuple* inbound_map(nat_map* nat, const net_tuple* pkt);

void cleanup_maps(nat_map* nat);

#endif
