#include <pv/net/ethernet.h>
#include <pv/nic.h>

#include "ptp.h"
#include <sys/time.h>

#include <string.h>
#include <unistd.h>

#define PTP_TYPE_SYNC 0x0
#define PTP_TYPE_DELAY_REQ 0x1
#define PTP_TYPE_PDELAY_REQ 0x2
#define PTP_TYPE_PDELAY_RESP 0x3
#define PTP_TYPE_FOLLOW_UP 0x8
#define PTP_TYPE_DELAY_RESP 0x9
#define PTP_TYPE_PDELAY_RESP_FOLLOW_UP 0xA
#define PTP_TYPE_ANNOUNCE 0xB
#define PTP_TYPE_SIGNALING 0xC
#define PTP_TYPE_MANAGEMENT 0xD

#define KERNEL_TIME_ADJUST_LIMIT 20000
#define NS_PER_SEC 1000000000

struct ptp_slave_data {
    struct pv_packet* pkt;
    struct timespec t1;
    struct timespec t2;
    struct timespec t3;
    struct timespec t4;
    struct clock_id client_clock_id;
    struct clock_id master_clock_id;
    struct timeval new_adj;
    int64_t delta;
    uint16_t portid;
    uint16_t seqid_sync;
    uint16_t seqid_followup;
    uint8_t ptpset;
    uint8_t kernel_time_set;
    uint16_t current_ptp_port;
};

static struct ptp_slave_data ptp_slave_data;

static inline struct ptp_header* to_ptp_header(struct pv_packet* pkt) {
    return PV_ETH_PAYLOAD(PV_PACKET_PAYLOAD(pkt));
}

static void parse_sync(struct ptp_slave_data* ptp_data);
static void parse_followup(struct ptp_slave_data* ptp_data);
static void parse_delay_response(struct ptp_slave_data* ptp_data);
static void sync_clock(struct ptp_slave_data* ptp_data);
static void update_kernel_time();
static int64_t calculate_delta(struct ptp_slave_data* ptp_data);

static inline uint64_t timespec64_to_ns(const struct timespec* ts) {
    return ((uint64_t)ts->tv_sec * NS_PER_SEC) + ts->tv_nsec;
}

static struct timeval ns_to_timeval(int64_t nsec) {
    struct timespec t_spec = {0, 0};
    struct timeval t_eval = {0, 0};
    int32_t rem;

    if (nsec == 0) {
        return t_eval;
    }

    rem = nsec % NS_PER_SEC;
    t_spec.tv_sec = nsec / NS_PER_SEC;

    if (rem < 0) {
        t_spec.tv_sec -= 1;
        rem += NS_PER_SEC;
    }

    t_spec.tv_nsec = rem;
    t_eval.tv_sec = t_spec.tv_sec;
    t_eval.tv_usec = t_spec.tv_nsec / 1000;

    return t_eval;
}

void process_ptp(struct pv_packet* pkt) {
    struct ptp_header* header = to_ptp_header(pkt);

    ptp_slave_data.pkt = pkt;
    ptp_slave_data.portid = pkt->nic_id;

    switch (header->msg_type) {
    case PTP_TYPE_SYNC:
        parse_sync(&ptp_slave_data);
        break;
    case PTP_TYPE_FOLLOW_UP:
        parse_followup(&ptp_slave_data);
        break;
    case PTP_TYPE_DELAY_RESP:
        parse_delay_response(&ptp_slave_data);
        sync_clock(&ptp_slave_data);
        break;
    default:
        break;
    }
}

static void parse_sync(struct ptp_slave_data* ptp_data) {
    struct ptp_header* ptp_header = to_ptp_header(ptp_data->pkt);

    ptp_data->seqid_sync = ptp_header->seq_id;

    if (ptp_data->ptpset == 0) {
        memcpy(&ptp_data->master_clock_id, &ptp_header->source_port_id.clock_id, sizeof(struct clock_id));
        ptp_data->ptpset = 1;
    }

    if (memcmp(&ptp_data->master_clock_id, &ptp_header->source_port_id.clock_id, sizeof(struct clock_id)) == 0) {
        if (ptp_data->ptpset == 1) {
            ptp_data->t2.tv_sec = 0;
            pv_nic_get_timestamp(ptp_data->portid, &ptp_data->t2);
            bool res = pv_nic_get_rx_timestamp(ptp_data->portid, &ptp_data->t2);

            if (res == false) {
                // XXX: DPDK doesn't work
            }
        }
    }

    pv_packet_free(ptp_data->pkt);
}

