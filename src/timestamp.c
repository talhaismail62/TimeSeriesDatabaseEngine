#include "timestamp.h"

static int64_t signextend(uint64_t val, int bits)
{

    if (val & (1ULL << (bits - 1)))
    {
        return val | (~0ULL << bits);
    }

    return val;
}

void tsencoderinit(struct timestampencoder *encoder, struct bitwriter *bw)
{

    encoder->bw = bw;

    encoder->prevtimestamp = 0;
    encoder->prevdelta = 0;

    encoder->isfirst = 1;
    encoder->issecond = 0;
}

void tsencoderwrite(struct timestampencoder *encoder, uint64_t timestamp)
{

    if (encoder->isfirst)
    {

        bwwrite(encoder->bw, timestamp, 64);

        encoder->prevtimestamp = timestamp;

        encoder->isfirst = 0;
        encoder->issecond = 1;

        return;
    }

    int64_t delta = timestamp - encoder->prevtimestamp;

    if (encoder->issecond)
    {

        bwwrite(encoder->bw, delta & 0x3FFF, 14);

        encoder->prevtimestamp = timestamp;
        encoder->prevdelta = delta;

        encoder->issecond = 0;

        return;
    }

    int64_t D = delta - encoder->prevdelta;
    if (D == 0)
    {
        bwwrite(encoder->bw, 0, 1);
    }
    else if (D >= -64 && D <= 63)
    {
        bwwrite(encoder->bw, 0x02, 2);
        bwwrite(encoder->bw, D & 0x7F, 7);
    }
    else if (D >= -256 && D <= 255)
    {
        bwwrite(encoder->bw, 0x06, 3);
        bwwrite(encoder->bw, D & 0x1FF, 9);
    }
    else if (D >= -2048 && D <= 2047)
    {
        bwwrite(encoder->bw, 0x0E, 4);
        bwwrite(encoder->bw, D & 0xFFF, 12);
    }
    else
    {
        bwwrite(encoder->bw, 0x0F, 4);
        bwwrite(encoder->bw, D & 0xFFFFFFFF, 32);
    }
    encoder->prevtimestamp = timestamp;
    encoder->prevdelta = delta;
}

void tsdecoderinit(struct timestampdecoder *decoder, struct bitreader *br)
{

    decoder->br = br;

    decoder->prevtimestamp = 0;
    decoder->prevdelta = 0;

    decoder->isfirst = 1;
    decoder->issecond = 0;
}

uint64_t tsdecoderread(struct timestampdecoder *decoder)
{

    if (decoder->isfirst)
    {

        uint64_t timestamp = brread(decoder->br, 64);

        decoder->prevtimestamp = timestamp;

        decoder->isfirst = 0;
        decoder->issecond = 1;

        return timestamp;
    }

    if (decoder->issecond)
    {

        int64_t delta = signextend(brread(decoder->br, 14), 14);

        uint64_t timestamp = decoder->prevtimestamp + delta;

        decoder->prevtimestamp = timestamp;
        decoder->prevdelta = delta;

        decoder->issecond = 0;

        return timestamp;
    }

    int bit0 = brread(decoder->br, 1);

    if (bit0 == 0)
    {

        uint64_t timestamp =
            decoder->prevtimestamp + decoder->prevdelta;

        decoder->prevtimestamp = timestamp;

        return timestamp;
    }

    int bit1 = brread(decoder->br, 1);

    int64_t D = 0;

    if (bit1 == 0)
    {

        D = signextend(brread(decoder->br, 7), 7);
    }
    else
    {

        int bit2 = brread(decoder->br, 1);

        if (bit2 == 0)
        {

            D = signextend(brread(decoder->br, 9), 9);
        }
        else
        {

            int bit3 = brread(decoder->br, 1);

            if (bit3 == 0)
            {

                D = signextend(brread(decoder->br, 12), 12);
            }
            else
            {

                D = signextend(brread(decoder->br, 32), 32);
            }
        }
    }

    int64_t delta = decoder->prevdelta + D;

    uint64_t timestamp =
        decoder->prevtimestamp + delta;

    decoder->prevtimestamp = timestamp;
    decoder->prevdelta = delta;

    return timestamp;
}
