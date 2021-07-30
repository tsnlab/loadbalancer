#include <arpa/inet.h>
#include <pv/config.h>
#include <pv/net/ethernet.h>
#include <pv/net/ipv6.h>
#include <pv/net/vlan.h>
#include <pv/nic.h>
#include <pv/pv.h>

#include <cl/map.h>
#include <cl/list.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "nat.h"
#include "net.h"

void print_map(nat_map* nat);

bool running = true;
uint64_t mymac;

struct schedule {
    uint32_t window; // in ns
    uint32_t prios;  // 0 = BE, 1 = prio0, so on
};

static uint32_t map_prio(int prio) {
    switch (prio) {
    case -1:
        return 0;
    default:
        return 1 << (prio + 1);
    }
}

struct map* prio_queue;
struct list* schedules;

void process(struct pv_packet* pkt);
void enqueue(struct pv_packet* pkt, int prio);

void process_queue();

static void handle_signal(int signo) {
    switch (signo) {
    case SIGINT:
        printf("SIGINT\n");
        break;
    case SIGTERM:
        printf("SIGINT\n");
        break;
    }
    running = false;
}

static struct pv_ethernet* get_ether(struct pv_packet* pkt) {
    return (struct pv_ethernet*)(pkt->buffer + pkt->start);
}

int main(int argc, const char* argv[]) {
    if(pv_init() != 0) {
        fprintf(stderr, "Cannot initialize packetvisor\n");
        exit(1);
    }

    struct pv_packet* pkts[512];
    const int max_pkts = sizeof(pkts) / sizeof(pkts[0]);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);


    // Setup prio map
    prio_queue = map_create(0x1000, uint16_hash, NULL);
    for (uint16_t prio = -1; prio <= 0x0fff; prio += 1) {
        struct list* list = list_create(NULL);
        map_put(prio_queue, from_u16(prio), list);
    }

    // Setup schedules
    schedules = list_create(NULL);
    // XXX: Use extended structure to store priorities
    // 0 = BE, 1 = VLAN prio 0
    struct schedule schs[] = {
        {.window = 300000, .prios = map_prio(3)},
        {.window = 300000, .prios = map_prio(3) | map_prio(2)},
        {.window = 400000, .prios = map_prio(-1)},
    };
    const int schs_len = sizeof(schs) / sizeof(schs[0]);
    for(int i = 0; i < schs_len; i += 1) {
        struct schedule* sch = malloc(sizeof(struct schedule));
        memcpy((void*)sch, (void*)&schs[i], sizeof(struct schedule));
        list_add(schedules, sch);
    }

    // Get NIC's mac
    mymac = pv_nic_get_mac(0);

    while(running) {
        uint16_t count = pv_nic_rx_burst(0, 0, pkts, max_pkts);

        if(count == 0) {
            // TODO: Check if queue is empty
            usleep(100);
            continue;
        }

        for(uint16_t i = 0; i < count; i++) {
            process(pkts[i]);
        }

        process_queue();
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
    case PV_ETH_TYPE_VLAN:
        ;
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

void process_queue() {
    // TODO: implement this;
}