static void parse_followup(struct ptp_slave_data* ptp_data) {
    struct ptp_message* ptp_msg = (struct ptp_message*)to_ptp_header(ptp_data->pkt);

    if (memcmp(&ptp_data->master_clock_id, &ptp_msg->header.source_port_id.clock_id, sizeof(struct clock_id)) != 0) {
        pv_packet_free(ptp_data->pkt);
        return;
    }

    ptp_data->seqid_followup = ptp_msg->header.seq_id;
    struct tstamp* origin_timestamp = &ptp_msg->follow_up.precise_origin_tstamp;
    ptp_data->t1.tv_nsec = origin_timestamp->ns;
    ptp_data->t1.tv_sec = ((uint64_t)origin_timestamp->sec_lsb) | ((uint64_t)origin_timestamp->sec_msb << 32);

    if (ptp_data->seqid_followup != ptp_data->seqid_sync) {
        pv_packet_free(ptp_data->pkt);
        return;
    }

    uint64_t macaddr = pv_nic_get_mac(ptp_data->portid);

    struct pv_packet* new_pkt = pv_packet_alloc();
    size_t pkt_size = sizeof(struct delay_req_msg) + PV_ETH_HDR_LEN;
    // FIXME: check boundary
    new_pkt->end = new_pkt->start + pkt_size;

    struct pv_ethernet* eth = PV_PACKET_PAYLOAD(new_pkt);
    struct ptp_message* new_msg = PV_ETH_PAYLOAD(eth);

    eth->smac = macaddr;
    eth->dmac = PV_ETH_ADDR_MULTICAST;
    eth->type = PV_ETH_TYPE_PTP;

    new_msg->delay_req.hdr.seq_id = ptp_data->seqid_sync;
    new_msg->delay_req.hdr.msg_type = PTP_TYPE_DELAY_REQ;
    new_msg->delay_req.hdr.ver = 2;
    new_msg->delay_req.hdr.control = 1;
    new_msg->delay_req.hdr.log_message_interval = 127;
    new_msg->delay_req.hdr.message_length = sizeof(struct delay_req_msg);
    new_msg->delay_req.hdr.domain_number = ptp_msg->header.domain_number;

    // Setup clock id
    new_msg->delay_req.hdr.source_port_id.clock_id.id[0] = (macaddr >> (8 * 5)) & 0xff;
    new_msg->delay_req.hdr.source_port_id.clock_id.id[1] = (macaddr >> (8 * 4)) & 0xff;
    new_msg->delay_req.hdr.source_port_id.clock_id.id[2] = (macaddr >> (8 * 3)) & 0xff;
    new_msg->delay_req.hdr.source_port_id.clock_id.id[3] = 0xff;
    new_msg->delay_req.hdr.source_port_id.clock_id.id[4] = 0xfe;
    new_msg->delay_req.hdr.source_port_id.clock_id.id[5] = (macaddr >> (8 * 2)) & 0xff;
    new_msg->delay_req.hdr.source_port_id.clock_id.id[6] = (macaddr >> (8 * 1)) & 0xff;
    new_msg->delay_req.hdr.source_port_id.clock_id.id[7] = (macaddr >> (8 * 0)) & 0xff;

    memcpy(&ptp_data->client_clock_id, &new_msg->delay_req.hdr.source_port_id.clock_id, sizeof(struct clock_id));

    // set TX_IEEE1588_TMST offload flag;
    pv_packet_set_offloads(new_pkt, PV_PKT_OFFLOAD_TX_TIMESTAMP);

    // Read timestamp before send. to prevent malfunctioning
    pv_nic_get_tx_timestamp(ptp_data->portid, &ptp_data->t3);

    pv_nic_tx(ptp_data->portid, 0, new_pkt);

    ptp_data->t3.tv_sec = 0;
    pv_nic_get_timestamp(ptp_data->portid, &ptp_data->t3);

    int wait_us = 0;
    while (pv_nic_get_tx_timestamp(ptp_data->portid, &ptp_data->t3) != true && wait_us < 1000) {
        usleep(1);
        wait_us += 1;
    }

    pv_packet_free(ptp_data->pkt);
}

