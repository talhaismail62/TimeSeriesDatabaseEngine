#ifndef BIT_IO_H
#define BIT_IO_H

#include <stdint.h>
#include <stdlib.h>

struct bytebuffer {
    uint8_t *data;
    size_t size;
    size_t capacity;
};

struct bitwriter {
    struct bytebuffer *buffer;
    uint8_t current_byte;
    int filledbits;
};

struct bitreader {
    struct bytebuffer *buffer;
    size_t byteoffset;
    int bitsconsumed;
};

struct bytebuffer* buffercreate();
void bufferappend(struct bytebuffer *buf, uint8_t byte);
void bufferfree(struct bytebuffer *buf);

struct bitwriter* bwcreate();
void bwwrite(struct bitwriter *bw, uint64_t value, int n_bits);
void bwclose(struct bitwriter *bw);
void bwfree(struct bitwriter *bw);

struct bitreader* brcreate(struct bytebuffer *buffer);
uint64_t brread(struct bitreader *br, int n_bits);
void brfree(struct bitreader *br);

#endif
