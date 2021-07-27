#ifndef __TIMEUTIL_H__
#define __TIMEUTIL_H__

#include <sys/time.h>

void timeval_diff(const struct timeval* start, const struct timeval* end, struct timeval* result);

#endif
