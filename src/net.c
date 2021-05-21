#include "net.h"


unsigned int net_tuple_hash_function(net_tuple * t) {
    return t->srcip * 7 + t->srcport * 13 + t->proto * 3;
}

SGLIB_DEFINE_LIST_FUNCTIONS(net_tuple, net_tuple_comparator, next);
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(net_tuple, NET_HASH_SIZE, net_tuple_hash_function);
