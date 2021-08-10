#include <pv/config.h>

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t get_tas_schedules(struct schedule** schedules, uint32_t* total_window) {
    const char prefix[] = "/loadbalancer/tas";
    const size_t schedule_count = pv_config_get_size(prefix);
    if (schedule_count <= 0) {
        return 0;
    }

    *schedules = (struct schedule*)calloc(schedule_count, sizeof(struct schedule));
    if (*schedules == NULL) {
        return 0;
    }

    for (int i = 0; i < schedule_count; i += 1) {
        const int key_size = strlen(prefix) + 4 /* [dd] */ + strlen("/prios");
        char key_schedule[key_size];
        snprintf(key_schedule, key_size, "%s[%d]/prios", prefix, i);
        const size_t prio_count = pv_config_get_size(key_schedule);

        int prio_inlined = 0;

        for (int j = 0; j < prio_count; j += 1) {
            char key_prio[key_size + 8];
            snprintf(key_prio, sizeof(key_prio), "%s[%d]", key_schedule, j);
            int prio = pv_config_get_num(key_prio);
            prio_inlined |= map_prio(prio);
        }

        // schedules[i]->prios is incorrect. use (*schedules)[i].prios
        struct schedule* sch = &(*schedules)[i];

        sch->prios = prio_inlined;
        char key_time[key_size + 8];
        snprintf(key_time, sizeof(key_time), "%s[%d]/time", prefix, i);
        sch->window = pv_config_get_num(key_time);

        *total_window += sch->window;
    }

    return schedule_count;
}

uint32_t map_prio(int prio) {
    // BE = 0b1
    // pri-0 = 0b10, pri-1 = 0b100
    switch (prio) {
    case -1:
        return 1;
    default:
        return 1 << (prio + 1);
    }
}

struct map* get_cbs_configs() {
    const char prefix[] = "/loadbalancer/cbs";
    // TODO: check if cbs config exists

    const size_t schedule_count = pv_config_get_size(prefix);
    if (schedule_count <= 0) {
        return NULL;
    }

    struct map* cbs_map = map_create(4, uint8_hash, NULL);
    if (cbs_map == NULL) {
        // This is an error
        return NULL;
    }

    for (int i = 0; i < schedule_count; i += 1) {

        // Get the key first
        const uint8_t prio = atoi(pv_config_get_key(prefix, i));

        const int key_size = strlen(prefix) + 3 /* /dd */ + strlen("/send") + 1;
        char subkey[key_size];

        struct credit_schedule* sch = malloc(sizeof(struct credit_schedule));
        assert(sch != NULL);

        snprintf(subkey, key_size, "%s/%d/high", prefix, prio);
        sch->high_credit = pv_config_get_num(subkey);
        snprintf(subkey, key_size, "%s/%d/low", prefix, prio);
        sch->low_credit = pv_config_get_num(subkey);
        snprintf(subkey, key_size, "%s/%d/idle", prefix, prio);
        sch->idle_slope = pv_config_get_num(subkey);
        snprintf(subkey, key_size, "%s/%d/send", prefix, prio);
        sch->send_slope = pv_config_get_num(subkey);

        sch->current_credit = 0;
        clock_gettime(CLOCK_REALTIME, &sch->last_checked);

        map_put(cbs_map, from_u8(prio), (void*)sch);
    }

    return cbs_map;
}
