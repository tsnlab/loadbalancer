#include "nat.h"


unsigned int port_tuple_hash_function(port_tuple * t) {
    return t->proto * 13 + t->port;
}

SGLIB_DEFINE_LIST_FUNCTIONS(port_tuple, port_tuple_comparator, next);
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(port_tuple, PORT_HASH_SIZE, port_tuple_hash_function);

uint16_t get_bind_port(port_hash ports, uint8_t proto) {
    static uint16_t rotation = 0;
    enum {
        start = 1024,
        window = 65536 - start,
    };

    port_tuple * found;
    port_tuple t;
    t.proto = proto;

    do {
        t.port = (rotation++ % window) + start;
        found = sglib_hashed_port_tuple_find_member(ports, &t);
    } while(found);

    found = malloc(sizeof(*found));
    assert(found != NULL);

    found->proto =proto;
    found->port = t.port;
    sglib_hashed_port_tuple_add(ports, found);

    return t.port;
}

void release_port(port_hash ports, uint8_t proto, uint16_t port) {
    port_tuple t = {
        .proto = proto,
        .port = port,
    };
    port_tuple * deleted;
    sglib_hashed_port_tuple_delete_if_member(ports, &t, &deleted);
    free(deleted);
}

net_tuple * outbound_map(net_hash nat_map, port_hash ports, net_tuple * pkt) {
    /*
    public ip = 8.8.8.8 -> 0.0.0.0
    pkt = 192.168.1.1:8080 -> 1.1.1.1:443
    outbound = 192.168.1.1:8080 :> 0.0.0.0:xxx
    inbound = 0.0.0.0:xxx :> 192.168.1.1:8080
    */

    assert(pkt != NULL);

    // FIXME: Why not found on same value
    net_tuple * outbound = sglib_hashed_net_tuple_find_member(nat_map, pkt);
    if(outbound != NULL) {
        return outbound;
    }

    // Add map
    uint16_t bindport = get_bind_port(ports, pkt->proto);

    outbound = malloc(sizeof(net_tuple));
    assert(outbound != NULL);

    outbound->proto = pkt->proto;
    outbound->keyip = pkt->keyip;
    outbound->keyport = pkt->keyport;
    outbound->valip = 0;
    outbound->valport = bindport;

    sglib_hashed_net_tuple_add(nat_map, outbound);

    // Add reverse mapping
    net_tuple * inbound = malloc(sizeof(net_tuple));
    assert(inbound != NULL);

    inbound->proto = pkt->proto;
    inbound->valip = pkt->keyip;
    inbound->valport = pkt->keyport;
    inbound->keyip = 0;
    inbound->keyport = bindport;

    sglib_hashed_net_tuple_add(nat_map, inbound);

    return outbound;
}

net_tuple * inbound_map(net_hash nat_map, port_hash ports, net_tuple * pkt) {
    assert(pkt != NULL);

    net_tuple * inbound = sglib_hashed_net_tuple_find_member(nat_map, pkt);
    return inbound;
}
