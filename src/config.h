#include <cl/map.h>

#include <stdint.h>
#include <time.h>

struct schedule {
    uint32_t window; // in ns
    uint32_t prios;  // 0 = BE, 1 = prio0, so on

    struct timespec until; // Updated on @see get_current_schedule
};

struct credit_schedule {
    int16_t high_credit;
    int16_t low_credit;
    int16_t idle_slope;
    int16_t send_slope;

    int16_t current_credit;

    struct timespec last_checked;
};

size_t get_tas_schedules(struct schedule** schedules, uint32_t* total_window);
uint32_t map_prio(int prio);

/**
 * @return map<prio: uint8_t, credit_schedule>
 */
struct map* get_cbs_configs();
