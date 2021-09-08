#include <pv/net/ethernet.h>
#include <pv/nic.h>

#include "ptp.h"
#include <sys/time.h>
#include <sys/timex.h>

#include <string.h>
#include <unistd.h>

#include "utils.h"

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

#define NS_PER_SEC 1000000000
#define KERNEL_TIME_ADJUST_LIMIT (NS_PER_SEC / 2)
#define NEW_MASTER_THRESHOLD_SEC 4

struct ptp_slave_data {
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
    uint16_t seqid_delay_req;
    uint8_t ptpset;
    uint8_t kernel_time_set;
    uint16_t current_ptp_port;
};

static struct ptp_slave_data ptp_slave_data;

static inline struct ptp_header* to_ptp_header(struct pv_packet* pkt) {
    return PV_ETH_PAYLOAD(PV_PACKET_PAYLOAD(pkt));
}

static void process_sync(struct ptp_slave_data* ptp_data, struct pv_packet* pkt);
static void process_followup(struct ptp_slave_data* ptp_data, struct pv_packet* pkt);
static void process_delay_request(struct ptp_slave_data* ptp_data, struct pv_packet* pkt);
static void process_delay_response(struct ptp_slave_data* ptp_data, struct pv_packet* pkt);
static void sync_clock(struct ptp_slave_data* ptp_data);
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

    ptp_slave_data.portid = pkt->nic_id;

    switch (header->msg_type) {
    case PTP_TYPE_SYNC:
        process_sync(&ptp_slave_data, pkt);
        break;
    case PTP_TYPE_FOLLOW_UP:
        process_followup(&ptp_slave_data, pkt);
        break;
    case PTP_TYPE_DELAY_REQ:
        process_delay_request(&ptp_slave_data, pkt);
        break;
    case PTP_TYPE_DELAY_RESP:
        process_delay_response(&ptp_slave_data, pkt);
        break;
    default:
        break;
    }
}

static bool is_same_clkid(struct clock_id* clk1, struct clock_id* clk2) {
    return memcmp(clk1, clk2, sizeof(struct clock_id)) == 0;
}

static void process_sync(struct ptp_slave_data* ptp_data, struct pv_packet* pkt) {
    struct ptp_header* ptp_header = to_ptp_header(pkt);

    if (ptp_data->ptpset == 0) {
        memcpy(&ptp_data->master_clock_id, &ptp_header->source_port_id.clock_id, sizeof(struct clock_id));
        ptp_data->ptpset = 1;
    } else {
        struct timespec now;
        clock_gettime(CLOCK_TAI, &now);
        if (now.tv_sec - ptp_data->t2.tv_sec > NEW_MASTER_THRESHOLD_SEC) {
            dprintf("New master!\n");
            memcpy(&ptp_data->master_clock_id, &ptp_header->source_port_id.clock_id, sizeof(struct clock_id));
        }
    }

    if (is_same_clkid(&ptp_data->master_clock_id, &ptp_header->source_port_id.clock_id)) {
        clock_gettime(CLOCK_TAI, &ptp_data->t2);
        ptp_data->seqid_sync = ptp_header->seq_id;
    }
}

static void process_followup(struct ptp_slave_data* ptp_data, struct pv_packet* pkt) {
    struct ptp_message* ptp_msg = (struct ptp_message*)to_ptp_header(pkt);

    if (!is_same_clkid(&ptp_data->master_clock_id, &ptp_msg->header.source_port_id.clock_id)) {
        return;
    }

    struct tstamp* origin_timestamp = &ptp_msg->follow_up.precise_origin_tstamp;
    ptp_data->t1.tv_nsec = origin_timestamp->ns;
    ptp_data->t1.tv_sec = ((uint64_t)origin_timestamp->sec_lsb) | ((uint64_t)origin_timestamp->sec_msb << 32);

    if (ptp_msg->header.seq_id != ptp_data->seqid_sync) {
        return;
    }
}

