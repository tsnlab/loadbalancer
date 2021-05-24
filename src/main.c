#include <stdio.h>
#include <arpa/inet.h>

#include <pv/net/ipv4.h>

#include "net.h"
#include "nat.h"

void print_map(nat_map * nat);

struct test {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t proto;
};

int main(int argc, const char *argv[])
{
    nat_map nat;
    make_nat(&nat);

    struct test tests[] = {
        {inet_addr("192.168.0.1"), inet_addr("1.1.1.1"), 31337, 443, PV_IP_PROTO_TCP},
        {inet_addr("192.168.0.2"), inet_addr("1.1.1.1"), 31337, 443, PV_IP_PROTO_TCP},
        {inet_addr("192.168.0.1"), inet_addr("1.1.1.2"), 31337, 443, PV_IP_PROTO_TCP},
        {inet_addr("192.168.0.1"), inet_addr("1.1.1.1"), 8087, 443, PV_IP_PROTO_TCP},
        {inet_addr("192.168.0.1"), inet_addr("1.1.1.1"), 8087, 443, PV_IP_PROTO_TCP},
        {inet_addr("192.168.0.1"), inet_addr("8.8.8.8"), 31337, 27015, PV_IP_PROTO_UDP},
        {inet_addr("192.168.0.1"), inet_addr("8.8.4.4"), 31337, 443, PV_IP_PROTO_UDP},
    };

    const int num_of_test = sizeof(tests) / sizeof(tests[0]);

    puts("== Outbound mapping checks");

    for(int i = 0; i < num_of_test; i += 1) {
        struct test * test = &tests[i];
        net_tuple pkt = {
            .inner_ip = test->src_ip,
            .outer_ip = test->dst_ip,
            .inner_port = test->src_port,
            .outer_port = test->dst_port,
            .proto = test->proto,
        };

        net_tuple* tuple = outbound_map(&nat, &pkt);

        printf("Mapped %02x %08x:%d -> %08x:%d -> %08x:%d\n",
               pkt.proto,
               pkt.inner_ip, pkt.inner_port,
               tuple->masq_ip, tuple->masq_port,
               pkt.outer_ip, pkt.outer_port);
    }

    print_map(&nat);

    // Check inbound
    puts("== Inbound mapping checks");
    for(int i = 0; i < 10; i += 1) {
        uint16_t port = 1024 + i;

        net_tuple pkt = {
            .outer_ip = tests[0].dst_ip,
            .outer_port = tests[0].dst_port,
            .masq_ip = 0,
            .masq_port = port,
            .proto = tests[0].proto,
        };

        net_tuple * tuple = inbound_map(&nat, &pkt);
        if (tuple == NULL) {
            printf("No mapping for %02x %08x:%d -> %08x:%d\n",
                   pkt.proto,
                   pkt.outer_ip, pkt.outer_port,
                   pkt.masq_ip, pkt.masq_port);
        } else {
            printf("mapping for %02x %08x:%d -> %08x:%d --> %08x:%d\n",
                   pkt.proto,
                   pkt.outer_ip, pkt.outer_port,
                   pkt.masq_ip, pkt.masq_port,
                   tuple->inner_ip, tuple->inner_port);
        }
    }

    return 0;
}

void print_map(nat_map * nat) {
    struct sglib_net_tuple_iterator it;
    net_tuple * t;

    puts("=== Port mappings ===");

    for(t = sglib_net_tuple_it_init(&it, nat->net_tuples); t != NULL; t = sglib_net_tuple_it_next(&it)) {
        printf("%02x %08x:%d -> %08x:%d -> %08x:%d\n",
               t->proto,
               t->inner_ip, t->inner_port,
               t->masq_ip, t->masq_port,
               t->outer_ip, t->outer_port);
    }
    puts("=== Port mappings ===");
}
