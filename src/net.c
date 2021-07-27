#include "net.h"

unsigned int port_tuple_hash_function(port_tuple* t) {
    return t->proto * 13 + t->port;
}

SGLIB_DEFINE_LIST_FUNCTIONS(net_tuple, net_tuple_comparator, next);

void net_tuple_init(net_tuple** tuples) {
    *tuples = NULL;
}

void net_tuple_add(net_tuple** tuples, net_tuple* t) {
    sglib_net_tuple_add(tuples, t);
}

void net_tuple_delete(net_tuple** tuples, net_tuple* t) {
    sglib_net_tuple_delete(tuples, t);
}
