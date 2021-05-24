#ifndef __NET_H__
#define __NET_H__

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <sglib.h>


#define PORT_HASH_SIZE 37

typedef struct net_tuple {
    uint32_t inner_ip;
    uint32_t masq_ip;
    uint32_t outer_ip;  // Not used yet.
    uint16_t inner_port;
    uint16_t masq_port;
    uint16_t outer_port;  // Not used yet.
    uint8_t proto;
    struct timeval last_access;
    struct net_tuple * next;
} net_tuple;

typedef struct port_tuple {
    uint8_t proto;
    uint16_t port;
    struct port_tuple * next;
} port_tuple;

typedef port_tuple *port_hash[PORT_HASH_SIZE];

unsigned int port_tuple_hash_function(port_tuple * t);

#define net_tuple_comparator(x, y) ( \
    ((x)->inner_ip != (y)->inner_ip) ? ((x)->inner_ip - (y)->inner_ip) : \
    ((x)->inner_port != (y)->inner_port) ? ((x)->inner_port - (y)->inner_port) : \
    ((x)->masq_ip != (y)->masq_ip) ? ((x)->masq_ip - (y)->masq_ip) : \
    ((x)->masq_port != (y)->masq_port) ? ((x)->masq_port - (y)->masq_port) : \
    ((x)->outer_ip != (y)->outer_ip) ? ((x)->outer_ip - (y)->outer_ip) : \
    ((x)->outer_port != (y)->outer_port) ? ((x)->outer_port - (y)->outer_port) : \
    ((x)->proto -= (y)->proto) \
)

#define port_tuple_comparator(x, y) ( \
    (x->proto != y->proto) ? x->proto - y->proto : \
    x->port - y->port \
)

SGLIB_DEFINE_LIST_PROTOTYPES(net_tuple, net_tuple_comparator, next)

SGLIB_DEFINE_LIST_PROTOTYPES(port_tuple, port_tuple_comparator, next)
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(port_tuple, PORT_HASH_SIZE, port_tuple_hash_function)

void net_tuple_init(net_tuple ** tuples);
void net_tuple_add(net_tuple ** tuples, net_tuple * t);
void net_tuple_delete(net_tuple ** tuples, net_tuple * t);

#endif
