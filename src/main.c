#include <stdio.h>
#include <arpa/inet.h>

#include <pv/net/ipv4.h>

#include "net.h"
#include "nat.h"

void print_map(nat_map * nat);

int main(int argc, const char *argv[])
{
    nat_map nat;
    make_nat(&nat);

    net_tuple pkt = {
            .inner_ip = inet_addr("192.168.0.1"),
            .outer_ip = inet_addr("1.1.1.1"),
            .proto = PV_IP_PROTO_TCP,
        };
    uint16_t test_ports[][2] = {
        {1024, 443},
        {1024, 80},
        {31337, 443},
        {31337, 80},
        {8080, 8080},
    };

    for (int x = 0; x < 2; x += 1) {
        switch(x) {
        case 0:
            pkt.proto = PV_IP_PROTO_TCP;
            break;
        case 1:
            pkt.proto = PV_IP_PROTO_UDP;
            break;
        }

        for(int i = 0; i < sizeof(test_ports) / sizeof(test_ports[0]); i += 1) {

            pkt.inner_port = test_ports[i][0];
            pkt.outer_port = test_ports[i][1];

            net_tuple* tuple = outbound_map(&nat, &pkt);

            printf("Add %02x %08x:%d -> %08x:%d -> %08x:%d\n", tuple->proto, tuple->inner_ip, tuple->inner_port,
                   tuple->masq_ip, tuple->masq_port, tuple->outer_ip, tuple->outer_port);
        }
    }

    print_map(&nat);

    return 0;
}

void print_map(nat_map * nat) {
    struct sglib_net_tuple_iterator it;
    net_tuple * t;

    for(t = sglib_net_tuple_it_init(&it, nat->net_tuples); t != NULL; t = sglib_net_tuple_it_next(&it)) {
        printf("%02x %08x:%d -> -> %08x:%d -> %08x:%d\n",
               t->proto,
               t->inner_ip, t->inner_port,
               t->masq_ip, t->masq_port,
               t->outer_ip, t->outer_port);
    }
}
