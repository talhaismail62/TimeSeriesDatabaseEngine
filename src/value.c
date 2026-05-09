#include "value.h"

#include <string.h>

static uint64_t doubletobits(double d) {

    uint64_t bits;

    memcpy(&bits, &d, sizeof(bits));

    return bits;
}

static double bitstodouble(uint64_t bits) {

    double d;

    memcpy(&d, &bits, sizeof(d));

    return d;
}

void valencoderinit(
    struct valueencoder *encoder,
    struct bitwriter *bw
) {

    encoder->bw = bw;

    encoder->prevvaluebits = 0;

    encoder->prevleading = 0;
    encoder->prevtrailing = 0;

    encoder->isfirst = 1;
}

void valencoderwrite(
    struct valueencoder *encoder,
    double value
) {

    uint64_t valbits = doubletobits(value);

    if (encoder->isfirst) {

        bwwrite(encoder->bw, valbits, 64);

        encoder->prevvaluebits = valbits;

        encoder->isfirst = 0;

        return;
    }

    uint64_t xorval =
        valbits ^ encoder->prevvaluebits;

    if (xorval == 0) {

        bwwrite(encoder->bw, 0, 1);

    } else {

        int leading = __builtin_clzll(xorval);

        int trailing = __builtin_ctzll(xorval);

        if (leading > 31) {
            leading = 31;
        }

        if (
            leading >= encoder->prevleading &&
            trailing >= encoder->prevtrailing
        ) {

            bwwrite(encoder->bw, 0x02, 2);

            int nmeaningful =
                64 -
                encoder->prevleading -
                encoder->prevtrailing;

            uint64_t meaningfulbits =
                xorval >> encoder->prevtrailing;

            bwwrite(
                encoder->bw,
                meaningfulbits,
                nmeaningful
            );

        } else {

            bwwrite(encoder->bw, 0x03, 2);

            int nmeaningful =
                64 - leading - trailing;

            bwwrite(
                encoder->bw,
                leading,
                5
            );

            bwwrite(
                encoder->bw,
                nmeaningful & 0x3F,
                6
            );

            uint64_t meaningfulbits =
                xorval >> trailing;

            bwwrite(
                encoder->bw,
                meaningfulbits,
                nmeaningful
            );

            encoder->prevleading = leading;
            encoder->prevtrailing = trailing;
        }
    }

    encoder->prevvaluebits = valbits;
}

void valdecoderinit(
    struct valuedecoder *decoder,
    struct bitreader *br
) {

    decoder->br = br;

    decoder->prevvaluebits = 0;

    decoder->prevleading = 0;
    decoder->prevtrailing = 0;

    decoder->isfirst = 1;
}

double valdecoderread(struct valuedecoder *decoder) {

    if (decoder->isfirst) {

        uint64_t valbits =
            brread(decoder->br, 64);

        decoder->prevvaluebits = valbits;

        decoder->isfirst = 0;

        return bitstodouble(valbits);
    }

    uint64_t xorval = 0;

    int bit0 = brread(decoder->br, 1);

    if (bit0 == 0) {

        xorval = 0;

    } else {

        int bit1 = brread(decoder->br, 1);

        if (bit1 == 0) {

            int nmeaningful =
                64 -
                decoder->prevleading -
                decoder->prevtrailing;

            uint64_t meaningfulbits =
                brread(
                    decoder->br,
                    nmeaningful
                );

            xorval =
                meaningfulbits <<
                decoder->prevtrailing;

        } else {

            int leading =
                brread(decoder->br, 5);

            int nmeaningful =
                brread(decoder->br, 6);

            if (nmeaningful == 0) {
                nmeaningful = 64;
            }

            int trailing =
                64 - leading - nmeaningful;

            uint64_t meaningfulbits =
                brread(
                    decoder->br,
                    nmeaningful
                );

            xorval =
                meaningfulbits << trailing;

            decoder->prevleading = leading;
            decoder->prevtrailing = trailing;
        }
    }

    uint64_t valbits =
        decoder->prevvaluebits ^ xorval;

    decoder->prevvaluebits = valbits;

    return bitstodouble(valbits);
}
