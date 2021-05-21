#include <stdio.h>
#include <arpa/inet.h>

#include <pv/net/ipv4.h>

#include "net.h"
#include "nat.h"


int main(int argc, const char *argv[])
{
    net_hash nat_map;
    sglib_hashed_net_tuple_init(nat_map);

    port_hash ports;
    sglib_hashed_port_tuple_init(ports);

    net_tuple pkt = {
            .keyip = inet_addr("192.168.0.1"),
            .valip = inet_addr("1.1.1.1"),
            .keyport = 31337,
            .proto = PV_IP_PROTO_TCP,
        };
    uint16_t test_ports[][2] = {
        {1024, 443},
        {1024, 80},
        {31337, 443},
        {31337, 80},
        {8080, 8080},
    };

    for(int i = 0; i < sizeof(test_ports) / sizeof(test_ports[0]); i += 1) {

        pkt.keyport = test_ports[i][0];
        pkt.valport = test_ports[i][1];

        net_tuple *outbound = outbound_map(nat_map, ports, &pkt);

        printf("%02x %08x:%d -> %08x:%d -> %08x:%d\n",
               outbound->proto,
               outbound->keyip, outbound->keyport,
               outbound->valip, outbound->valport,
               pkt.valip, pkt.valport);
    }

    struct sglib_hashed_net_tuple_iterator it;
    struct net_tuple * l;
    for(l = sglib_hashed_net_tuple_it_init(&it, nat_map); l != NULL; l = sglib_hashed_net_tuple_it_next(&it)) {
        printf("(%p) %02x %08x:%d -> %08x:%d (next: %p)\n",
               l,
               l->proto,
               l->keyip, l->keyport,
               l->valip, l->valport,
               l->next);
    }

    net_tuple pkt1 = {
        .keyip = inet_addr("192.168.0.1"),
        .valip = inet_addr("1.1.1.1"),
        .keyport = 31337,
        .valport = 443,
        .proto = PV_IP_PROTO_TCP,
    };

    net_tuple pkt2 = {
        .keyip = inet_addr("192.168.0.1"),
        .valip = inet_addr("1.1.1.1"),
        .keyport = 31337,
        .valport = 443,
        .proto = PV_IP_PROTO_TCP,
    };

    printf("%d\n", net_tuple_comparator(&pkt1, &pkt2));

    printf("%p\n", sglib_hashed_net_tuple_find_member(nat_map, &pkt1));
    printf("%p\n", sglib_hashed_net_tuple_find_member(nat_map, &pkt2));
    return 0;
}
