#include <stdint.h>
#include <time.h>

struct schedule {
    uint32_t window; // in ns
    uint32_t prios;  // 0 = BE, 1 = prio0, so on

    struct timespec until; // Updated on @see get_current_schedule
};

size_t get_tas_schedules(struct schedule** schedules);
uint32_t map_prio(int prio);
