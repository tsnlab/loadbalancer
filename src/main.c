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

bool running = true;

int port_count;
struct port* ports;
#define MAX_QUEUE_SIZE 2048 // TODO: Set this value from user config

// TAS related configs
struct schedule* schedules;
size_t schedules_size = 0;
uint32_t total_window = 0;

void process(struct pv_packet* pkt);
void enqueue(struct pv_packet* pkt, int portid, int prio);

struct schedule* get_current_schedule();
uint16_t process_queue();

bool select_queue(int prios, int* selected_portid, int* selected_prio);

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

static inline struct pv_ethernet* get_ether(struct pv_packet* pkt) {
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

    port_count = pv_nic_count();

    if (port_count < 2) {
        fprintf(stderr, "Need at least 2 NICs\n");
        exit(1);
    }

    int cores[2];
    int cores_count = pv_config_get_cores(cores, 2);
    if (cores_count < 2) {
        fprintf(stderr, "Need at least 2 cores\n");
        exit(1);
    }

    ports = calloc(sizeof(struct port), port_count);
    ports_init(ports, port_count);

    // Setup schedules
    schedules_size = get_tas_schedules(&schedules, &total_window);
    printf("TAS %ld\n", schedules_size);

    // Start processing

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

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

    // FIXME: use CAM table
    int portid = (pkt->nic_id + 1) % port_count;

    enqueue(pkt, portid, prio);
}

void enqueue(struct pv_packet* pkt, int portid, int prio) {
    if (port_queue_size(&ports[portid], prio) > MAX_QUEUE_SIZE) {
        dprintf("Drop pkt toport %d prio %d\n", portid, prio);
        pv_packet_free(pkt);
    } else {
        pkt->nic_id = portid;
        port_push_tx(&ports[portid], prio, pkt);
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
        bool queue_available;
        int portid, prio;
        if (current_schedule != NULL) {
            queue_available = select_queue(current_schedule->prios, &portid, &prio);
        } else {
            queue_available = select_queue(PRIOS_ALL, &portid, &prio);
        }

        if (!queue_available) {
            break;
        }

        dprintf("selected port: %d, prio: %d\n", portid, prio);

        // pull from queue
        pkt = port_pop_tx(&ports[portid], prio);

        if (pkt == NULL) {
            dprintf("Impossible, maybe concurrent error\n");
            break;
        }

        dprintf("There are pkt\n");

        size_t pkt_bytes = PV_PACKET_PAYLOAD_LEN(pkt);

        int sent = pv_nic_tx(portid, 0, pkt);
        if (sent == 0) {
            pv_packet_free(pkt);
            dprintf("NOT SENT!!!!!\n");
        }
        count += sent;

        clock_gettime(CLOCK_REALTIME, &now);
        if (sent > 0) {
            spend_cbs_credit(&ports[portid], prio, pkt_bytes, &now);
        }
    } while (timespec_compare(&until, &now) > 0);

    return count;
}

bool select_queue(int prios, int* selected_portid, int* selected_prio) {

    bool best_normal_queue = false;
    bool best_cbs_queue = false;

    int best_credit = -1;
    int best_port = -1;
    int best_pri = -2;

    int best_cbs_credit = -1;
    int best_cbs_port = -1;
    int best_cbs_pri = -2;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    for (int portid = 0; portid < port_count; portid += 1) {
        for (int pri = -1; pri < PRIO_RANGE; pri += 1) {
            if (map_prio(pri) & prios) {

                int credit, cbs_credit;
                calculate_credits(&ports[portid], pri, &credit, &cbs_credit, &now);

                size_t queue_size = port_queue_size(&ports[portid], pri);

                if (queue_size > 0) {
                    if (cbs_credit != -1) {
                        if (best_cbs_queue == false || cbs_credit > best_cbs_credit) {
                            dprintf("Got cbs best: %d %d\n", portid, pri);
                            best_cbs_queue = true;
                            best_cbs_credit = cbs_credit;
                            best_cbs_port = portid;
                            best_cbs_pri = pri;
                        }
                    } else {
                        if (best_normal_queue == false || credit > best_credit) {
                            dprintf("Got normal best: %d %d\n", portid, pri);
                            best_normal_queue = true;
                            best_credit = credit;
                            best_port = portid;
                            best_pri = pri;
                        }
                    }
                }
            }
        }
    }

    if (best_cbs_queue) {
        *selected_portid = best_cbs_port;
        *selected_prio = best_cbs_pri;
        spend_credit(&ports[best_cbs_port], best_cbs_pri);
        return best_cbs_queue;
    } else {
        *selected_portid = best_port;
        *selected_prio = best_pri;
        spend_credit(&ports[best_port], best_pri);
        return best_normal_queue;
    }
}
