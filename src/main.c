#include <cl/list.h>
#include <cl/map.h>
#include <pv/config.h>
#include <pv/net/ethernet.h>
#include <pv/net/ipv6.h>
#include <pv/net/vlan.h>
#include <pv/nic.h>
#include <pv/pv.h>
#include <pv/thread.h>

#include <arpa/inet.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "port.h"
#include "timeutil.h"
#include "utils.h"

// clang-format off
// #define dprintf printf
#define dprintf(...) do{}while(false)
// clang-format on

bool running = true;
uint64_t mymac;

struct map* prio_queue; // prio => list<pv_packet>
struct pv_thread_lock queue_locks[PRIO_RANGE + 1];
#define MAX_QUEUE_SIZE 2048 // TODO: Set this value from user config

// TAS related configs
struct schedule* schedules;
size_t schedules_size = 0;
uint32_t total_window = 0;
int credits[PRIO_RANGE + 1] = {
    0,
};

// CBS related configs
// map<prio: int8_t, struct credit_schedule>
struct map* cbs_schedules = NULL;

static void prio_write_lock(int prio) {
    pv_thread_lock_write_lock(&queue_locks[prio + 1]);
}

static void prio_write_unlock(int prio) {
    pv_thread_lock_write_unlock(&queue_locks[prio + 1]);
}

static void prio_read_lock(int prio) {
    pv_thread_lock_read_lock(&queue_locks[prio + 1]);
}

static void prio_read_unlock(int prio) {
    pv_thread_lock_read_unlock(&queue_locks[prio + 1]);
}

void process(struct pv_packet* pkt);
void enqueue(struct pv_packet* pkt, int prio);

struct schedule* get_current_schedule();
uint16_t process_queue();

int select_queue(int prios, struct list** queue);

static void handle_signal(int signo) {
    switch (signo) {
    case SIGINT:
        printf("SIGINT\n");
        break;
    case SIGTERM:
        printf("SIGTERM\n");
        break;
    }
    running = false;
}

static struct pv_ethernet* get_ether(struct pv_packet* pkt) {
    return (struct pv_ethernet*)PV_PACKET_PAYLOAD(pkt);
}

static int read_loop(void* _dummy) {
    printf("Starting Reader on %d!\n", pv_thread_core_id());

    struct pv_packet* pkts[64];
    const int max_pkts = sizeof(pkts) / sizeof(pkts[0]);

    while (running) {
        uint16_t read_count_a = pv_nic_rx_burst(0, 0, pkts, max_pkts);

        for (uint16_t i = 0; i < read_count_a; i++) {
            process(pkts[i]);
        }

        uint16_t read_count_b = pv_nic_rx_burst(1, 0, pkts, max_pkts);

        for (uint16_t i = 0; i < read_count_b; i++) {
            process(pkts[i]);
        }
    }

    printf("Ending reader!\n");

    return 0;
}

static int write_loop(void* _dummy) {
    printf("Starting writer on %d!\n", pv_thread_core_id());

    while (running) {
        uint16_t write_count = process_queue();
        (void)write_count;
    }

    printf("Ending writer!\n");

    return 0;
}

static int main_loop(void* _dummy) {
    int coreid = pv_thread_core_id();

    switch (coreid) {
    case 0:
        write_loop(_dummy);
        break;
    case 1:
        read_loop(_dummy);
        break;
    default:
        printf("I'm useless %d\n", coreid);
    }

    return 0;
}

