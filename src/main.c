#include <cl/list.h>
#include <cl/map.h>
#include <pv/config.h>
#include <pv/net/ethernet.h>
#include <pv/net/ipv6.h>
#include <pv/net/vlan.h>
#include <pv/nic.h>
#include <pv/pv.h>

#include <arpa/inet.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "nat.h"
#include "net.h"
#include "timeutil.h"
#include "utils.h"

#define PRIO_RANGE 0x8

#define dprintf printf
// #define dprintf(...) do{}while(false)

void print_map(nat_map* nat);

bool running = true;
uint64_t mymac;

struct map* prio_queue; // prio => list<pv_packet>

// TAS related configs
struct schedule* schedules;
size_t schedules_size = 0;
uint32_t total_window = 0;
int credits[PRIO_RANGE + 1] = {
    0,
};

// CBS related configs
// map<prio: uint8_t, struct credit_schedule>
struct map* cbs_schedules = NULL;

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
    return (struct pv_ethernet*)(pkt->buffer + pkt->start);
}

int main(int argc, const char* argv[]) {
    if (pv_init() != 0) {
        fprintf(stderr, "Cannot initialize packetvisor\n");
        exit(1);
    }

    struct pv_packet* pkts[512];
    const int max_pkts = sizeof(pkts) / sizeof(pkts[0]);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Setup prio map
    prio_queue = map_create(PRIO_RANGE + 1, int16_hash, NULL);
    for (int16_t prio = -1; prio < PRIO_RANGE; prio += 1) {
        struct list* list = list_create(NULL);
        map_put(prio_queue, from_i16(prio), list);
    }

    // Setup schedules
    schedules_size = get_tas_schedules(&schedules, &total_window);

    cbs_schedules = get_cbs_configs();

    // Get NIC's mac
    mymac = pv_nic_get_mac(0);

    while (running) {
        uint16_t read_count = pv_nic_rx_burst(0, 0, pkts, max_pkts);

        printf("Read %d pkts\n", read_count);

        for (uint16_t i = 0; i < read_count; i++) {
            process(pkts[i]);
        }

        printf("pkts processed\n");

        uint16_t write_count = process_queue();

        if (read_count == 0 && write_count == 0) {
            usleep(100);
        }
    }

    pv_finalize();
    return 0;
}

void process(struct pv_packet* pkt) {
    struct pv_ethernet* ether = get_ether(pkt);

    if (ether->dmac != mymac && ether->dmac == 0xffffffffffff) {
        pv_packet_free(pkt);
        return;
    }

    int prio;

    if (ether->type != PV_ETH_TYPE_VLAN) {
        pv_packet_free(pkt);
        return;
    }

    struct pv_vlan* vlan = PV_ETH_PAYLOAD(ether);
    prio = vlan->priority;

    if (vlan->type != 0x1337) {
        pv_packet_free(pkt);
        return;
    }

    // Return to sender
    ether->dmac = ether->smac;
    ether->smac = mymac;
    // TODO: calculate checksums here

    enqueue(pkt, prio);
}

void enqueue(struct pv_packet* pkt, int prio) {
    struct list* queue = map_get(prio_queue, from_i16(prio));
    assert(queue != NULL);

    struct credit_schedule* sch = map_get(cbs_schedules, from_u8(prio));
    if (sch != NULL) {
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        sch->last_checked = now;
    }

    list_add(queue, pkt);
}

struct schedule* get_current_schedule() {
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);

    uint64_t now_u = now.tv_sec * 1000000000 + now.tv_nsec;
    uint64_t mod = now_u % total_window;

    int sum = 0;
    dprintf("Lets check %ld schedules\n", schedules_size);
    for (int i = 0; i < schedules_size; i += 1) {
        dprintf("check schedule %d\n", i);
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
    dprintf("Current window is for %x\n", current_schedule->prios);

    do {
        struct pv_packet* pkt = NULL;
        struct list* best_queue;
        int prio = select_queue(current_schedule->prios, &best_queue);

        if (best_queue == NULL) {
            break;
        }

        dprintf("Selected prio = %d\n", prio);

        pkt = list_remove_at(best_queue, 0);

        if (pkt == NULL) {
            dprintf("Pkt is null\n");
            break;
        }

        dprintf("There are pkt\n");

        struct credit_schedule* cbs_sch = map_get(cbs_schedules, from_u8(prio));
        if (cbs_sch != NULL) {
            dprintf("This is cbs enabled queue\n");
            // This is cbs scheduled queue.
            size_t speed = 1000000000; // FIXME: use proper setting from NIC
            int calculated_credits = cbs_sch->send_slope * PV_PACKET_PAYLOAD_LEN(pkt) * 8 / speed;
            cbs_sch->current_credit =
                minmax(cbs_sch->current_credit -= calculated_credits, cbs_sch->low_credit, cbs_sch->high_credit);
        }

        dprintf("Send packet\n");
        pv_nic_tx(0, 0, pkt);
        count += 1;

        clock_gettime(CLOCK_REALTIME, &now);
        if (cbs_sch != NULL) { // Was cbs queue
            cbs_sch->last_checked = now;
        }
    } while (timespec_compare(&current_schedule->until, &now) > 0);

    return count;
}

int select_queue(int prios, struct list** queue) {
    const int low = -5;
    const int high = 10;
    struct list* best = NULL;
    int best_pri;
    int best_credit;

    int best_cbs_credit = -1;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    for (int pri = -1; pri < PRIO_RANGE; pri += 1) {
        if (map_prio(pri) & prios) {
            struct credit_schedule* cbs_sch = map_get(cbs_schedules, from_u8(pri));
            struct list* queue = map_get(prio_queue, from_i16(pri));
            assert(queue != NULL);

            if (cbs_sch != NULL) {
                if (list_size(queue) == 0) {
                    // Case 1. There are no queued packet.
                    cbs_sch->current_credit = minmax(cbs_sch->current_credit, cbs_sch->low_credit, 0);
                    cbs_sch->last_checked = now;
                } else {
                    // Case 2. calculate waited credits
                    struct timespec diff;
                    timespec_diff(&cbs_sch->last_checked, &now, &diff);

                    int calculated_credit = (cbs_sch->idle_slope * (diff.tv_sec + ((double)diff.tv_nsec / 1000000000)));
                    cbs_sch->current_credit =
                        minmax(cbs_sch->current_credit + calculated_credit, cbs_sch->low_credit, cbs_sch->high_credit);
                    cbs_sch->last_checked = now;
                }
            }

            if (pri >= 0 && cbs_sch != NULL && cbs_sch->current_credit >= 0 &&
                cbs_sch->current_credit >= best_cbs_credit) {
                if (list_size(queue) > 0) {
                    best_pri = pri;
                    best_credit = credits[pri + 1];
                    best = queue;
                }
            } else if (best == NULL || best_credit <= credits[pri + 1]) {
                if (list_size(queue) > 0) {
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
