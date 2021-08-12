#ifndef __TIMEUTIL_H__
#define __TIMEUTIL_H__

#include <sys/time.h>

void timeval_diff(const struct timeval* start, const struct timeval* end, struct timeval* result);
void timespec_diff(const struct timespec* start, const struct timespec* end, struct timespec* result);

// >0 if a > b
int timespec_compare(const struct timespec* a, const struct timespec* b);

#endif
