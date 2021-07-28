
#include "nat.h"

#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "timeutil.h"

net_tuple* net_tuple_find_outbound(net_tuple_list tuples, const net_tuple* target);

net_tuple* net_tuple_find_inbound(net_tuple_list tuples, const net_tuple* target);

struct port_forward {
    uint32_t inner_ip;
    uint16_t inner_port;
};

size_t port_compare(void* a, void* b) {
    struct port_forward* x = a;
    struct port_forward* y = b;
    return x->inner_ip == y->inner_ip ? x->inner_ip - y->inner_ip : x->inner_port - y->inner_port;
}

void make_nat(nat_map* nat) {
    nat->net_tuples = list_create(net_tuple_compare);
    nat->ports = map_create(PORT_HASH_SIZE, port_tuple_hash, port_tuple_compare);
    nat->port_forwards = map_create(PORT_HASH_SIZE, port_tuple_hash, port_tuple_compare);
}

bool add_port_forward(nat_map* nat, const port_tuple* port, const uint32_t inner_ip, const uint16_t inner_port) {
    struct list* list = map_get(nat->port_forwards, (void*)port);
    if (list == NULL) {
        list = list_create(port_compare);
        if (list == NULL) {
            return false;
        }
        map_put(nat->port_forwards, (void*)port, (void*)list);
    }

    struct port_forward* pf = malloc(sizeof(struct port_forward));
    if (pf == NULL) {
        return false;
    }

    pf->inner_ip = inner_ip;
    pf->inner_port = inner_port;

    return list_add(list, (void*)pf);
}

bool remove_port_forward(nat_map* nat, const port_tuple* port, const uint32_t inner_ip, const uint16_t inner_port) {
    struct list* list = map_get(nat->port_forwards, (void*)port);
    if (list == NULL) {
        return false;
    }

    struct port_forward pf = {inner_ip, inner_port};

    struct port_forward* removed = list_remove(list, &pf);
    if (removed != NULL) {
        free(removed);
    }

    if (list_size(list) == 0) {
        list_destroy(list);
        map_remove(nat->port_forwards, (void*)port);
    }

    return removed != NULL;
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

    // Check port_forward first
    struct port_tuple ptuple = {
        .proto = pkt->proto,
        .port = pkt->masq_port,
    };

    net_tuple* tuple = net_tuple_find_inbound(nat->net_tuples, pkt);
    if (tuple != NULL) {
        return tuple;
    }

    struct list* pf_list = map_get(nat->port_forwards, &ptuple);
    if (pf_list != NULL) {
        // XXX: using simple unsecure random
        srand(time(NULL) + rand());
        size_t idx = rand() % list_size(pf_list);

        struct port_forward* pf = list_get(pf_list, idx);
        net_tuple* tuple = malloc(sizeof(net_tuple));
        assert(tuple != NULL);

        *tuple = *pkt;
        tuple->inner_ip = pf->inner_ip;
        tuple->inner_port = pf->inner_port;
        net_tuple_add(nat->net_tuples, tuple);
        return tuple;
    }

    return NULL;
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
        if (tuple->proto == target->proto && tuple->inner_ip == target->inner_ip &&
            tuple->inner_port == target->inner_port) {
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
        if (tuple->proto == target->proto && tuple->outer_ip == target->outer_ip &&
            tuple->outer_port == target->outer_port && tuple->masq_ip == target->masq_ip &&
            tuple->masq_port == target->masq_port) {
            return tuple;
        }
    }

    return NULL;
}
