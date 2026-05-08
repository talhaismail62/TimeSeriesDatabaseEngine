#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "../src/bit_io.h"

uint64_t rand64() {
    uint64_t r1 = (uint64_t)rand();
    uint64_t r2 = (uint64_t)rand();
    uint64_t r3 = (uint64_t)rand();

    return (r1 << 42) ^ (r2 << 21) ^ r3;
}

int main() {

    srand(time(NULL));

    int num_values = 1000;

    uint64_t values[1000];
    int widths[1000];

    printf("Starting test\n");

    struct bitwriter *bw = bwcreate();

    for (int i = 0; i < num_values; i++) {

        widths[i] = (rand() % 64) + 1;

        uint64_t mask =
            (widths[i] == 64)
            ? ~0ULL
            : ((1ULL << widths[i]) - 1);

        values[i] = rand64() & mask;

        bwwrite(bw, values[i], widths[i]);
    }

    bwclose(bw);

    printf(
        "Success\n",
        num_values,
        bw->buffer->size
    );

    struct bitreader *br = brcreate(bw->buffer);

    for (int i = 0; i < num_values; i++) {

        uint64_t read_val = brread(br, widths[i]);

        if (read_val != values[i]) {

            printf(
                "Failed\n",
                i,
                (unsigned long long)values[i],
                (unsigned long long)read_val,
                widths[i]
            );

            return 1;
        }
    }

    printf("All values decoded and matched perfectly\n");

    brfree(br);

    bwfree(bw);

    return 0;
}
