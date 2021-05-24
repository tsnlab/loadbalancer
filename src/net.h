#ifndef __NET_H__
#define __NET_H__

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <sglib.h>


#define NET_HASH_SIZE 7
#define PORT_HASH_SIZE 32

typedef struct net_tuple {
    uint32_t cli_ip;
    uint32_t masq_ip;
    uint32_t dst_ip;  // Not used yet.
    uint16_t cli_port;
    uint16_t masq_port;
    uint16_t dst_port;  // Not used yet.
    uint8_t proto;
    struct timeval last_access;
    struct net_tuple * next_cli;
    struct net_tuple * next_masq;
} net_tuple, net_tuple_cli, net_tuple_masq;

typedef struct port_tuple {
    uint8_t proto;
    uint16_t port;
    struct port_tuple * next;
} port_tuple;


typedef struct net_hash {
    net_tuple_cli *cli_hash[NET_HASH_SIZE];
    net_tuple_masq *masq_hash[NET_HASH_SIZE];
} net_hash;

typedef port_tuple *port_hash[PORT_HASH_SIZE];

unsigned int net_tuple_cli_hash_function(net_tuple * t);
unsigned int net_tuple_masq_hash_function(net_tuple * t);
unsigned int port_tuple_hash_function(port_tuple * t);

#define net_tuple_cli_comparator(x, y) ( \
    ((x)->proto != (y)->proto) ? ((x)->proto - (y)->proto) : \
    ((x)->cli_ip != (y)->cli_ip) ? ((x)->cli_ip - (y)->cli_ip) : \
    ((x)->cli_port - (y)->cli_port) \
)

#define net_tuple_masq_comparator(x, y) ( \
    ((x)->proto != (y)->proto) ? ((x)->proto - (y)->proto) : \
    ((x)->masq_ip != (y)->masq_ip) ? ((x)->masq_ip - (y)->masq_ip) : \
    ((x)->masq_port - (y)->masq_port) \
)

#define port_tuple_comparator(x, y) ( \
    (x->proto != y->proto) ? x->proto - y->proto : \
    x->port - y->port \
)

SGLIB_DEFINE_LIST_PROTOTYPES(net_tuple_cli, net_tuple_cli_comparator, next_cli)
SGLIB_DEFINE_LIST_PROTOTYPES(net_tuple_masq, net_tuple_cli_comparator, next_masq)
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(net_tuple_cli, NET_HASH_SIZE, net_tuple_cli_hash_function)
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(net_tuple_masq, NET_HASH_SIZE, net_tuple_masq_hash_function)

SGLIB_DEFINE_LIST_PROTOTYPES(port_tuple, port_tuple_comparator, next)
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(port_tuple, PORT_HASH_SIZE, port_tuple_hash_function)

void net_tuple_iterate(net_hash * h);
void net_tuple_init(net_hash * h);
void net_tuple_add(net_hash * h, net_tuple * t);
void net_tuple_delete(net_hash * h, net_tuple * t);

#endif
