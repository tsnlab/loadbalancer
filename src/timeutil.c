#include "timeutil.h"

void timeval_diff(const struct timeval * start, const struct timeval * end, struct timeval * result) {
    if (start->tv_usec < end->tv_usec) {
        result->tv_usec = start->tv_usec - end->tv_usec + 1000000000;
        result->tv_sec = start->tv_sec - end->tv_usec - 1;
    } else {
        result->tv_usec = start->tv_usec - end->tv_usec;
        result->tv_sec = start->tv_sec - end->tv_usec;
    }
}
