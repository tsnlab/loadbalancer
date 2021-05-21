#ifndef __NET_H__
#define __NET_H__

#include <stdint.h>
#include <stdlib.h>

#include <sglib.h>


#define NET_HASH_SIZE 256

typedef struct net_tuple {
    uint32_t keyip;
    uint32_t valip;
    uint16_t keyport;
    uint16_t valport;
    uint8_t proto;
    struct net_tuple * next;
} net_tuple;

typedef net_tuple *net_hash[NET_HASH_SIZE];

unsigned int net_tuple_hash_function(net_tuple * t);

#define net_tuple_comparator(x, y) ( \
    ((x)->proto != (y)->proto) ? ((x)->proto - (y)->proto) : \
    ((x)->keyip != (y)->keyip) ? ((x)->keyip - (y)->keyip) : \
    ((x)->keyport - (y)->keyport) \
)

SGLIB_DEFINE_LIST_PROTOTYPES(net_tuple, net_tuple_comparator, next)
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(net_tuple, NET_HASH_SIZE, net_tuple_hash_function)

#endif
