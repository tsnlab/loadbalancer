#include <pv/config.h>

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t get_tas_schedules(struct schedule** schedules) {
    const char prefix[] = "/loadbalancer/tas";
    const size_t schedule_count = pv_config_get_size(prefix);
    if (schedule_count <= 0) {
        return -1;
    }

    *schedules = (struct schedule*)calloc(schedule_count, sizeof(struct schedule));
    if (*schedules == NULL) {
        return -1;
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
