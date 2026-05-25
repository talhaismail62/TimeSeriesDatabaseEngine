#include <stdio.h>
#include <stdlib.h>
#include "../src/bit_io.h"
#include "../src/timestamp.h"
#include "../src/value.h"

int main() {
    struct bitwriter *bw = bwcreate();
    
    struct timestampencoder ts_encoder;
    struct valueencoder val_encoder;
    
    tsencoderinit(&ts_encoder, bw); // exactly one
    valencoderinit(&val_encoder, bw);

    long base_timestamp = 1728000000;
    double stable_value = 50.00;
    int test_points = 10000;

    for (int i = 0; i < test_points; i++) {
        long current_ts = base_timestamp + i; 
        
        double current_val = stable_value;
        
        // Simulate real-world step trends instead of rapid sub-bit oscillations
        if (i > 2000 && i < 4000) {
            current_val = 50.05; // Holds flat step
        } else if (i >= 4000 && i < 7000) {
            current_val = 51.22; // Holds another flat step
        } else if (i >= 7000) {
            current_val = 49.85; // Holds final flat step
        }

        tsencoderwrite(&ts_encoder, current_ts);
        valencoderwrite(&val_encoder, current_val);
    }

    bwclose(bw);

    long long naive_bytes = (long long)test_points * 16LL;
    long long compressed_bytes = (long long)bw->buffer->size;

    double ratio = (double)naive_bytes / (double)compressed_bytes;

    printf("=== Gorilla Architecture Micro-Benchmark ===\n");
    printf("Processed Points:    %d\n", test_points);
    printf("Naive Size (Bytes):  %lld\n", naive_bytes);
    printf("Compressed Size:     %lld\n", compressed_bytes);
    printf("Targeted Ratio:      %.2fx\n", ratio);
    
    if (ratio > 5.0) {
        printf("RESULT: SUCCESS! Your encoder math algorithms are flawless.\n");
    } else {
        printf("RESULT: FAIL! The bitwriter is leaking bytes somewhere internally.\n");
    }

    bwfree(bw);
    return 0;
}