#pragma once

#include <cl/map.h>

#include <stdint.h>
#include <time.h>

struct schedule {
    uint32_t window; // in ns
    uint32_t prios;  // 0 = BE, 1 = prio0, so on

    struct timespec until; // Updated on @see get_current_schedule
};

struct credit_schedule {
    bool is_cbs;
    int32_t high_credit;
    int32_t low_credit;
    int32_t idle_slope;
    int32_t send_slope;
};

size_t get_tas_schedules(struct schedule** schedules, uint32_t* total_window);
uint32_t map_prio(int prio);

void get_cbs_configs(struct credit_schedule* schedules);
