#include "bit_io.h"

struct bytebuffer* buffercreate() {
    struct bytebuffer *buf = malloc(sizeof(struct bytebuffer));

    buf->capacity = 64;
    buf->size = 0;
    buf->data = malloc(buf->capacity);

    return buf;
}

void bufferappend(struct bytebuffer *buf, uint8_t byte) {
    if (buf->size >= buf->capacity) {
        buf->capacity *= 2;
        buf->data = realloc(buf->data, buf->capacity);
    }

    buf->data[buf->size++] = byte;
}

void bufferfree(struct bytebuffer *buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

struct bitwriter* bwcreate() {
    struct bitwriter *bw = malloc(sizeof(struct bitwriter));

    bw->buffer = buffercreate();
    bw->current_byte = 0;
    bw->filledbits = 0;

    return bw;
}

void bwwrite(struct bitwriter *bw, uint64_t value, int n_bits) {
    while (n_bits > 0) {

        int free_in_byte = 8 - bw->filledbits;
        int take = (n_bits < free_in_byte) ? n_bits : free_in_byte;
        int shift = n_bits - take;

        uint64_t chunk = (value >> shift) & ((1ULL << take) - 1);

        bw->current_byte |= chunk << (free_in_byte - take);

        bw->filledbits += take;
        n_bits -= take;

        if (bw->filledbits == 8) {

            bufferappend(bw->buffer, bw->current_byte);

            bw->current_byte = 0;
            bw->filledbits = 0;
        }
    }
}

void bwclose(struct bitwriter *bw) {
    if (bw->filledbits > 0) {

        bufferappend(bw->buffer, bw->current_byte);

        bw->current_byte = 0;
        bw->filledbits = 0;
    }
}

void bwfree(struct bitwriter *bw) {
    if (bw) {
        bufferfree(bw->buffer);
        free(bw);
    }
}

struct bitreader* brcreate(struct bytebuffer *buffer) {
    struct bitreader *br = malloc(sizeof(struct bitreader));

    br->buffer = buffer;
    br->byteoffset = 0;
    br->bitsconsumed = 0;

    return br;
}

uint64_t brread(struct bitreader *br, int n_bits) {
    uint64_t result = 0;

    while (n_bits > 0) {

        if (br->byteoffset >= br->buffer->size) {
            return result;
        }

        int available_in_byte = 8 - br->bitsconsumed;
        int take = (n_bits < available_in_byte) ? n_bits : available_in_byte;

        uint8_t current = br->buffer->data[br->byteoffset];

        uint8_t chunk =
            (current >> (available_in_byte - take)) &
            ((1 << take) - 1);

        result = (result << take) | chunk;

        br->bitsconsumed += take;
        n_bits -= take;

        if (br->bitsconsumed == 8) {
            br->bitsconsumed = 0;
            br->byteoffset++;
        }
    }

    return result;
}

void brfree(struct bitreader *br) {
    if (br) {
        free(br);
    }
}