static void process_delay_request(struct ptp_slave_data* ptp_data, struct pv_packet* pkt) {
    struct ptp_message* ptp_msg = (struct ptp_message*)to_ptp_header(pkt);

    ptp_data->seqid_delay_req = ptp_msg->header.seq_id;

    clock_gettime(CLOCK_TAI, &ptp_data->t3);
    memcpy(&ptp_data->client_clock_id, &ptp_msg->header.source_port_id.clock_id, sizeof(struct clock_id));
}

static void process_delay_response(struct ptp_slave_data* ptp_data, struct pv_packet* pkt) {
    struct ptp_message* ptp_msg = (struct ptp_message*)to_ptp_header(pkt);

    uint16_t seq_id = ptp_msg->delay_resp.hdr.seq_id;
    if (seq_id != ptp_data->seqid_delay_req) {
        return;
    }

    if (!is_same_clkid(&ptp_data->master_clock_id, &ptp_msg->header.source_port_id.clock_id)) {
        return;
    }
    if (!is_same_clkid(&ptp_data->client_clock_id, &ptp_msg->delay_resp.req_port_id.clock_id)) {
        return;
    }

    struct tstamp* rx_timestamp = &ptp_msg->delay_resp.rx_tstamp;
    ptp_data->t4.tv_nsec = rx_timestamp->ns;
    ptp_data->t4.tv_sec = ((uint64_t)rx_timestamp->sec_lsb) | ((uint64_t)rx_timestamp->sec_msb << 32);

    dprintf("t1 %010ld.%09ld\n", ptp_data->t1.tv_sec, ptp_data->t1.tv_nsec);
    dprintf("t2 %010ld.%09ld\n", ptp_data->t2.tv_sec, ptp_data->t2.tv_nsec);
    dprintf("t3 %010ld.%09ld\n", ptp_data->t3.tv_sec, ptp_data->t3.tv_nsec);
    dprintf("t4 %010ld.%09ld\n", ptp_data->t4.tv_sec, ptp_data->t4.tv_nsec);

    ptp_data->delta = calculate_delta(ptp_data);
    ptp_data->current_ptp_port = ptp_data->portid;

    sync_clock(ptp_data);
}

static void sync_clock(struct ptp_slave_data* ptp_data) {
    dprintf("Sync clock! delta: %+010ld\n", ptp_data->delta);

    if (ptp_data->delta > KERNEL_TIME_ADJUST_LIMIT || ptp_data->delta < -KERNEL_TIME_ADJUST_LIMIT) {
        dprintf("TOO FAR!!\n");
        struct timespec now;
        clock_gettime(CLOCK_TAI, &now);
        now.tv_nsec += ptp_data->delta % NS_PER_SEC;
        now.tv_sec += ptp_data->delta / NS_PER_SEC;
        if (now.tv_nsec < 0) {
            now.tv_sec -= 1;
            now.tv_nsec += NS_PER_SEC;
        } else if (now.tv_nsec >= NS_PER_SEC) {
            now.tv_sec += 1;
            now.tv_nsec -= NS_PER_SEC;
        }
        dprintf("Set time as %010ld.%09ld\n", now.tv_sec, now.tv_nsec);
        clock_settime(CLOCK_TAI, &now);
        // dprintf("Set time as %010ld.%09ld\n", ptp_data->t4.tv_sec, ptp_data->t4.tv_nsec);
        // clock_settime(CLOCK_REALTIME, &ptp_data->t4);
    } else {
        struct timex tx;
        memset(&tx, 0, sizeof(tx));
        tx.modes = ADJ_SETOFFSET | ADJ_NANO;
        tx.time.tv_sec = ptp_data->delta / NS_PER_SEC;
        tx.time.tv_usec = ptp_data->delta % NS_PER_SEC; // usec but nsec. lol
        if (tx.time.tv_usec < 0) {
            tx.time.tv_sec -= 1;
            tx.time.tv_usec += NS_PER_SEC;
            (void)ns_to_timeval;
        }
        adjtimex(&tx);
    }
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
