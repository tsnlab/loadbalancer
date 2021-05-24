#include "net.h"


unsigned int net_tuple_cli_hash_function(net_tuple * t) {
    unsigned int res = t->cli_ip * 7 + t->cli_port * 13 + t->proto * 3;
    printf("Hash cli: %u %u\n", res, res % NET_HASH_SIZE);
    return res;
}

unsigned int net_tuple_masq_hash_function(net_tuple * t) {
    unsigned int res = t->masq_ip * 7 + t->masq_port * 13 + t->proto;
    printf("Hash masq: %u\n", res);
    return res;
}

unsigned int port_tuple_hash_function(port_tuple * t) {
    return t->proto * 13 + t->port;
}

SGLIB_DEFINE_LIST_FUNCTIONS(net_tuple_cli, net_tuple_cli_comparator, next_cli);
SGLIB_DEFINE_LIST_FUNCTIONS(net_tuple_masq, net_tuple_masq_comparator, next_masq);
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(net_tuple_cli, NET_HASH_SIZE, net_tuple_cli_hash_function);
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(net_tuple_masq, NET_HASH_SIZE, net_tuple_masq_hash_function);

void net_tuple_iterate(net_hash * h) {
    net_tuple * t;
    struct sglib_hashed_net_tuple_cli_iterator it_cli;
    struct sglib_hashed_net_tuple_masq_iterator it_masq;

    printf("cli_hash = %p, masq_hash = %p\n", h->cli_hash, h->masq_hash);

    puts("CLI:");
    t = sglib_hashed_net_tuple_cli_it_init(&it_cli, h->cli_hash);
    for (
        t = sglib_hashed_net_tuple_cli_it_init(&it_cli, h->cli_hash);
        t != NULL;
        t = sglib_hashed_net_tuple_cli_it_next(&it_cli)
    ) {
        printf("%p -> %p (%p)\n", t, t->next_cli, t->next_masq);
    }

    puts("MASQ:");
    t = sglib_hashed_net_tuple_masq_it_init(&it_masq, h->masq_hash);
    for (
        t = sglib_hashed_net_tuple_masq_it_init(&it_masq, h->masq_hash);
        t != NULL;
        t = sglib_hashed_net_tuple_masq_it_next(&it_masq)
    ) {
        printf("%p -> %p (%p)\n", t, t->next_masq, t->next_cli);
    }
}

void net_tuple_init(net_hash * h) {
    sglib_hashed_net_tuple_cli_init(h->cli_hash);
    sglib_hashed_net_tuple_masq_init(h->masq_hash);
}

void net_tuple_add(net_hash * h, net_tuple * t) {
    sglib_net_tuple_cli_add(h->cli_hash, t);
    printf("cli,t->next: %p, %p\n", t->next_cli, t->next_masq);
    sglib_net_tuple_masq_add(h->masq_hash, t);
    printf("masq,t->next: %p, %p\n", t->next_cli, t->next_masq);
}

void net_tuple_delete(net_hash * h, net_tuple * t) {
    sglib_net_tuple_cli_delete(h->cli_hash, t);
    sglib_net_tuple_masq_delete(h->masq_hash, t);

    free(t);
}
