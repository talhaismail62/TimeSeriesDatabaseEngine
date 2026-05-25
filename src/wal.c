#include "wal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* Each WAL record is exactly 16 bytes: int64 timestamp + IEEE-754 double. */
#define WAL_RECORD_BYTES 16

static void build_wal_path(char *out, size_t outsz,
                           const char *dataDir, const char *metric)
{
    snprintf(out, outsz, "%s/%s/wal.log", dataDir, metric);
}

static void ensure_metric_dir(const char *dataDir, const char *metric)
{
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/%s", dataDir, metric);
    if (mkdir(dir, 0777) != 0 && errno != EEXIST) {
        perror("wal: mkdir");
    }
}

void wal_append(const char *dataDir, const char *metric, long ts, double val)
{
    ensure_metric_dir(dataDir, metric);

    char path[512];
    build_wal_path(path, sizeof(path), dataDir, metric);

    FILE *f = fopen(path, "ab");
    if (!f) {
        perror("wal_append: fopen");
        return;
    }

    int64_t ts64 = (int64_t)ts;
    if (fwrite(&ts64, sizeof(ts64), 1, f) != 1 ||
        fwrite(&val,  sizeof(val),  1, f) != 1) {
        perror("wal_append: fwrite");
        fclose(f);
        return;
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);
}

void wal_replay(const char *dataDir, const char *metric, HeadBlock *head)
{
    char path[512];
    build_wal_path(path, sizeof(path), dataDir, metric);

    FILE *f = fopen(path, "rb");
    if (!f) {
        /* No WAL file is normal on a clean start. */
        return;
    }

    int64_t ts64;
    double  val;
    int replayed = 0;

    while (fread(&ts64, sizeof(ts64), 1, f) == 1 &&
           fread(&val,  sizeof(val),  1, f) == 1) {
        long ts = (long)ts64;
        /* Skip points already covered by a flushed chunk. */
        if (ts <= head->lastTimestamp)
            continue;
        if (PUT_value(head, ts, val))
            replayed++;
    }

    fclose(f);

    if (replayed > 0)
        printf("WAL replay: recovered %d points for metric '%s'\n",
               replayed, metric);
}

void wal_truncate(const char *dataDir, const char *metric)
{
    char path[512];
    build_wal_path(path, sizeof(path), dataDir, metric);

    if (remove(path) != 0 && errno != ENOENT) {
        perror("wal_truncate: remove");
    }
}
