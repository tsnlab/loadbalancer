#include <assert.h>

#include "nat.h"
#include "timeutil.h"

net_tuple* net_tuple_find_outbound(net_tuple_list tuples, const net_tuple* target);

net_tuple* net_tuple_find_inbound(net_tuple_list tuples, const net_tuple* target);

void make_nat(nat_map* nat) {
    nat->net_tuples = list_create(net_tuple_compare);
    nat->ports = map_create(PORT_HASH_SIZE, port_tuple_hash, port_tuple_compare);
}

uint16_t get_bind_port(port_tuple_map ports, uint8_t proto) {
    static uint16_t rotation = 0;
    enum {
        start = 1024,
        window = 65536 - start,
    };

    bool found;
    port_tuple t;
    t.proto = proto;

    do {
        t.port = (rotation++ % window) + start;
        found = map_has(ports, &t);
    } while (found);

    map_put(ports, &t, from_bool(true));

    return t.port;
}

void release_port(port_tuple_map ports, uint8_t proto, uint16_t port) {
    port_tuple t = {
        .proto = proto,
        .port = port,
    };
    map_remove(ports, &t);
}

net_tuple* outbound_map(nat_map* nat, const net_tuple* pkt) {
    /*
    public ip = 8.8.8.8 -> 0.0.0.0
    pkt = 192.168.1.1:8080 -> 1.1.1.1:443
    outbound = 192.168.1.1:8080 :> 0.0.0.0:xxx
    inbound = 0.0.0.0:xxx :> 192.168.1.1:8080
    */

    assert(pkt != NULL);

    // FIXME: Why not found on same value
    net_tuple* tuple = net_tuple_find_outbound(nat->net_tuples, pkt);
    if (tuple != NULL) {
        return tuple;
    }

    // Add map
    uint16_t bindport = get_bind_port(nat->ports, pkt->proto);

    tuple = malloc(sizeof(net_tuple));
    assert(tuple != NULL);

    *tuple = *pkt;
    tuple->masq_ip = 0;
    tuple->masq_port = bindport;

    net_tuple_add(nat->net_tuples, tuple);

    return tuple;
}

net_tuple* inbound_map(nat_map* nat, const net_tuple* pkt) {
    assert(pkt != NULL);

    net_tuple* tuple = net_tuple_find_inbound(nat->net_tuples, pkt);
    return tuple;
}

void cleanup_maps(nat_map* nat) {
    struct timeval now, diff;
    gettimeofday(&now, NULL);

    struct list_iterator iter;
    list_iterator_init(&iter, nat->net_tuples);
    while (list_iterator_has_next(&iter)) {
        net_tuple* tuple = (net_tuple*)list_iterator_next(&iter);
        timeval_diff(&tuple->last_access, &now, &diff);
        if (diff.tv_sec >= TIMEOUT_SEC) {
            list_iterator_remove(&iter);
        }
    }
}

net_tuple* net_tuple_find_outbound(net_tuple_list tuples, const net_tuple* target) {
    struct list_iterator iter;
    list_iterator_init(&iter, tuples);
    while (list_iterator_has_next(&iter)) {
        net_tuple* tuple = (net_tuple*)list_iterator_next(&iter);
        if (tuple->proto == target->proto && tuple->inner_ip == target->inner_ip && tuple->inner_port == target->inner_port) {
            return tuple;
        }
    }

    return NULL;
}

net_tuple* net_tuple_find_inbound(net_tuple_list tuples, const net_tuple* target) {
    struct list_iterator iter;
    list_iterator_init(&iter, tuples);
    while (list_iterator_has_next(&iter)) {
        net_tuple* tuple = (net_tuple*)list_iterator_next(&iter);
        if (tuple->proto == target->proto &&
            /* tuple->masq_ip == target->masq_ip && */ tuple->masq_port == target->masq_port) {
            return tuple;
        }
    }

    return NULL;
}
