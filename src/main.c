#include <arpa/inet.h>
#include <pv/net/ipv4.h>

#include <stdio.h>

#include "nat.h"
#include "net.h"

void print_map(nat_map* nat);

struct test {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t proto;
};

int main(int argc, const char* argv[]) {
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

    for (int i = 0; i < num_of_test; i += 1) {
        struct test* test = &tests[i];
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
               pkt.inner_ip,
               pkt.inner_port,
               tuple->masq_ip,
               tuple->masq_port,
               pkt.outer_ip,
               pkt.outer_port);
    }

    print_map(&nat);

    // Check inbound
    puts("== Inbound mapping checks");
    for (int i = 0; i < 10; i += 1) {
        uint16_t port = 1024 + i;

        net_tuple pkt = {
            .outer_ip = tests[0].dst_ip,
            .outer_port = tests[0].dst_port,
            .masq_ip = 0,
            .masq_port = port,
            .proto = tests[0].proto,
        };

        net_tuple* tuple = inbound_map(&nat, &pkt);
        if (tuple == NULL) {
            printf("No mapping for %02x %08x:%d -> %08x:%d\n",
                   pkt.proto,
                   pkt.outer_ip,
                   pkt.outer_port,
                   pkt.masq_ip,
                   pkt.masq_port);
        } else {
            printf("mapping for %02x %08x:%d -> %08x:%d --> %08x:%d\n",
                   pkt.proto,
                   pkt.outer_ip,
                   pkt.outer_port,
                   pkt.masq_ip,
                   pkt.masq_port,
                   tuple->inner_ip,
                   tuple->inner_port);
        }
    }

    puts("== Loadbalancing checks ==");
    const uint16_t lb_port = 0xbeef;
    port_tuple ptuple = {PV_IP_PROTO_UDP, lb_port};
    add_port_forward(&nat, &ptuple, inet_addr("192.168.1.100"), 1111);
    add_port_forward(&nat, &ptuple, inet_addr("192.168.1.101"), 2222);
    add_port_forward(&nat, &ptuple, inet_addr("192.168.1.102"), 3333);
    add_port_forward(&nat, &ptuple, inet_addr("192.168.1.103"), 4444);
    add_port_forward(&nat, &ptuple, inet_addr("192.168.1.104"), 5555);
    for (int i = 0; i < 10; i += 1) {
        net_tuple pkt = {
            .outer_ip = inet_addr("99.99.99.99"),
            .outer_port = 31337 + i % 5,
            .masq_ip = 0,
            .masq_port = lb_port,
            .proto = PV_IP_PROTO_UDP,
        };

        net_tuple* tuple = inbound_map(&nat, &pkt);
        if (tuple == NULL) {
            printf("No mapping for %02x %08x:%d -> %08x:%d\n",
                   pkt.proto,
                   pkt.outer_ip,
                   pkt.outer_port,
                   pkt.masq_ip,
                   pkt.masq_port);
        } else {
            printf("mapping for %02x %08x:%d -> %08x:%d --> %08x:%d\n",
                   pkt.proto,
                   pkt.outer_ip,
                   pkt.outer_port,
                   pkt.masq_ip,
                   pkt.masq_port,
                   tuple->inner_ip,
                   tuple->inner_port);
        }
    }

    return 0;
}

void print_map(nat_map* nat) {
    net_tuple* t;
    struct list_iterator iter;

    puts("=== Port mappings ===");

    list_iterator_init(&iter, nat->net_tuples);
    while (list_iterator_has_next(&iter)) {
        t = (net_tuple*)list_iterator_next(&iter);
        printf("%02x %08x:%d -> %08x:%d -> %08x:%d\n",
               t->proto,
               t->inner_ip,
               t->inner_port,
               t->masq_ip,
               t->masq_port,
               t->outer_ip,
               t->outer_port);
    }
    puts("=== Port mappings ===");
}
