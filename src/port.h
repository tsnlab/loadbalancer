#pragma once

#include <cl/map.h>
#include <pv/packet.h>
#include <pv/thread.h>

#include <time.h>

#define PRIO_RANGE 0x8
#define PRIOS_ALL (~0)

#define port_queue_lowcredit -5
#define port_queue_highcredit 10

/**
 * BE(-1) => 0
 * prio(0) => 1
 */
inline int prio_to_index(int prio) {
    return prio + 1;
}

inline int index_to_prio(int index) {
    return index - 1;
}

struct port {
    struct pv_thread_lock lock;
    // map<prio: int, queue: struct queue>
    struct map* prio_queues;
    // Not cbs, just prevent starvation.
    int queue_credits[PRIO_RANGE + 1];

    size_t remaining_pkts;
};

struct queue {
    struct pv_thread_lock lock;

    struct list* pkts;

    bool is_cbs;
    int32_t high_credit;
    int32_t low_credit;
    int32_t send_slope;
    int32_t idle_slope;

    int32_t cbs_credits;
    struct timespec last_checked;
};

void ports_init(struct port* ports, size_t count);

void calculate_credits(struct port* port, int prio, int* credit, int* cbs_credit, const struct timespec* now);

size_t port_queue_size(struct port* port, int prio);
bool port_push_tx(struct port* port, int prio, struct pv_packet* pkt);
struct pv_packet* port_pop_tx(struct port* port, int prio);

bool port_is_tx_empty(struct port* port);
