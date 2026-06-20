#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h>

struct chunkheader {
    uint32_t chunkid;
    uint64_t startts;
    uint64_t endts;
    uint32_t sizebytes; 
    uint32_t point_count;
};


int chunkwrite(const char *filepath, struct chunkheader *header, const uint8_t *payload);


int chunkread(const char *filepath, struct chunkheader *header, uint8_t **payload);

#endif

