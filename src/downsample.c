#include <sys/types.h>
#include <dirent.h>
#include "include/downsample.h"
#include "include/registry.h"
#include "include/chunk.h"

#include "include/flush.h"
#include "include/bit_io.h"
#include "include/timestamp.h"
#include "include/value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#define COARSE_BUCKET_SECONDS 60
#define DOWNSAMPLE_AFTER_SECONDS 3600 /* only downsample chunks older than 1 h */
#define THREAD_SLEEP_SECONDS 300      /* wake every 5 minutes */

/* ------------------------------------------------------------------ */
/* Path helpers                                                          */
/* ------------------------------------------------------------------ */

void downsample_coarse_dir(const char *dataDir, const char *metric,
                           char *out, size_t outsz)
{
    snprintf(out, outsz, "%s/%s_1m", dataDir, metric);
}

static void done_marker_path(const char *coarseDir, uint32_t chunkid,
                             char *out, size_t outsz)
{
    snprintf(out, outsz, "%s/%u.done", coarseDir, chunkid);
}

static int marker_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static void create_marker(const char *path)
{
    FILE *f = fopen(path, "w");
    if (f)
        fclose(f);
}

/* ------------------------------------------------------------------ */
/* Dispatch check                                                        */
/* ------------------------------------------------------------------ */

int downsample_should_use(const char *dataDir, const char *metric,
                          long from, long to, int bucket_seconds)
{
    if ((to - from) < DOWNSAMPLE_AFTER_SECONDS)
        return 0;
    if (bucket_seconds < COARSE_BUCKET_SECONDS)
        return 0;

    char coarseDir[512];
    downsample_coarse_dir(dataDir, metric, coarseDir, sizeof(coarseDir));

    struct stat st;
    return (stat(coarseDir, &st) == 0 && S_ISDIR(st.st_mode));
}

/* ------------------------------------------------------------------ */
/* Core downsampling logic                                              */
/* ------------------------------------------------------------------ */

typedef struct
{
    long ts;
    double val;
} DSPoint;

/* Decompress an entire raw chunk into a freshly allocated array.
   Caller must free *out. Returns point count, or -1 on error. */
static int decompress_all(const char *filepath, DSPoint **out)
{
    struct chunkheader header;
    uint8_t *payload = NULL;

    if (chunkread(filepath, &header, &payload) != 0)
        return -1;

    struct bytebuffer buf = {.data = payload, .size = header.sizebytes};
    struct bitreader *br = brcreate(&buf);

    struct timestampdecoder tsdec;
    struct valuedecoder valdec;
    tsdecoderinit(&tsdec, br);
    valdecoderinit(&valdec, br);

    int n = (int)header.point_count;
    *out = malloc(sizeof(DSPoint) * (size_t)n);
    if (!*out)
    {
        free(payload);
        brfree(br);
        return -1;
    }

    for (int i = 0; i < n; i++)
    {
        (*out)[i].ts = (long)tsdecoderread(&tsdec);
        (*out)[i].val = valdecoderread(&valdec);
    }

    free(payload);
    brfree(br);
    return n;
}

/* Produce one coarse chunk (60-second averages) from pts[0..n-1].
   Writes the result to coarseDir using flushtochunk. */
static void write_coarse_chunk(const char *coarseDir,
                               const char *metricName,
                               DSPoint *pts, int n)
{
    if (n == 0)
        return;

    /* Upper-bound on number of buckets: range / 60 + 1 */
    long range = pts[n - 1].ts - pts[0].ts + COARSE_BUCKET_SECONDS;
    int max_buckets = (int)(range / COARSE_BUCKET_SECONDS) + 1;

    uint64_t *out_ts = malloc(sizeof(uint64_t) * (size_t)max_buckets);
    double *out_val = malloc(sizeof(double) * (size_t)max_buckets);
    if (!out_ts || !out_val)
    {
        free(out_ts);
        free(out_val);
        return;
    }

    int out_n = 0;

    /* Align first bucket to a minute boundary. */
    long bucket_start = (pts[0].ts / COARSE_BUCKET_SECONDS) * COARSE_BUCKET_SECONDS;
    long bucket_end = bucket_start + COARSE_BUCKET_SECONDS;

    double sum = 0.0;
    int count = 0;

    for (int i = 0; i < n; i++)
    {
        /* Flush completed buckets before this point. */
        while (pts[i].ts >= bucket_end)
        {
            if (count > 0)
            {
                out_ts[out_n] = (uint64_t)bucket_start;
                out_val[out_n] = sum / (double)count;
                out_n++;
                sum = 0.0;
                count = 0;
            }
            bucket_start = bucket_end;
            bucket_end += COARSE_BUCKET_SECONDS;
        }
        sum += pts[i].val;
        count++;
    }
    /* Final partial bucket. */
    if (count > 0)
    {
        out_ts[out_n] = (uint64_t)bucket_start;
        out_val[out_n] = sum / (double)count;
        out_n++;
    }

    if (out_n > 0)
    {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s_%u.chunk",
                 coarseDir, metricName, (uint32_t)out_ts[0]);
        flushtochunk((uint32_t)out_ts[0], filepath, out_ts, out_val, out_n);
    }

    free(out_ts);
    free(out_val);
}

