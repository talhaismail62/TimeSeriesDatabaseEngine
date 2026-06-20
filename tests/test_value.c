#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../include/value.h"

double randdrift()
{

    return ((double)rand() / (double)RAND_MAX) * 2.0 - 1.0;
}

int main()
{

    printf(
        "Starting value compression est...\n");

    srand(time(NULL));

    int num_points = 1000;

    double values[1000];

    values[0] = 45.2;

    for (int i = 1; i < num_points; i++)
    {

        values[i] =
            values[i - 1] +
            (randdrift() * 0.5);

        if (i % 15 == 0)
        {
            values[i] = values[i - 1];
        }
    }

    struct bitwriter *bw = bwcreate();

    struct valueencoder encoder;

    valencoderinit(&encoder, bw);

    for (int i = 0; i < num_points; i++)
    {

        valencoderwrite(&encoder, values[i]);
    }

    bwclose(bw);

    printf(
        "Successfully encoded %d floating-point values into %zu bytes.\n",
        num_points,
        bw->buffer->size);

    struct bitreader *br =
        brcreate(bw->buffer);

    struct valuedecoder decoder;

    valdecoderinit(&decoder, br);

    for (int i = 0; i < num_points; i++)
    {

        double decoded_val =
            valdecoderread(&decoder);

        if (decoded_val != values[i])
        {

            printf(
                "Failed\n",
                i,
                values[i],
                decoded_val);

            return 1;
        }
    }

    printf(
        "Success\n");

    brfree(br);

    bwfree(bw);

    return 0;
}
