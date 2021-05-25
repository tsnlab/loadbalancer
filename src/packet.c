#include <pv/pv.h>
#include <pv/packet.h>
#include <pv/nic.h>
#include <pv/net/ethernet.h>
#include <pv/net/arp.h>
#include <pv/net/ipv4.h>

#include "packet.h"
#include "debug.h"


bool is_arp(struct pv_ethernet * ether) {
    return ether->type == PV_ETH_TYPE_ARP;
}

bool is_icmp(struct pv_ethernet * ether) {
    if(ether->type != PV_ETH_TYPE_IPv4) {
        return false;
    }

    struct pv_ipv4 * ipv4 = (struct pv_ipv4 *)PV_ETH_PAYLOAD(ether);
    return ipv4->proto == PV_IP_PROTO_ICMP;
}

uint16_t process_arp(struct pv_packet * pkt, uint16_t nicid, uint64_t mac, uint32_t ip) {
    struct pv_ethernet * ether = (struct pv_ethernet *)pv_packet_data_start(pkt);
    struct pv_arp * arp = (struct pv_arp *)PV_ETH_PAYLOAD(ether);

    if(arp->opcode != PV_ARP_OPCODE_ARP_REQUEST) {
        return 0;
    }

    if(arp->dst_proto != ip) {
        return 0;
    }

    dprintf("Replying to ARP Request\n");

    ether->dmac = ether->smac;
    ether->smac = mac;

    arp->opcode = PV_ARP_OPCODE_ARP_REPLY;
    arp->dst_hw = arp->src_hw;
    arp->dst_proto = arp->src_proto;
    arp->src_hw = mac;
    arp->src_proto = ip;

    pv_nic_tx(nicid, 0, pkt);

    return 1;
}

uint16_t process_icmp(struct pv_packet * pkt, uint16_t nicid, uint64_t mac, uint32_t ip) {
    return 0;
}