/* Process one metric: scan its raw chunks, downsample those older than 1 h. */
static void downsample_metric(const char *dataDir, metric_registry *entry)
{
    long now = (long)time(NULL);
    long cutoff = now - DOWNSAMPLE_AFTER_SECONDS;

    char coarseDir[512];
    downsample_coarse_dir(dataDir, entry->key, coarseDir, sizeof(coarseDir));

    int made_dir = 0;

    /* registry_lock is held by the caller for the HASH_ITER; we release it
       around the actual I/O to avoid blocking client threads for too long. */
    for (int i = 0; i < entry->chunkCount; i++)
    {
        if (entry->chunks[i].end_ts >= cutoff)
            continue; /* not old enough */

        uint32_t chunkid = (uint32_t)entry->chunks[i].start_ts;

        char marker[512];
        done_marker_path(coarseDir, chunkid, marker, sizeof(marker));

        if (marker_exists(marker))
            continue; /* already downsampled */

        /* Create the coarse directory on first use. */
        if (!made_dir)
        {
            if (mkdir(coarseDir, 0777) != 0 && errno != EEXIST)
                perror("downsample: mkdir");
            made_dir = 1;
        }

        char raw_path[512];
        strncpy(raw_path, entry->chunks[i].filename, sizeof(raw_path) - 1);

        /* Drop the registry lock while doing disk I/O. */
        pthread_mutex_unlock(&registry_lock);

        DSPoint *pts = NULL;
        int n = decompress_all(raw_path, &pts);
        if (n > 0)
        {
            write_coarse_chunk(coarseDir, entry->key, pts, n);
            create_marker(marker);
            printf("downsample: produced coarse chunk for %s chunk %u (%d pts -> 1m avg)\n",
                   entry->key, chunkid, n);
        }
        free(pts);

        pthread_mutex_lock(&registry_lock);
    }
}

/* ------------------------------------------------------------------ */
/* Background thread                                                     */
/* ------------------------------------------------------------------ */

static char g_dataDir[512];
static pthread_t g_thread;
static volatile int g_stop = 0;

static void *downsample_thread(void *arg)
{
    (void)arg;
    while (!g_stop)
    {
        for (int i = 0; i < THREAD_SLEEP_SECONDS && !g_stop; i++)
            sleep(1);
        if (g_stop)
            break;

        pthread_mutex_lock(&registry_lock);
        metric_registry *entry, *tmp;
        HASH_ITER(hh, registry, entry, tmp)
        downsample_metric(g_dataDir, entry);
        pthread_mutex_unlock(&registry_lock);
    }
    return NULL;
}

void downsample_run_now(const char *dataDir)
{
    pthread_mutex_lock(&registry_lock);
    metric_registry *entry, *tmp;
    HASH_ITER(hh, registry, entry, tmp)
    downsample_metric(dataDir, entry);
    pthread_mutex_unlock(&registry_lock);
}

void downsample_start(const char *dataDir)
{
    strncpy(g_dataDir, dataDir, sizeof(g_dataDir) - 1);
    g_dataDir[sizeof(g_dataDir) - 1] = '\0';
    g_stop = 0;

    if (pthread_create(&g_thread, NULL, downsample_thread, NULL) != 0)
        perror("downsample_start: pthread_create");
    else
        printf("downsample: background thread started (runs every %ds)\n",
               THREAD_SLEEP_SECONDS);
}

void downsample_stop(void)
{
    g_stop = 1;
    pthread_join(g_thread, NULL);
}
