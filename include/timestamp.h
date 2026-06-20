#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stdint.h>
#include "include/bit_io.h"

struct timestampencoder
{
    struct bitwriter *bw;
    uint64_t prevtimestamp;
    int64_t prevdelta;
    int isfirst;
    int issecond;
};

struct timestampdecoder
{
    struct bitreader *br;
    uint64_t prevtimestamp;
    int64_t prevdelta;
    int isfirst;
    int issecond;
};

void tsencoderinit(struct timestampencoder *encoder, struct bitwriter *bw);
void tsencoderwrite(struct timestampencoder *encoder, uint64_t timestamp);

void tsdecoderinit(struct timestampdecoder *decoder, struct bitreader *br);
uint64_t tsdecoderread(struct timestampdecoder *decoder);

#endif