static void parse_delay_response(struct ptp_slave_data* ptp_data) {
    struct ptp_message* ptp_msg = (struct ptp_message*)to_ptp_header(ptp_data->pkt);

    uint16_t seq_id = ptp_msg->delay_resp.hdr.seq_id;
    if (memcmp(&ptp_data->client_clock_id, &ptp_msg->delay_resp.req_port_id.clock_id, sizeof(struct clock_id)) != 0) {
        pv_packet_free(ptp_data->pkt);
        return;
    }

    if (seq_id != ptp_data->seqid_followup) {
        pv_packet_free(ptp_data->pkt);
        return;
    }

    struct tstamp* rx_timestamp = &ptp_msg->delay_resp.rx_tstamp;
    ptp_data->t4.tv_nsec = rx_timestamp->ns;
    ptp_data->t4.tv_sec = ((uint64_t)rx_timestamp->sec_lsb) | ((uint64_t)rx_timestamp->sec_msb << 32);

    // printf("t1 %ld.%09ld\n", ptp_data->t1.tv_sec, ptp_data->t1.tv_nsec);
    // printf("t2 %ld.%09ld\n", ptp_data->t2.tv_sec, ptp_data->t2.tv_nsec);
    // printf("t3 %ld.%09ld\n", ptp_data->t3.tv_sec, ptp_data->t3.tv_nsec);
    // printf("t4 %ld.%09ld\n", ptp_data->t4.tv_sec, ptp_data->t4.tv_nsec);

    ptp_data->delta = calculate_delta(ptp_data);
    ptp_data->current_ptp_port = ptp_data->portid;

    pv_packet_free(ptp_data->pkt);
}

static void sync_clock(struct ptp_slave_data* ptp_data) {

    if (ptp_data->delta > NS_PER_SEC || ptp_data->delta < -NS_PER_SEC) {
        // printf("TOO FAR!!\n");
        struct timespec now;
        pv_nic_get_timestamp(ptp_data->portid, &now);
        now.tv_sec += ptp_data->delta / NS_PER_SEC;
        now.tv_nsec += ptp_data->delta % NS_PER_SEC;
        pv_nic_set_timestamp(ptp_data->portid, &ptp_data->t4);
    } else {
        bool res = pv_nic_timesync_adjust_time(ptp_data->portid, ptp_data->delta);
        if (res == false) {
            // XXX: Failed to adjust time
        }
    }

    update_kernel_time();
    // (void)update_kernel_time;
}

static void update_kernel_time() {
    int64_t nsec;
    struct timespec net_time, sys_time;

    clock_gettime(CLOCK_REALTIME, &sys_time);
    bool res = pv_nic_get_timestamp(ptp_slave_data.current_ptp_port, &net_time);

    if (res) {
        nsec = (int64_t)timespec64_to_ns(&net_time) - (int64_t)timespec64_to_ns(&sys_time);
        ptp_slave_data.new_adj = ns_to_timeval(nsec);
    } else {
        // XXX: Set from kernel time
        nsec = ptp_slave_data.delta;
        net_time = ptp_slave_data.t4;
        ptp_slave_data.new_adj = ns_to_timeval(nsec);
    }

    if (nsec > KERNEL_TIME_ADJUST_LIMIT || nsec < -KERNEL_TIME_ADJUST_LIMIT) {
        clock_settime(CLOCK_REALTIME, &net_time);
    } else {
        adjtime(&ptp_slave_data.new_adj, 0);
    }

    // printf("nettime: %ld.%09ld\n", net_time.tv_sec, net_time.tv_nsec);
    // printf("systime: %ld.%09ld\n", sys_time.tv_sec, sys_time.tv_nsec);
    // printf("nsec: %ld\n", nsec);
}

static int64_t calculate_delta(struct ptp_slave_data* ptp_data) {
    int64_t delta;
    uint64_t t1 = 0;
    uint64_t t2 = 0;
    uint64_t t3 = 0;
    uint64_t t4 = 0;

    t1 = timespec64_to_ns(&ptp_data->t1);
    t2 = timespec64_to_ns(&ptp_data->t2);
    t3 = timespec64_to_ns(&ptp_data->t3);
    t4 = timespec64_to_ns(&ptp_data->t4);

    delta = -((int64_t)((t2 - t1) - (t4 - t3))) / 2;

    return delta;
}
