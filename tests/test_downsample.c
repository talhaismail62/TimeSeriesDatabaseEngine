#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#include "../include/downsample.h"
#include "../include/registry.h"
#include "../include/flush.h"
#include "../include/chunk.h"
#include "../include/bit_io.h"
#include "../include/timestamp.h"
#include "../include/value.h"

#define DATA_DIR "/tmp/test_downsample_data"
#define METRIC "cpu.test"

static void rmdir_recursive(const char *path)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);
}

static void setup(void)
{
    rmdir_recursive(DATA_DIR);
    mkdir(DATA_DIR, 0777);
    char metricDir[512];
    snprintf(metricDir, sizeof(metricDir), "%s/%s", DATA_DIR, METRIC);
    mkdir(metricDir, 0777);
}

/* ------------------------------------------------------------------ */
/* Helper: write a raw chunk with N points at 1-second intervals       */
/* ------------------------------------------------------------------ */

static void write_raw_chunk(const char *metricDir, long base_ts,
                            int n_points, double base_val)
{
    uint64_t *ts = malloc(sizeof(uint64_t) * (size_t)n_points);
    double *val = malloc(sizeof(double) * (size_t)n_points);

    for (int i = 0; i < n_points; i++)
    {
        ts[i] = (uint64_t)(base_ts + i);
        val[i] = base_val + (double)i * 0.01;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/%s_%lu.chunk",
             metricDir, METRIC, (unsigned long)ts[0]);
    flushtochunk((uint32_t)ts[0], path, ts, val, n_points);

    free(ts);
    free(val);
}

/* ------------------------------------------------------------------ */
/* Test 1: coarse_dir path construction                                 */
/* ------------------------------------------------------------------ */

static void test_coarse_dir_path(void)
{
    char out[512];
    downsample_coarse_dir("/data", "cpu.usage", out, sizeof(out));
    assert(strcmp(out, "/data/cpu.usage_1m") == 0);
    printf("PASS: test_coarse_dir_path\n");
}

/* ------------------------------------------------------------------ */
/* Test 2: should_use returns 0 when coarse dir is absent              */
/* ------------------------------------------------------------------ */

static void test_should_use_no_coarse_dir(void)
{
    setup();
    /* No _1m directory exists yet. */
    int r = downsample_should_use(DATA_DIR, METRIC,
                                  0, 7200, /* 2-hour range */
                                  60);
    assert(r == 0);
    printf("PASS: test_should_use_no_coarse_dir\n");
}

/* ------------------------------------------------------------------ */
/* Test 3: should_use returns 0 for short ranges or small buckets      */
/* ------------------------------------------------------------------ */

static void test_should_use_short_range(void)
{
    setup();
    /* Create coarse dir so the only reason to say no is range/bucket. */
    char coarseDir[512];
    downsample_coarse_dir(DATA_DIR, METRIC, coarseDir, sizeof(coarseDir));
    mkdir(coarseDir, 0777);

    /* Range < 3600 s -> should not use coarse. */
    assert(downsample_should_use(DATA_DIR, METRIC, 0, 1800, 60) == 0);
    /* bucket_seconds < 60 -> should not use coarse. */
    assert(downsample_should_use(DATA_DIR, METRIC, 0, 7200, 30) == 0);
    /* Both large range and large bucket -> should use. */
    assert(downsample_should_use(DATA_DIR, METRIC, 0, 7200, 60) == 1);

    printf("PASS: test_should_use_short_range\n");
}

/* ------------------------------------------------------------------ */
/* Test 4: end-to-end — write raw chunk, run downsample_metric via    */
/* the public thread entry (we call the internal logic directly by     */
/* starting the thread, sleeping briefly, then stopping it).           */
/* We verify the coarse chunk file appears in _1m/.                    */
/* ------------------------------------------------------------------ */

static void test_coarse_chunk_produced(void)
{
    setup();

    char metricDir[512];
    snprintf(metricDir, sizeof(metricDir), "%s/%s", DATA_DIR, METRIC);

    /* Write a raw chunk with 120 points (2 minutes) starting 2 hours ago. */
    long base_ts = (long)time(NULL) - 7200;
    write_raw_chunk(metricDir, base_ts, 120, 42.0);

    /* Register the metric and its chunk in the registry so
       downsample_run_now can find it. */
    HeadBlock *head = getMetricFromHashTable((char *)METRIC, true);
    (void)head;

    metric_registry *entry;
    pthread_mutex_lock(&registry_lock);
    HASH_FIND_STR(registry, METRIC, entry);
    if (entry)
    {
        char chunkPath[512];
        snprintf(chunkPath, sizeof(chunkPath), "%s/%s_%lu.chunk",
                 metricDir, METRIC, (unsigned long)base_ts);
        entry->chunks[0].start_ts = base_ts;
        entry->chunks[0].end_ts = base_ts + 119;
        strncpy(entry->chunks[0].filename, chunkPath,
                sizeof(entry->chunks[0].filename) - 1);
        entry->chunkCount = 1;
    }
    pthread_mutex_unlock(&registry_lock);

    /* Run one downsampling pass synchronously. */
    downsample_run_now(DATA_DIR);

    /* The coarse directory should now exist. */
    char coarseDir[512];
    downsample_coarse_dir(DATA_DIR, METRIC, coarseDir, sizeof(coarseDir));

    struct stat st;
    assert(stat(coarseDir, &st) == 0 && S_ISDIR(st.st_mode));

    /* At least one .chunk file should be in the coarse dir. */
    int found = 0;
    DIR *d = opendir(coarseDir);
    if (d)
    {
        struct dirent *de;
        while ((de = readdir(d)) != NULL)
        {
            if (strstr(de->d_name, ".chunk"))
            {
                found = 1;
                break;
            }
        }
        closedir(d);
    }
    assert(found == 1);

    /* A .done marker for the raw chunk should also exist. */
    char marker[512];
    snprintf(marker, sizeof(marker), "%s/%lu.done",
             coarseDir, (unsigned long)base_ts);
    assert(stat(marker, &st) == 0);

    printf("PASS: test_coarse_chunk_produced\n");
}

int main(void)
{
    test_coarse_dir_path();
    test_should_use_no_coarse_dir();
    test_should_use_short_range();
    test_coarse_chunk_produced();

    rmdir_recursive(DATA_DIR);
    printf("All downsample tests passed.\n");
    return 0;
}
