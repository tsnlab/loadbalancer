#ifndef __NET_H__
#define __NET_H__

#include <sys/time.h>

#include <stdint.h>
#include <stdlib.h>

#define PORT_HASH_SIZE 37

typedef struct net_tuple {
    uint32_t inner_ip;
    uint32_t masq_ip;
    uint32_t outer_ip; // Not used yet.
    uint16_t inner_port;
    uint16_t masq_port;
    uint16_t outer_port; // Not used yet.
    uint8_t proto;
    struct timeval last_access;
} net_tuple;

typedef struct list* net_tuple_list;

typedef struct port_tuple {
    uint8_t proto;
    uint16_t port;
} port_tuple;

typedef struct map* port_tuple_map;

size_t port_tuple_hash(void* t);
size_t port_tuple_compare(void* a, void* b);
size_t net_tuple_compare(void* a, void* b);

void net_tuple_init(net_tuple_list* tuples);
void net_tuple_add(net_tuple_list tuples, const net_tuple* t);
void net_tuple_delete(net_tuple_list tuples, const net_tuple* t);

#endif
