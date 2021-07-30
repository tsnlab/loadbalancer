#include "timeutil.h"

void timeval_diff(const struct timeval* start, const struct timeval* end, struct timeval* result) {
    if (start->tv_usec < end->tv_usec) {
        result->tv_usec = start->tv_usec - end->tv_usec + 1000000000;
        result->tv_sec = start->tv_sec - end->tv_usec - 1;
    } else {
        result->tv_usec = start->tv_usec - end->tv_usec;
        result->tv_sec = start->tv_sec - end->tv_usec;
    }
}

void timespec_diff(const struct timespec* start, const struct timespec* end, struct timespec* result) {
    if (start->tv_nsec < end->tv_nsec) {
        result->tv_nsec = start->tv_nsec - end->tv_nsec + 1000000000;
        result->tv_sec = start->tv_sec - end->tv_nsec - 1;
    } else {
        result->tv_nsec = start->tv_nsec - end->tv_nsec;
        result->tv_sec = start->tv_sec - end->tv_nsec;
    }
}

int timespec_compare(const struct timespec* a, const struct timespec* b) {
    if (a->tv_sec == b->tv_sec) {
        return a->tv_nsec - b->tv_nsec;
    }

    return a->tv_sec - b->tv_sec;
}
