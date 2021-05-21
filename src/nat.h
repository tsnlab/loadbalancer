#ifndef __NAT_H__
#define __NAT_H__

#include "net.h"

#define PORT_HASH_SIZE 32

typedef struct port_tuple {
    uint8_t proto;
    uint16_t port;
    struct port_tuple * next;
} port_tuple;

typedef port_tuple *port_hash[PORT_HASH_SIZE];
unsigned int port_tuple_hash_function(port_tuple * t);

#define port_tuple_comparator(x, y) ( \
    (x->proto != y->proto) ? x->proto - y->proto : \
    x->port - y->port \
)

SGLIB_DEFINE_LIST_PROTOTYPES(port_tuple, port_tuple_comparator, next)
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(port_tuple, PORT_HASH_SIZE, port_tuple_hash_function)

uint16_t get_bind_port(port_hash ports, uint8_t proto);
void release_port(port_hash ports, uint8_t proto, uint16_t port);

// FIXME: Cannot use const to pkt due to sglib's problem
net_tuple * outbound_map(net_hash nat_map, port_hash ports, /*const*/ net_tuple * pkt);
net_tuple * inbound_map(net_hash nat_map, port_hash ports, /*const*/ net_tuple * pkt);

#endif
