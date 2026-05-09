#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>
#include "bit_io.h"

struct valueencoder {
    struct bitwriter *bw;
    uint64_t prevvaluebits;
    int prevleading;
    int prevtrailing;
    int isfirst;
};

struct valuedecoder {
    struct bitreader *br;
    uint64_t prevvaluebits;
    int prevleading;
    int prevtrailing;
    int isfirst;
};

void valencoderinit(struct valueencoder *encoder, struct bitwriter *bw);
void valencoderwrite(struct valueencoder *encoder, double value);

void valdecoderinit(struct valuedecoder *decoder, struct bitreader *br);
double valdecoderread(struct valuedecoder *decoder);

#endif