int main(int argc, const char* argv[]) {
    if (pv_init() != 0) {
        fprintf(stderr, "Cannot initialize packetvisor\n");
        exit(1);
    }

    if (pv_nic_count() != 2) {
        fprintf(stderr, "Need 2 NICs\n");
        exit(1);
    }

    int cores[2];
    int cores_count = pv_config_get_cores(cores, 2);
    if (cores_count < 2) {
        fprintf(stderr, "Need at least 2 cores\n");
        exit(1);
    }

    // Setup prio map
    prio_queue = map_create(PRIO_RANGE + 1, int16_hash, NULL);
    for (int16_t prio = -1; prio < PRIO_RANGE; prio += 1) {
        struct list* list = list_create(NULL);
        map_put(prio_queue, from_i16(prio), list);
        pv_thread_lock_init(&queue_locks[prio + 1]);
    }

    // Setup schedules
    schedules_size = get_tas_schedules(&schedules, &total_window);
    printf("TAS %ld\n", schedules_size);

    cbs_schedules = get_cbs_configs();

    // Get NIC's mac
    mymac = pv_nic_get_mac(0);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Start processing

    // (void)main_loop;
    // int res_writer = pv_thread_run_at(write_loop, NULL, cores[0]);
    // int res_reader = pv_thread_run_at(read_loop, NULL, cores[1]);
    // if (res_writer != 0 || res_reader != 0) {
    //     fprintf(stderr, "Failed to start threads\n");
    //     exit(1);
    // }

    // pv_thread_wait_all();

    pv_thread_run_all(main_loop, NULL, true);
    pv_thread_wait_all();

    pv_finalize();
    return 0;
}

void process(struct pv_packet* pkt) {
    struct pv_ethernet* ether = get_ether(pkt);

    int prio;

    if (ether->type == PV_ETH_TYPE_VLAN) {
        struct pv_vlan* vlan = PV_ETH_PAYLOAD(ether);
        prio = vlan->priority;
    } else {
        prio = -1;
    }

    // Just forward to another port
    switch (pkt->nic_id) {
    case 0:
        pkt->nic_id = 1;
        break;
    case 1:
        pkt->nic_id = 0;
        break;
    }
    // TODO: calculate checksums here

    enqueue(pkt, prio);
}

void enqueue(struct pv_packet* pkt, int prio) {
    struct list* queue = map_get(prio_queue, from_i16(prio));
    assert(queue != NULL);
    prio_read_lock(prio);
    size_t q_len = list_size(queue);
    prio_read_unlock(prio);
    if (q_len >= MAX_QUEUE_SIZE) {
        dprintf("Drop pkt prio %d\n", prio);
        pv_packet_free(pkt);
    } else {
        prio_write_lock(prio);
        list_add(queue, pkt);
        prio_write_unlock(prio);
    }
}

struct schedule* get_current_schedule() {
    struct timespec now;

    if (schedules_size == 0) {
        return NULL;
    }

    clock_gettime(CLOCK_REALTIME, &now);

    uint64_t now_u = now.tv_sec * 1000000000 + now.tv_nsec;
    uint64_t mod = now_u % total_window;

    int sum = 0;
    for (int i = 0; i < schedules_size; i += 1) {
        struct schedule* sch = &schedules[i];
        sum += sch->window;
        if (sum > mod) {
            sch->until.tv_sec = now.tv_sec;
            sch->until.tv_nsec += sum - mod; // FIXME: use tv_sec also.
            return sch;
        }
    }

    // Never reach here
    return NULL;
}

