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
            .srcip = inet_addr("192.168.0.1"),
            .dstip = inet_addr("1.1.1.1"),
            .srcport = 31337,
            .proto = PV_IP_PROTO_TCP,
        };
    uint16_t iports[] = {1024, 1024, 1000, 1000, 8080, 443};

    for(int i = 0; i < sizeof(iports) / sizeof(iports[0]); i += 1) {

        pkt.dstport = iports[i];

        net_tuple *outbound = outbound_map(nat_map, ports, &pkt);

        printf("%02x %08x:%d -> %08x:%d\n",
               outbound->proto,
               outbound->srcip, outbound->srcport,
               outbound->dstip, outbound->dstport);
    }

    struct sglib_hashed_net_tuple_iterator it;
    struct net_tuple * l;
    for(l = sglib_hashed_net_tuple_it_init(&it, nat_map); l != NULL; l = sglib_hashed_net_tuple_it_next(&it)) {
        printf("was %02x %08x:%d -> %08x:%d\n",
               l->proto,
               l->srcip, l->srcport,
               l->dstip, l->dstport);
    }
    return 0;
}
