#include "port.h"

#include <assert.h>
#include <string.h>

void ports_init(struct port* ports, size_t count) {
    memset(ports, 0, sizeof(struct port) * count);

    for (int i = 0; i < count; i += 1) {
        ports[i].prio_queues = map_create(PRIO_RANGE + 1, int16_hash, NULL);
        assert(ports[i].prio_queues != NULL);

        for (int16_t prio = -1; prio < PRIO_RANGE; prio += 1) {
            struct queue* queue = malloc(sizeof(struct queue));
            assert(queue != NULL);
            memset(queue, 0, sizeof(struct queue));
            queue->pkts = list_create(NULL);
            map_put(ports[i].prio_queues, prio, queue);
        }
    }
}
