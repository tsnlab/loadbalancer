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

uint64_t self_macs[2];
uint32_t self_ips[2];

void print_nic_info();
uint16_t process_pkts(
    nat_map * nat, int nic_id,
    struct pv_packet **pkts_outer, uint16_t nrecv_outer);

int main(int argc, char * argv[]) {
    int ret = pv_init();
    if(ret != 0) {
        fprintf(stderr, "Failed to init pv\n");
        exit(ret);
    }

    for(int i = 0; i < 2; i += 1) {
        pv_nic_get_mac(i, &self_macs[i]);
        pv_nic_get_ipv4(i, &self_ips[i]);
    }

    print_nic_info();

    nat_map nat;
    make_nat(&nat);

    dprintf("Debugging is on\n");

    struct pv_packet * pkt_buf[1024] = {};
    const int buf_size = sizeof(pkt_buf) / sizeof(pkt_buf[0]);

    while(1) {
        for(int nic_id = 0; nic_id < 2; nic_id += 1) {
            uint16_t nrecv = pv_nic_rx_burst(nic_id, 0, pkt_buf, buf_size);

            if(nrecv == 0) {
                continue;
            }

            dprintf("Received %d pkts from NIC %d\n", nrecv, nic_id);
            process_pkts(&nat, nic_id, pkt_buf, nrecv);
        }
    }

    return 0;
}

void print_nic_info() {
    for(int i = 0; i < 2; i += 1) {
        printf("Nic %d: %012lx %d.%d.%d.%d\n",
               i,
               self_macs[i],
               self_ips[i] >> (8 * 3) & 0xff,
               self_ips[i] >> (8 * 2) & 0xff,
               self_ips[i] >> (8 * 1) & 0xff,
               self_ips[i] >> (8 * 0) & 0xff);
    }
}

uint16_t process_pkts(nat_map * nat, int nic_id, struct pv_packet **pkts, uint16_t nrecv) {
    uint16_t nsent = 0;

    dprintf("Processing\n");
    for(int i = 0; i < nrecv; i += 1) {
        struct pv_packet * pkt = pkts[i];

        struct pv_ethernet * ether = (struct pv_ethernet *)pv_packet_data_start(pkt);

        if(is_arp(ether)) {
            dprintf("Process arp\n");
            nsent += process_arp(pkt, nic_id, self_macs[nic_id], self_ips[nic_id]);
        } else if (is_icmp(ether)) {
            dprintf("Process icmp\n");
            nsent += process_icmp(pkt, nic_id, self_macs[nic_id], self_ips[nic_id]);
        } else {
            dprintf("Not processed\n");
        }
    }

    return nsent;
}
