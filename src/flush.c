#include "flush.h"

#include "bit_io.h"
#include "timestamp.h"
#include "value.h"
#include "chunk.h"

#include <stdio.h>
#include <stdlib.h>

int flushtochunk(
    uint32_t chunkid,
    const char *filepath,
    uint64_t *timestamps,
    double *values,
    int numpoints
) {

    if (numpoints == 0) {
        return 0;
    }

    struct bitwriter *bw = bwcreate();

    struct timestampencoder tsenc;

    struct valueencoder valenc;

    tsencoderinit(&tsenc, bw);

    valencoderinit(&valenc, bw);

    for (int i = 0; i < numpoints; i++) {

        tsencoderwrite(
            &tsenc,
            timestamps[i]
        );

        valencoderwrite(
            &valenc,
            values[i]
        );
    }

    bwclose(bw);

    struct chunkheader header;

    header.chunkid = chunkid;

    header.startts = timestamps[0];

    header.endts =
        timestamps[numpoints - 1];

    header.sizebytes =
        bw->buffer->size;
    header.point_count = (uint32_t)numpoints;

    int result =
        chunkwrite(
            filepath,
            &header,
            bw->buffer->data
        );

    if (result == 0) {

        printf(
            "Successfully flushed chunk %u to %s (Compressed %d points into %u bytes)\n",
            chunkid,
            filepath,
            numpoints,
            header.sizebytes
        );

    } else {

        printf(
            "Failed to flush chunk\n",
            chunkid
        );
    }

    bwfree(bw);

    return result;
}