uint16_t process_queue() {
    uint16_t count = 0;
    const struct schedule* current_schedule = get_current_schedule();
    struct timespec now;

    struct timespec until;
    if (current_schedule != NULL) {
        until = current_schedule->until;
    } else {
        // There is no TAS
        clock_gettime(CLOCK_REALTIME, &now);
        until = now;
        until.tv_nsec += 500000;
        if (until.tv_nsec >= 1000000000) {
            until.tv_sec += 1;
            until.tv_nsec -= 1000000000;
        }
    }

    do {
        struct pv_packet* pkt = NULL;
        struct list* best_queue;
        int prio;
        if (current_schedule != NULL) {
            prio = select_queue(current_schedule->prios, &best_queue);
        } else {
            prio = select_queue(PRIOS_ALL, &best_queue);
        }

        if (best_queue == NULL) {
            break;
        }

        prio_write_lock(prio);
        pkt = list_remove_at(best_queue, 0);
        prio_write_unlock(prio);

        if (pkt == NULL) {
            dprintf("Impossible, maybe concurrent error\n");
            break;
        }

        dprintf("There are pkt\n");

        struct credit_schedule* cbs_sch = map_get(cbs_schedules, from_i8(prio));
        if (cbs_sch != NULL) {
            dprintf("This is cbs enabled queue\n");
            // This is cbs scheduled queue.
            size_t speed = 1000000000; // FIXME: use proper setting from NIC
            int calculated_credits = (double)cbs_sch->send_slope / speed * PV_PACKET_PAYLOAD_LEN(pkt) * 8;
            prio_write_lock(prio);
            cbs_sch->current_credit =
                minmax(cbs_sch->current_credit += calculated_credits, cbs_sch->low_credit, cbs_sch->high_credit);
            dprintf("credit - %d = %d\n", calculated_credits, cbs_sch->current_credit);
            prio_write_unlock(prio);
        }

        int sent = pv_nic_tx(pkt->nic_id, 0, pkt);
        if (sent == 0) {
            pv_packet_free(pkt);
            dprintf("NOT SENT!!!!!\n");
        }
        count += sent;

        clock_gettime(CLOCK_REALTIME, &now);
        if (cbs_sch != NULL) { // Was cbs queue
            cbs_sch->last_checked = now;
        }
    } while (timespec_compare(&until, &now) > 0);

    return count;
}

int select_queue(int prios, struct list** queue) {
    const int low = -5;
    const int high = 10;
    struct list* best = NULL;
    int best_pri = -2;
    int best_credit = low - 1;

    int best_cbs_credit = -1;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    for (int pri = -1; pri < PRIO_RANGE; pri += 1) {
        if (map_prio(pri) & prios) {
            struct credit_schedule* cbs_sch = map_get(cbs_schedules, from_i8(pri));
            struct list* queue = map_get(prio_queue, from_i16(pri));
            assert(queue != NULL);
            prio_read_lock(pri);
            size_t queue_size = list_size(queue);
            prio_read_unlock(pri);

            if (cbs_sch != NULL) {
                if (queue_size == 0) {
                    // Case 1. There are no queued packet.
                    cbs_sch->current_credit = minmax(cbs_sch->current_credit, cbs_sch->low_credit, 0);
                    cbs_sch->last_checked = now;
                } else {
                    // Case 2. calculate waited credits
                    struct timespec diff;
                    timespec_diff(&cbs_sch->last_checked, &now, &diff);

                    int calculated_credits =
                        (cbs_sch->idle_slope * (diff.tv_sec + ((double)diff.tv_nsec / 1000000000)));
                    // Don't increase credit until reaches 0. because of inaccuracy of integer
                    if (cbs_sch->current_credit + calculated_credits >= 0) {
                        prio_write_lock(pri);
                        cbs_sch->current_credit = minmax(cbs_sch->current_credit + calculated_credits,
                                                         cbs_sch->low_credit,
                                                         cbs_sch->high_credit);
                        cbs_sch->last_checked = now;
                        dprintf("credit + %d = %d\n", calculated_credits, cbs_sch->current_credit);
                        prio_write_unlock(pri);
                    }
                }
            }

            if (cbs_sch != NULL) {
                if (cbs_sch->current_credit >= 0 && cbs_sch->current_credit >= best_cbs_credit && queue_size > 0) {
                    best_pri = pri;
                    best_credit = credits[pri + 1];
                    best = queue;
                    best_cbs_credit = cbs_sch->current_credit;
                }
            } else {
                if (best_cbs_credit == -1 && (best == NULL || best_credit <= credits[pri + 1]) && queue_size > 0) {
                    best_pri = pri;
                    best_credit = credits[pri + 1];
                    best = queue;
                }
            }
        }
        credits[pri + 1] = minmax(credits[pri + 1] + 1, low, high);
    }

    if (best != NULL) {
        credits[best_pri + 1] -= 2;
    }

    *queue = best;
    return best_pri;
}
