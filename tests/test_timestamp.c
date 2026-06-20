#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/timestamp.h"

int main()
{

    printf(
        "Starting timestamp test\n");

    int num_points = 1000;

    uint64_t timestamps[1000];

    uint64_t start_ts = 1728000000;

    timestamps[0] = start_ts;

    for (int i = 1; i < num_points; i++)
    {

        timestamps[i] = timestamps[i - 1] + 10;

        if (i % 100 == 0)
        {
            timestamps[i] += 2;
        }
    }

    struct bitwriter *bw = bwcreate();

    struct timestampencoder encoder;

    tsencoderinit(&encoder, bw);

    for (int i = 0; i < num_points; i++)
    {

        tsencoderwrite(&encoder, timestamps[i]);
    }

    bwclose(bw);

    printf("Successfully encoded %d timestamps into %zu bytes.\n", num_points, bw->buffer->size);

    struct bitreader *br = brcreate(bw->buffer);

    struct timestampdecoder decoder;

    tsdecoderinit(&decoder, br);

    for (int i = 0; i < num_points; i++)
    {

        uint64_t decoded_ts =
            tsdecoderread(&decoder);

        if (decoded_ts != timestamps[i])
        {

            printf(
                "Failed\n",
                i,
                (unsigned long long)timestamps[i],
                (unsigned long long)decoded_ts);

            return 1;
        }
    }

    printf(
        "Success\n");

    brfree(br);

    bwfree(bw);

    return 0;
}
