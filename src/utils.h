#pragma once
int minmax(const int value, const int min, const int max);

// clang-format off
#ifdef DEBUG
#include <stdio.h>
#define dprintf printf
#else
#define dprintf(...) do{}while(false)
#endif
// clang-format on
