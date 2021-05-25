#ifndef __LB_DEBUG_H__
#define __LB_DEBUG_H__

#include <stdio.h>

#ifdef DEBUG
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...) do { } while(false)
#endif  // DEBUG

#endif  // __LB_DEBUG_H__
