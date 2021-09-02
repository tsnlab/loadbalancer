#pragma once

#include <pv/packet.h>

#include <stdbool.h>
#include <stdint.h>

struct tstamp {
    uint16_t sec_msb;
    uint32_t sec_lsb;
    uint32_t ns;
} __attribute__((packed, scalar_storage_order("big-endian")));

struct clock_id {
    uint8_t id[8];
} __attribute__((packed, scalar_storage_order("big-endian")));

struct port_id {
    struct clock_id clock_id;
    uint16_t port_number;
} __attribute__((packed, scalar_storage_order("big-endian")));

struct ptp_header {
    uint8_t msg_type;
    uint8_t ver;
    uint16_t message_length;
    uint8_t domain_number;
    uint8_t reserved1;
    uint8_t flag_field[2];
    int64_t correction;
    uint32_t reserved2;
    struct port_id source_port_id;
    uint16_t seq_id;
    uint8_t control;
    int8_t log_message_interval;
} __attribute__((packed, scalar_storage_order("big-endian")));

struct sync_msg {
    struct ptp_header hdr;
    struct tstamp origin_tstamp;
} __attribute__((packed, scalar_storage_order("big-endian")));

struct follow_up_msg {
    struct ptp_header hdr;
    struct tstamp precise_origin_tstamp;
    uint8_t suffix[0];
} __attribute__((packed, scalar_storage_order("big-endian")));

struct delay_req_msg {
    struct ptp_header hdr;
    struct tstamp origin_tstamp;
} __attribute__((packed, scalar_storage_order("big-endian")));

struct delay_resp_msg {
    struct ptp_header hdr;
    struct tstamp rx_tstamp;
    struct port_id req_port_id;
    uint8_t suffix[0];
} __attribute__((packed, scalar_storage_order("big-endian")));

struct ptp_message {
    union {
        struct ptp_header header;
        struct sync_msg sync;
        struct delay_req_msg delay_req;
        struct follow_up_msg follow_up;
        struct delay_resp_msg delay_resp;
    } __attribute__((packed, scalar_storage_order("big-endian")));
};

void process_ptp(struct pv_packet* pkt);
