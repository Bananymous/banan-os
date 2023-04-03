#pragma once

#include <sys/types.h>

#define makedev(maj, min) ((dev_t)(maj) << 32 | (dev_t)(min))

#define major(dev) (((dev) >> 32) & 0xFFFFFFFF)
#define minor(dev) ( (dev)        & 0xFFFFFFFF)
