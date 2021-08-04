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
struct schedule* schedules;
size_t schedules_size = 0;
uint32_t total_window = 0;
int credits[PRIO_RANGE + 1] = {
    0,
};

void process(struct pv_packet* pkt);
void enqueue(struct pv_packet* pkt, int prio);

struct schedule* get_current_schedule();
uint16_t process_queue();

struct list* select_queue(struct map* queues, int prios);

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
    prio_queue = map_create(PRIO_RANGE, uint16_hash, NULL);
    for (uint16_t prio = -1; prio < PRIO_RANGE; prio += 1) {
        struct list* list = list_create(NULL);
        map_put(prio_queue, from_u16(prio), list);
    }

    // Setup schedules
    schedules_size = get_tas_schedules(&schedules);

    // Get NIC's mac
    mymac = pv_nic_get_mac(0);

    while (running) {
        uint16_t read_count = pv_nic_rx_burst(0, 0, pkts, max_pkts);

        for (uint16_t i = 0; i < read_count; i++) {
            process(pkts[i]);
        }

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
    }

    int prio;

    switch (ether->type) {
    case PV_ETH_TYPE_VLAN:;
        struct pv_vlan* vlan = PV_ETH_PAYLOAD(ether);
        prio = vlan->priority;
        break;
    default:
        prio = -1;
        break;
    }

    // Return to sender
    ether->dmac = ether->smac;
    ether->smac = mymac;
    // TODO: calculate checksums here

    enqueue(pkt, prio);
}

void enqueue(struct pv_packet* pkt, int prio) {
    struct list* queue = map_get(prio_queue, from_u16(prio));
    assert(queue != NULL);

    list_add(queue, pkt);
}

struct schedule* get_current_schedule() {
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);

    uint64_t now_u = now.tv_sec * 1000000000 + now.tv_nsec;
    uint64_t mod = now_u % total_window;

    int sum = 0;
    for (int i = 0; i < schedules_size; i += 1) {
        struct schedule* sch = &schedules[i];
        sum += sch->window;
        if (sum > mod) {
            sch->until.tv_sec = now.tv_sec;
            sch->until.tv_nsec += sum - mod; // FIXME
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
        struct list* best_queue = select_queue(prio_queue, current_schedule->prios);

        if (best_queue == NULL) {
            break;
        }

        pkt = list_remove_at(best_queue, 0);

        if (pkt == NULL) {
            dprintf("Pkt is null\n");
            break;
        }

        dprintf("Send packet\n");
        pv_nic_tx(0, 0, pkt);
        count += 1;

        clock_gettime(CLOCK_REALTIME, &now);
    } while (timespec_compare(&current_schedule->until, &now) > 0);

    return count;
}

struct list* select_queue(struct map* queues, int prios) {
    const int low = -5;
    const int high = 10;
    struct list* best = NULL;
    int best_pri;
    int best_credit;

    for (int pri = -1; pri < PRIO_RANGE; pri += 1) {
        if (map_prio(pri) & prios) {
            if (best == NULL || best_credit <= credits[pri + 1]) {
                struct list* list = map_get(queues, from_u16(pri));
                if (list_size(best) > 0) {
                    best_pri = pri;
                    best_credit = credits[pri + 1];
                    best = list;
                }
            }
        }
        credits[pri + 1] = minmax(credits[pri + 1] + 1, low, high);
    }

    if (best != NULL) {
        credits[best_pri + 1] -= 2;
    }

    return best;
}
