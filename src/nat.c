#include "nat.h"
#include "timeutil.h"

SGLIB_DEFINE_LIST_FUNCTIONS(port_tuple, port_tuple_comparator, next);
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(port_tuple, PORT_HASH_SIZE, port_tuple_hash_function);

net_tuple * net_tuple_find_outbound(net_tuple** tuples, net_tuple* target);

net_tuple * net_tuple_find_inbound(net_tuple** tuples, net_tuple* target);

void make_nat(nat_map* nat) {
    nat->net_tuples = NULL;
    sglib_hashed_port_tuple_init(nat->ports);
}

uint16_t get_bind_port(port_hash ports, uint8_t proto) {
    static uint16_t rotation = 0;
    enum {
        start = 1024,
        window = 65536 - start,
    };

    port_tuple* found;
    port_tuple t;
    t.proto = proto;

    do {
        t.port = (rotation++ % window) + start;
        found = sglib_hashed_port_tuple_find_member(ports, &t);
    } while(found);

    found = malloc(sizeof(*found));
    assert(found != NULL);

    found->proto = proto;
    found->port = t.port;
    sglib_hashed_port_tuple_add(ports, found);

    return t.port;
}

void release_port(port_hash ports, uint8_t proto, uint16_t port) {
    port_tuple t = {
        .proto = proto,
        .port = port,
    };
    port_tuple* deleted;
    sglib_hashed_port_tuple_delete_if_member(ports, &t, &deleted);
    free(deleted);
}

net_tuple* outbound_map(nat_map* nat, net_tuple* pkt) {
    /*
    public ip = 8.8.8.8 -> 0.0.0.0
    pkt = 192.168.1.1:8080 -> 1.1.1.1:443
    outbound = 192.168.1.1:8080 :> 0.0.0.0:xxx
    inbound = 0.0.0.0:xxx :> 192.168.1.1:8080
    */

    assert(pkt != NULL);

    // FIXME: Why not found on same value
    net_tuple* tuple = net_tuple_find_outbound(&nat->net_tuples, pkt);
    if(tuple != NULL) {
        return tuple;
    }

    // Add map
    uint16_t bindport = get_bind_port(nat->ports, pkt->proto);

    tuple = malloc(sizeof(net_tuple));
    assert(tuple != NULL);

    // XXX: Maybe memcpy is faster??
    tuple->proto = pkt->proto;
    tuple->inner_ip = pkt->inner_ip;
    tuple->inner_port = pkt->inner_port;
    tuple->masq_ip = 0;
    tuple->masq_port = bindport;
    tuple->outer_ip = pkt->outer_ip;
    tuple->outer_port = pkt->outer_port;

    net_tuple_add(&nat->net_tuples, tuple);

    return tuple;
}

net_tuple* inbound_map(nat_map* nat, net_tuple* pkt) {
    assert(pkt != NULL);

    net_tuple* tuple = net_tuple_find_inbound(&nat->net_tuples, pkt);
    return tuple;
}

void cleanup_maps(nat_map* nat) {

    struct sglib_net_tuple_iterator it;
    net_tuple* tuple;

    struct timeval now, diff;
    gettimeofday(&now, NULL);

    for(tuple = sglib_net_tuple_it_init(&it, nat->net_tuples); tuple != NULL; tuple = sglib_net_tuple_it_next(&it)) {
        timeval_diff(&tuple->last_access, &now, &diff);
        if(diff.tv_sec >= TIMEOUT_SEC) {
            sglib_net_tuple_delete(&nat->net_tuples, tuple);
        }
    }
}

net_tuple * net_tuple_find_outbound(net_tuple** tuples, net_tuple* target) {

    struct sglib_net_tuple_iterator it;
    net_tuple* tuple;
    for(tuple = sglib_net_tuple_it_init(&it, *tuples); tuple != NULL; tuple = sglib_net_tuple_it_next(&it)) {
        if(tuple->proto == target->proto && tuple->inner_ip == target->inner_ip && tuple->inner_port == target->inner_port) {
            return tuple;
        }
    }

    return NULL;
}

net_tuple * net_tuple_find_inbound(net_tuple** tuples, net_tuple* target) {

    struct sglib_net_tuple_iterator it;
    net_tuple * tuple;
    for(tuple = sglib_net_tuple_it_init(&it, *tuples); tuple != NULL; tuple = sglib_net_tuple_it_next(&it)) {
        if(tuple->proto == target->proto && tuple->masq_ip == target->masq_ip && tuple->masq_port == target->masq_port) {
            return tuple;
        }
    }

    return NULL;
}
