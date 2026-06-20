#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/chunk.h"

int main()
{

    printf(
        "Starting chunk test\n");

    const char *filepath =
        "test_data.chunk";

    struct chunkheader write_header;

    write_header.chunkid = 1;

    write_header.startts = 1728000000;

    write_header.endts = 1728000050;

    write_header.sizebytes = 12;

    uint8_t write_payload[12] = {
        0xDE,
        0xAD,
        0xBE,
        0xEF,
        0x01,
        0x02,
        0x03,
        0x04,
        0xAA,
        0xBB,
        0xCC,
        0xDD};

    if (
        chunkwrite(
            filepath,
            &write_header,
            write_payload) != 0)
    {

        printf(
            "Failed\n");

        return 1;
    }

    printf(
        "Success\n");

    struct chunkheader read_header;

    uint8_t *read_payload = NULL;

    if (
        chunkread(
            filepath,
            &read_header,
            &read_payload) != 0)
    {

        printf(
            "Failed\n");

        return 1;
    }

    if (
        read_header.chunkid != write_header.chunkid ||
        read_header.sizebytes != write_header.sizebytes ||
        memcmp(
            write_payload,
            read_payload,
            12) != 0)
    {

        printf(
            "Failed\n");

        free(read_payload);

        return 1;
    }

    printf(
        "Success\n");

    free(read_payload);

    remove(filepath);

    return 0;
}
