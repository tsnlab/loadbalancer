#include "port.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "timeutil.h"
#include "utils.h"

static inline struct queue* get_queue(struct port* port, int16_t prio) {
    return map_get(port->prio_queues, from_i16(prio));
}

void ports_init(struct port* ports, size_t count) {
    memset(ports, 0, sizeof(struct port) * count);

    struct credit_schedule cbs_schedules[PRIO_RANGE + 1];
    get_cbs_configs(cbs_schedules);

    for (int i = 0; i < count; i += 1) {
        ports[i].prio_queues = map_create(PRIO_RANGE + 1, int16_hash, NULL);
        assert(ports[i].prio_queues != NULL);

        pv_thread_lock_init(&ports[i].lock);

        for (int16_t prio = -1; prio < PRIO_RANGE; prio += 1) {
            struct queue* queue = malloc(sizeof(struct queue));
            assert(queue != NULL);
            memset(queue, 0, sizeof(struct queue));
            queue->pkts = list_create(NULL);

            pv_thread_lock_init(&queue->lock);

            if (cbs_schedules[prio_to_index(prio)].is_cbs) {
                queue->is_cbs = cbs_schedules[prio_to_index(prio)].is_cbs;
                queue->high_credit = cbs_schedules[prio_to_index(prio)].high_credit;
                queue->low_credit = cbs_schedules[prio_to_index(prio)].low_credit;
                queue->send_slope = cbs_schedules[prio_to_index(prio)].send_slope;
                queue->idle_slope = cbs_schedules[prio_to_index(prio)].idle_slope;
            }

            map_put(ports[i].prio_queues, from_i16(prio), queue);
        }
    }
}

void calculate_credits(struct port* port, int prio, int* credit, int* cbs_credit, const struct timespec* now) {
    struct queue* queue = get_queue(port, prio);
    size_t queue_size = port_queue_size(port, prio);

    int calculated_credit =
        minmax(port->queue_credits[prio_to_index(prio)] + 1, port_queue_lowcredit, port_queue_highcredit);
    port->queue_credits[prio_to_index(prio)] = calculated_credit;
    *credit = calculated_credit;

    if (queue->is_cbs) {
        struct timespec diff;
        timespec_diff(&queue->last_checked, now, &diff);

        int calculated_credits = (queue->idle_slope * (diff.tv_sec + ((double)diff.tv_nsec / 1000000000)));
        // Don't increase credit until reaches 0. because of inaccuracy of integer
        if (queue->cbs_credits + calculated_credits >= 0) {
            pv_thread_lock_write_lock(&queue->lock);
            queue->cbs_credits = minmax(queue->cbs_credits + calculated_credits,
                                        queue->low_credit,
                                        queue_size > 0 ? queue->high_credit : 0);
            queue->last_checked = *now;
            dprintf("credit + %d = %d\n", calculated_credits, queue->cbs_credits);
            pv_thread_lock_write_unlock(&queue->lock);
        }
    } else {
        *cbs_credit = -1;
    }
}

void spend_credit(struct port* port, int prio) {
    port->queue_credits[prio_to_index(prio)] =
        minmax(port->queue_credits[prio_to_index(prio)] - 2, port_queue_lowcredit, port_queue_highcredit);
}

size_t port_queue_size(struct port* port, int prio) {
    struct queue* queue = get_queue(port, prio);
    pv_thread_lock_read_lock(&queue->lock);
    size_t res = list_size(queue->pkts);
    pv_thread_lock_read_unlock(&queue->lock);
    return res;
}

bool port_push_tx(struct port* port, int prio, struct pv_packet* pkt) {
    struct queue* queue = get_queue(port, prio);
    pv_thread_lock_write_lock(&queue->lock);
    bool res = list_add(queue->pkts, pkt);
    pv_thread_lock_write_unlock(&queue->lock);

    if (res == false) {
        return false;
    }

    pv_thread_lock_write_lock(&port->lock);
    port->remaining_pkts += 1;
    pv_thread_lock_write_unlock(&port->lock);

    return true;
}

struct pv_packet* port_pop_tx(struct port* port, int prio) {
    struct queue* queue = get_queue(port, prio);
    return (struct pv_packet*)list_remove_at(queue->pkts, 0);
}

bool port_is_tx_empty(struct port* port) {
    pv_thread_lock_read_lock(&port->lock);
    int remaining = port->remaining_pkts;
    pv_thread_lock_read_unlock(&port->lock);

    return remaining == 0;
}
