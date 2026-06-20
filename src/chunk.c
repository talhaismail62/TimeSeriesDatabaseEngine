#include "include/chunk.h"

#include <stdio.h>
#include <stdlib.h>

int chunkwrite(
    const char *filepath,
    struct chunkheader *header,
    const uint8_t *payload)
{

    FILE *file = fopen(filepath, "wb");

    if (!file)
    {

        perror(
            "Failed to open chunk file for writing");

        return -1;
    }

    if (
        fwrite(
            header,
            sizeof(struct chunkheader),
            1,
            file) != 1)
    {

        fclose(file);

        return -1;
    }

    if (
        fwrite(
            payload,
            1,
            header->sizebytes,
            file) != header->sizebytes)
    {

        fclose(file);

        return -1;
    }

    fclose(file);

    return 0;
}

int chunkread(
    const char *filepath,
    struct chunkheader *header,
    uint8_t **payload)
{

    FILE *file = fopen(filepath, "rb");

    if (!file)
    {

        perror(
            "Failed to open chunk file for reading");

        return -1;
    }

    if (
        fread(
            header,
            sizeof(struct chunkheader),
            1,
            file) != 1)
    {

        fclose(file);

        return -1;
    }

    *payload =
        (uint8_t *)malloc(header->sizebytes);

    if (!(*payload))
    {

        fclose(file);

        return -1;
    }

    if (
        fread(
            *payload,
            1,
            header->sizebytes,
            file) != header->sizebytes)
    {

        free(*payload);

        fclose(file);

        return -1;
    }

    fclose(file);

    return 0;
}
