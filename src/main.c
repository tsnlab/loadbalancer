#include <stdio.h>
#include <arpa/inet.h>

#include <pv/net/ipv4.h>

#include "net.h"
#include "nat.h"


int main(int argc, const char *argv[])
{
    nat_map nat;
    make_nat(&nat);

    net_tuple pkt = {
            .cli_ip = inet_addr("192.168.0.1"),
            .dst_ip = inet_addr("1.1.1.1"),
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

        pkt.cli_port = test_ports[i][0];
        pkt.dst_port = test_ports[i][1];

        net_tuple *tuple = outbound_map(&nat, &pkt);

        printf("%02x %08x:%d -> %08x:%d -> %08x:%d\n",
               tuple->proto,
               tuple->cli_ip, tuple->cli_port,
               tuple->masq_ip, tuple->masq_port,
               tuple->dst_ip, tuple->dst_port);
    }

    net_tuple_iterate(&nat.net_hash);
    puts("done");

    struct sglib_hashed_net_tuple_cli_iterator it;
    net_tuple * t;
    puts("Here?");
    for(t = sglib_hashed_net_tuple_cli_it_init(&it, nat.net_hash.cli_hash); t != NULL; t = sglib_hashed_net_tuple_cli_it_next(&it)) {
        printf("(%p) %02x %08x:%d -> -> %08x:%d -> %08x:%d\n",
               t,
               t->proto,
               t->cli_ip, t->cli_port,
               t->masq_ip, t->masq_port,
               t->dst_ip, t->dst_port);
    }

    return 0;
}
