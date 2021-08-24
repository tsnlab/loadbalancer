#include <pv/config.h>

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port.h"

size_t get_tas_schedules(struct schedule** schedules, uint32_t* total_window) {
    const char prefix[] = "/loadbalancer/tas";
    const int schedule_count = pv_config_get_size(prefix);
    if (schedule_count <= 0) {
        printf("No TAS\n");
        return 0;
    }

    *schedules = (struct schedule*)calloc(schedule_count, sizeof(struct schedule));
    if (*schedules == NULL) {
        return 0;
    }

    for (unsigned int i = 0; i < schedule_count; i += 1) {
        const int key_size = strlen(prefix) + 4 /* [dd] */ + strlen("/prios");
        char key_schedule[key_size];
        snprintf(key_schedule, key_size, "%s[%u]/prios", prefix, i);
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

void get_cbs_configs(struct credit_schedule* schedules) {
    const char prefix[] = "/loadbalancer/cbs";

    if (pv_config_get_type(prefix) != PV_CONFIG_DICT) {
        return;
    }

    const size_t schedule_count = pv_config_get_size(prefix);
    if (schedule_count <= 0) {
        return;
    }

    memset(schedules, 0, sizeof(struct credit_schedule) * (PRIO_RANGE + 1));

    for (unsigned int i = 0; i < schedule_count; i += 1) {

        // Get the key first
        const int8_t prio = atoi(pv_config_get_key(prefix, i));
        printf("Prio: %d\n", prio);
        int index = prio_to_index(prio);

        schedules[index].is_cbs = true;

        const int key_size = strlen(prefix) + 5 /* /-dd */ + strlen("/send") + 1;
        char subkey[key_size];

        snprintf(subkey, key_size, "%s/%d/high", prefix, prio);
        schedules[index].high_credit = pv_config_get_num(subkey);
        snprintf(subkey, key_size, "%s/%d/low", prefix, prio);
        schedules[index].low_credit = pv_config_get_num(subkey);
        snprintf(subkey, key_size, "%s/%d/idle", prefix, prio);
        schedules[index].idle_slope = pv_config_get_num(subkey);
        snprintf(subkey, key_size, "%s/%d/send", prefix, prio);
        schedules[index].send_slope = pv_config_get_num(subkey);
    }
}
