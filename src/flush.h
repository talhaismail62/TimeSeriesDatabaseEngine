#ifndef FLUSH_H
#define FLUSH_H

#include <stdint.h>


int flushtochunk(uint32_t chunkid, const char *filepath, uint64_t *timestamps, double *values, int numpoints);

#endif
