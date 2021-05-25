#ifndef __LB_PACKET_H__
#define __LB_PACKET_H__

#include <stdint.h>

#include <pv/pv.h>

bool is_arp(struct pv_ethernet * ether);
bool is_icmp(struct pv_ethernet * ether);

uint16_t process_arp(struct pv_packet * pkt, uint16_t nicid, uint64_t mac, uint32_t ip);
uint16_t process_icmp(struct pv_packet * pkt, uint16_t nicid, uint64_t mac, uint32_t ip);

#endif
