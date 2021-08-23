#pragma once

#include <cl/map.h>
#include <pv/thread.h>

#include <time.h>

#define PRIO_RANGE 0x8
#define PRIOS_ALL (~0)

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
    // map<prio: int, queue: struct queue>
    struct map* prio_queues;
    // Not cbs, just prevent starvation.
    int queue_credits[PRIO_RANGE + 1];

    volatile size_t remaining_pkts;
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
