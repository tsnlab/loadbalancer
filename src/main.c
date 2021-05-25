#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <pv/pv.h>
#include <pv/packet.h>
#include <pv/nic.h>
#include <pv/net/ethernet.h>

#include "nat.h"
#include "packet.h"
#include "debug.h"

enum {
    NIC_ID_OUTER = 0,
    NIC_ID_INNER = 1,
};

uint64_t outer_mac;
uint64_t inner_mac;
uint32_t outer_ip;
uint32_t inner_ip;

void print_nic_info();
uint16_t process_pkts(
    nat_map * nat,
    struct pv_packet **pkts_outer, uint16_t nrecv_outer,
    struct pv_packet **pkts_inner, uint16_t nrecv_inner);

int main(int argc, char * argv[]) {
    int ret = pv_init();
    if(ret != 0) {
        fprintf(stderr, "Failed to init pv\n");
        exit(ret);
    }

    pv_nic_get_mac(0, &outer_mac);
    pv_nic_get_mac(1, &inner_mac);
    pv_nic_get_ipv4(0, &outer_ip);
    pv_nic_get_ipv4(1, &inner_ip);

    print_nic_info();

    nat_map nat;
    make_nat(&nat);

    dprintf("Debugging is on\n");

    struct pv_packet * pkt_buf_outer[1024] = {};
    const int buf_size_outer = sizeof(pkt_buf_outer) / sizeof(pkt_buf_outer[0]);

    struct pv_packet * pkt_buf_inner[1024] = {};
    const int buf_size_inner = sizeof(pkt_buf_inner) / sizeof(pkt_buf_inner[0]);

    while(1) {
        uint16_t nrecv_outer = pv_nic_rx_burst(NIC_ID_OUTER, 0, pkt_buf_outer, buf_size_outer);
        uint16_t nrecv_inner = pv_nic_rx_burst(NIC_ID_INNER, 0, pkt_buf_inner, buf_size_inner);

        dprintf("Received %d, %d pkts\n", nrecv_outer, nrecv_inner);

        if(nrecv_outer + nrecv_inner == 0) {
            continue;
        }

        process_pkts(&nat, pkt_buf_outer, nrecv_outer, pkt_buf_inner, nrecv_inner);
    }

    return 0;
}

void print_nic_info() {
    printf("Outer: %012lx %d.%d.%d.%d\n",
           outer_mac,
           outer_ip >> (8 * 3) & 0xff,
           outer_ip >> (8 * 2) & 0xff,
           outer_ip >> (8 * 1) & 0xff,
           outer_ip >> (8 * 0) & 0xff);
    printf("Inner: %012lx %d.%d.%d.%d\n",
           inner_mac,
           inner_ip >> (8 * 3) & 0xff,
           inner_ip >> (8 * 2) & 0xff,
           inner_ip >> (8 * 1) & 0xff,
           inner_ip >> (8 * 0) & 0xff);
}

uint16_t process_pkts(nat_map * nat, struct pv_packet **pkts_outer, uint16_t nrecv_outer, struct pv_packet **pkts_inner, uint16_t nrecv_inner) {
    uint16_t nsent = 0;

    dprintf("Processing outer\n");
    for(int i = 0; i < nrecv_outer; i += 1) {
        struct pv_packet * pkt = pkts_outer[i];

        struct pv_ethernet * ether = (struct pv_ethernet *)pv_packet_data_start(pkt);

        if(is_arp(ether)) {
            dprintf("Process arp\n");
            nsent += process_arp(pkt, NIC_ID_OUTER, outer_mac, outer_ip);
        } else if (is_icmp(ether)) {
            dprintf("Process icmp\n");
            nsent += process_icmp(pkt, NIC_ID_OUTER, outer_mac, outer_ip);
        } else {
            dprintf("Not processed\n");
        }
    }

    dprintf("Processing inner\n");
    for(int i = 0; i < nrecv_inner; i += 1) {
        struct pv_packet * pkt = pkts_inner[i];

        struct pv_ethernet * ether = (struct pv_ethernet *)pv_packet_data_start(pkt);

        if(is_arp(ether)) {
            dprintf("Process arp\n");
            nsent += process_arp(pkt, NIC_ID_OUTER, outer_mac, outer_ip);
        } else if (is_icmp(ether)) {
            dprintf("Process icmp\n");
            nsent += process_icmp(pkt, NIC_ID_OUTER, outer_mac, outer_ip);
        } else {
            dprintf("Not processed\n");
        }
    }

    return nsent;
}
