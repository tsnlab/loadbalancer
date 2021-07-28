#include <cl/list.h>

#include "net.h"

size_t port_tuple_hash(void* t) {
    port_tuple* x = t;
    return x->proto * 13 + x->port;
}

size_t net_tuple_compare(void* a, void* b) {
    net_tuple* x = a;
    net_tuple* y = b;
    return (((x)->inner_ip != (y)->inner_ip)       ? ((x)->inner_ip - (y)->inner_ip)
            : ((x)->inner_port != (y)->inner_port) ? ((x)->inner_port - (y)->inner_port)
            : ((x)->masq_ip != (y)->masq_ip)       ? ((x)->masq_ip - (y)->masq_ip)
            : ((x)->masq_port != (y)->masq_port)   ? ((x)->masq_port - (y)->masq_port)
            : ((x)->outer_ip != (y)->outer_ip)     ? ((x)->outer_ip - (y)->outer_ip)
            : ((x)->outer_port != (y)->outer_port) ? ((x)->outer_port - (y)->outer_port)
                                                   : ((x)->proto -= (y)->proto));
}

size_t port_tuple_compare(void* a, void* b) {
    port_tuple* x = a;
    port_tuple* y = b;
    return ((x->proto != y->proto) ? x->proto - y->proto : x->port - y->port);
}

void net_tuple_init(net_tuple_list* tuples) {
    *tuples = list_create(net_tuple_compare);
}

void net_tuple_add(net_tuple_list tuples, const net_tuple* t) {
    list_add(tuples, (void*)t);
}

void net_tuple_delete(net_tuple_list tuples, const net_tuple* t) {
    list_remove(tuples, (void*)t);
}
