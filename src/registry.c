#include "include/registry.h"
#include <stdlib.h>
#include "include/uthash.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include "include/head.h"
#include "include/flush.h"
#include "include/timestamp.h"
#include "include/bit_io.h"
#include "include/chunk.h"
#include "include/value.h"
#include "include/wal.h"
#include "include/downsample.h"
#include <dirent.h>

typedef struct
{
        long ts;
        double val;
} Point;

static int append_point(Point **arr, int *n, int *cap, long ts, double val);
static int collect_points_from_chunk(const char *filepath, long start, long end, Point **arr, int *n, int *cap);
static int collect_points_from_head(HeadBlock *head, long start, long end, Point **arr, int *n, int *cap);

static int cmp_point_ts(const void *a, const void *b);

static char *agg_points(Point *pts, int n, long start, long end, int bucketSeconds, const char *func);

metric_registry *registry = NULL;
pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;

static char g_dataDir[512] = {0};

void registry_init(const char *dataDir)
{
        strncpy(g_dataDir, dataDir, sizeof(g_dataDir) - 1);
        g_dataDir[sizeof(g_dataDir) - 1] = '\0';
}

HeadBlock *getMetricFromHashTable(char *key, bool flag)
{
        metric_registry *tempEntry;

        printf("[%s]\n", key);
        HASH_FIND_STR(registry, key, tempEntry);
        // if(tempEntry == NULL)
        //         printf("getMetricFromHashTable:  NULL\n");
        // else
        //         printf("getMetricFromHashTable:  NOT NULL\n");

        if (tempEntry == NULL && flag == true)
        {
                tempEntry = (metric_registry *)malloc(sizeof(metric_registry));
                strncpy(tempEntry->key, key, sizeof(tempEntry->key) - 1);
                tempEntry->key[sizeof(tempEntry->key) - 1] = '\0';

                tempEntry->head = getNewHeadBlock();
                tempEntry->chunkCount = 0;
                tempEntry->chunkCapacity = 4;
                tempEntry->chunks = (ChunkMetadata *)malloc(sizeof(ChunkMetadata) * tempEntry->chunkCapacity);

                HASH_ADD_STR(registry, key, tempEntry);
                printf("New Metric was created as %s\n", tempEntry->key);
        }
        return tempEntry->head;
}

bool Head_PUT(char *metricName, long timestamp, double value, char *dataDir)
{
        // printf("reached Head_STATS, %s\n", metricName);
        pthread_mutex_lock(&registry_lock);
        HeadBlock *head = getMetricFromHashTable(metricName, true);
        pthread_mutex_unlock(&registry_lock);

        pthread_mutex_lock(&head->lock);

        if (!head)
        {
                return false;
        }

        bool res = PUT_value(head, timestamp, value);

        if (!res && head->size >= HEAD_CAPACITY)
        {
                pthread_mutex_unlock(&head->lock);
                headflush(metricName, dataDir);
                pthread_mutex_lock(&head->lock);
                res = PUT_value(head, timestamp, value);
        }

        if (res)
                wal_append(dataDir, metricName, timestamp, value);

        pthread_mutex_unlock(&head->lock);
        return res;
}

char *Head_GET(char *metricName, long startTimestamp, long endTimestamp, int *size)
{
        pthread_mutex_lock(&registry_lock);
        metric_registry *entry;
        HASH_FIND_STR(registry, metricName, entry);
        pthread_mutex_unlock(&registry_lock);

        if (!entry)
                return NULL;

        *size = 0;
        int capacity = 2048;
        char *result = malloc(capacity);
        result[0] = '\0';
        int current_len = 0;

        for (int i = 0; i < entry->chunkCount; i++)
        {

                if (entry->chunks[i].start_ts <= endTimestamp && entry->chunks[i].end_ts >= startTimestamp)
                {

                        int pointsInChunk = 0;
                        char *diskData = decompressChunk(entry->chunks[i].filename, startTimestamp, endTimestamp, &pointsInChunk);
                        if (diskData)
                        {

                                *size += pointsInChunk;
                                int diskLen = strlen(diskData);

                                if (current_len + diskLen + 1 > capacity)
                                {
                                        capacity = current_len + diskLen + 2048;
                                        result = realloc(result, capacity);
                                }
                                strcat(result, diskData);
                                current_len += diskLen;
                                free(diskData);
                        }
                }
        }

        int ramPoints = 0;
        pthread_mutex_lock(&entry->head->lock);
        char *ramData = GET_value(entry->head, startTimestamp, endTimestamp, &ramPoints);
        pthread_mutex_unlock(&entry->head->lock);

        if (ramData)
        {
                *size += ramPoints;
                int ramLen = strlen(ramData);
                if (current_len + ramLen + 1 > capacity)
                {
                        capacity = current_len + ramLen + 1;
                        result = realloc(result, capacity);
                }
                strcat(result, ramData);
                free(ramData);
        }

        char summary[64];
        sprintf(summary, "(%d points)\n", *size);

        if (current_len + strlen(summary) + 1 > capacity)
        {
                result = realloc(result, current_len + strlen(summary) + 1);
        }

        strcpy(result + current_len, summary);

        return result;
}
char *Head_AGG(char *metricName,
               long startTimestamp,
               long endTimestamp,
               int bucketSeconds,
               const char *func)
{
        // validate
        if (bucketSeconds <= 0 || startTimestamp >= endTimestamp)
        {
                char *err = (char *)malloc(64);
                if (err)
                        snprintf(err, 64, "ERR invalid range or bucket\n");
                return err;
        }

        pthread_mutex_lock(&registry_lock);
        metric_registry *entry;
        HASH_FIND_STR(registry, metricName, entry);
        pthread_mutex_unlock(&registry_lock);

        if (!entry)
        {
                // no metric -> empty result
                char *out = (char *)malloc(32);
                if (out)
                        snprintf(out, 32, "(0 buckets)\n");
                return out;
        }

        Point *pts = NULL;
        int n = 0, cap = 0;

        // 1) disk chunks — use coarse (_1m) data for large ranges if available
        int use_coarse = (g_dataDir[0] != '\0') &&
                         downsample_should_use(g_dataDir, metricName,
                                               startTimestamp, endTimestamp,
                                               bucketSeconds);

        if (use_coarse)
        {
                /* Scan the coarse directory for chunk files. */
                char coarseDir[512];
                downsample_coarse_dir(g_dataDir, metricName,
                                      coarseDir, sizeof(coarseDir));

                DIR *d = opendir(coarseDir);
                if (d)
                {
                        struct dirent *de;
                        while ((de = readdir(d)) != NULL)
                        {
                                if (!strstr(de->d_name, ".chunk"))
                                        continue;
                                char path[768];
                                snprintf(path, sizeof(path), "%s/%s",
                                         coarseDir, de->d_name);
                                /* collect_points_from_chunk filters by [start,end) internally */
                                collect_points_from_chunk(path,
                                                          startTimestamp, endTimestamp,
                                                          &pts, &n, &cap);
                        }
                        closedir(d);
                }
        }
        else
        {
                for (int i = 0; i < entry->chunkCount; i++)
                {
                        if (entry->chunks[i].start_ts < endTimestamp &&
                            entry->chunks[i].end_ts >= startTimestamp)
                        {
                                if (collect_points_from_chunk(entry->chunks[i].filename,
                                                              startTimestamp, endTimestamp,
                                                              &pts, &n, &cap) != 0)
                                {
                                        free(pts);
                                        char *err = (char *)malloc(64);
                                        if (err)
                                                snprintf(err, 64, "ERR disk read\n");
                                        return err;
                                }
                        }
                }
        }

        // 2) RAM head
        pthread_mutex_lock(&entry->head->lock);
        if (collect_points_from_head(entry->head,
                                     startTimestamp, endTimestamp,
                                     &pts, &n, &cap) != 0)
        {
                pthread_mutex_unlock(&entry->head->lock);
                free(pts);
                char *err = (char *)malloc(64);
                if (err)
                        snprintf(err, 64, "ERR no memory\n");
                return err;
        }
        pthread_mutex_unlock(&entry->head->lock);

        if (n == 0)
        {
                char *out = (char *)malloc(32);
                if (out)
                        snprintf(out, 32, "(0 buckets)\n");
                free(pts);
                return out;
        }

        // sort by timestamp
        qsort(pts, n, sizeof(Point), cmp_point_ts);

        // aggregate
        char *out = agg_points(pts, n, startTimestamp, endTimestamp, bucketSeconds, func);
        free(pts);
        return out;
}
char *decompressChunk(const char *filepath, long start, long end, int *chunkPoints)
{
        struct chunkheader header;
        uint8_t *payload = NULL;
        *chunkPoints = 0;
        int count = 0;

        if (chunkread(filepath, &header, &payload) != 0)
                return NULL;

        struct bytebuffer buf = {.data = payload, .size = header.sizebytes};
        struct bitreader *br = brcreate(&buf);

        struct timestampdecoder tsdec;
        struct valuedecoder valdec;
        tsdecoderinit(&tsdec, br);
        valdecoderinit(&valdec, br);

        int capacity = 1024;
        char *chunkResult = malloc(capacity);
        chunkResult[0] = '\0';
        int current_len = 0;

        for (uint32_t i = 0; i < header.point_count; i++)
        {
                long ts = (long)tsdecoderread(&tsdec);
                double val = valdecoderread(&valdec);

                if (ts >= start && ts < end)
                {
                        count++;
                        char temp[128];
                        int n = snprintf(temp, sizeof(temp), "%ld\t%.2f\n", ts, val);

                        if (current_len + n + 1 > capacity)
                        {
                                capacity *= 2;
                                chunkResult = realloc(chunkResult, capacity);
                        }
                        strcpy(chunkResult + current_len, temp);
                        current_len += n;
                }
        }
        *chunkPoints = count;
        free(payload);
        free(br);
        return chunkResult;
}

void print_metric(char *metric)
{
        metric_registry *entry;

        pthread_mutex_lock(&registry_lock);
        HASH_FIND_STR(registry, metric, entry);
        pthread_mutex_unlock(&registry_lock);

        if (!entry)
        {
                printf("Metric not found\n");
                return;
        }

        HeadBlock *hb = entry->head;
        pthread_mutex_lock(&hb->lock);
        printf("Metric: %s\n", metric);
        for (int i = 0; i < hb->size; i++)
        {
                printf("  %ld -> %.2f\n", hb->timestamps[i], hb->values[i]);
        }
        pthread_mutex_unlock(&hb->lock);
}

void cleanupRegistry(char *dataDir)
{
        metric_registry *entry, *tmp;

        HASH_ITER(hh, registry, entry, tmp)
        {

                if (entry->head && entry->head->size > 0)
                {
                        printf("Persisting %d points for[] %s]...\n", entry->head->size, entry->key);
                        headflush(entry->key, dataDir);
                }

                if (entry->head)
                {

                        if (entry->head->timestamps)
                                free(entry->head->timestamps);
                        if (entry->head->values)
                                free(entry->head->values);

                        pthread_mutex_destroy(&entry->head->lock);
                        free(entry->head);
                }

                if (entry->chunks)
                {
                        free(entry->chunks);
                }

                HASH_DEL(registry, entry);

                free(entry);
        }
        registry = NULL;
}

char *Head_STATS(char *metricName)
{
        pthread_mutex_lock(&registry_lock);
        metric_registry *entry;
        HASH_FIND_STR(registry, metricName, entry);
        pthread_mutex_unlock(&registry_lock);

        if (!entry)
        {
                char *result = malloc(64);
                snprintf(result, 64, "Metric not found: %s\n", metricName);
                return result;
        }

        char *stats = malloc(512);
        if (!stats)
                return NULL;

        // head details
        pthread_mutex_lock(&entry->head->lock);
        int in_memory = entry->head->size;
        long first_ram_ts = (in_memory > 0) ? entry->head->timestamps[0] : -1;
        long last_ram_ts = (in_memory > 0) ? entry->head->lastTimestamp : -1;
        pthread_mutex_unlock(&entry->head->lock);

        //  chunk details
        int disk_chunks = entry->chunkCount;
        long first_disk_ts = (disk_chunks > 0) ? entry->chunks[0].start_ts : -1;
        long last_disk_ts = (disk_chunks > 0) ? entry->chunks[disk_chunks - 1].end_ts : -1;

        long overall_first = (first_disk_ts != -1) ? first_disk_ts : first_ram_ts;
        long overall_last = (last_ram_ts != -1) ? last_ram_ts : last_disk_ts;

        int offset = 0;
        offset += snprintf(stats + offset, 512 - offset, "metric: %s\n", metricName);
        offset += snprintf(stats + offset, 512 - offset, "in memory: %d\n", in_memory);
        offset += snprintf(stats + offset, 512 - offset, "disk chunks: %d\n", disk_chunks);

        if (overall_first != -1)
        {
                offset += snprintf(stats + offset, 512 - offset, "first timestamp: %ld\n", overall_first);
        }
        else
        {
                offset += snprintf(stats + offset, 512 - offset, "first timestamp: N/A\n");
        }

        if (overall_last != -1)
        {
                offset += snprintf(stats + offset, 512 - offset, "last timestamp: %ld\n", overall_last);
        }
        else
        {
                offset += snprintf(stats + offset, 512 - offset, "last timestamp: N/A\n");
        }

        return stats;
}

bool headflush(char *metricname, char *dataDir)
{

        pthread_mutex_lock(&registry_lock);

        HeadBlock *head =
            getMetricFromHashTable(
                metricname,
                false);

        pthread_mutex_unlock(&registry_lock);

        if (!head)
        {
                return false;
        }

        pthread_mutex_lock(&head->lock);

        if (head->size == 0)
        {

                pthread_mutex_unlock(&head->lock);

                return true;
        }

        char metricDir[256];
        snprintf(metricDir, sizeof(metricDir), "%s/%s", dataDir, metricname);
        mkdir(metricDir, 0777);

        uint32_t chunkid =
            (uint32_t)head->timestamps[0];

        char filepath[256];

        snprintf(
            filepath,
            sizeof(filepath),
            "%s/%s_%u.chunk",
            metricDir,
            metricname,
            chunkid);

        int res =
            flushtochunk(
                chunkid,
                filepath,
                (uint64_t *)head->timestamps,
                head->values,
                head->size);

        if (res == 0)
        {

                metric_registry *entry;
                HASH_FIND_STR(registry, metricname, entry);

                if (entry->chunkCount >= entry->chunkCapacity)
                {
                        entry->chunkCapacity *= 2;
                        entry->chunks = realloc(entry->chunks, sizeof(ChunkMetadata) * entry->chunkCapacity);
                }

                entry->chunks[entry->chunkCount].start_ts = head->timestamps[0];
                entry->chunks[entry->chunkCount].end_ts = head->lastTimestamp;
                strcpy(entry->chunks[entry->chunkCount].filename, filepath);
                entry->chunkCount++;
                head->size = 0;
                head->lastTimestamp = 0;
                wal_truncate(dataDir, metricname);
        }

        pthread_mutex_unlock(&head->lock);

        return (res == 0);
}
static int append_point(Point **arr, int *n, int *cap, long ts, double val)
{
        if (*cap == 0)
        {
                *cap = 1024;
                *arr = (Point *)malloc(sizeof(Point) * (*cap));
                if (!*arr)
                        return -1;
        }
        else if (*n >= *cap)
        {
                *cap *= 2;
                Point *tmp = (Point *)realloc(*arr, sizeof(Point) * (*cap));
                if (!tmp)
                        return -1;
                *arr = tmp;
        }

        (*arr)[*n].ts = ts;
        (*arr)[*n].val = val;
        (*n)++;
        return 0;
}
static int collect_points_from_chunk(const char *filepath, long start, long end,
                                     Point **arr, int *n, int *cap)
{
        struct chunkheader header;
        uint8_t *payload = NULL;

        if (chunkread(filepath, &header, &payload) != 0)
        {
                return -1;
        }

        struct bytebuffer buf = {.data = payload, .size = header.sizebytes};
        struct bitreader *br = brcreate(&buf);

        struct timestampdecoder tsdec;
        struct valuedecoder valdec;
        tsdecoderinit(&tsdec, br);
        valdecoderinit(&valdec, br);

        for (uint32_t i = 0; i < header.point_count; i++)
        {
                long ts = (long)tsdecoderread(&tsdec);
                double val = valdecoderread(&valdec);

                // IMPORTANT: doc requires [start, end)
                if (ts >= start && ts < end)
                {
                        if (append_point(arr, n, cap, ts, val) != 0)
                        {
                                free(payload);
                                brfree(br);
                                return -1;
                        }
                }
        }

        free(payload);
        brfree(br);
        return 0;
}
static int collect_points_from_head(HeadBlock *head, long start, long end,
                                    Point **arr, int *n, int *cap)
{
        // caller should lock head->lock; we’ll do it in Head_AGG
        for (int i = 0; i < head->size; i++)
        {
                long ts = head->timestamps[i];
                if (ts >= start && ts < end)
                {
                        if (append_point(arr, n, cap, ts, head->values[i]) != 0)
                        {
                                return -1;
                        }
                }
        }
        return 0;
}
static int cmp_point_ts(const void *a, const void *b)
{
        const Point *pa = (const Point *)a;
        const Point *pb = (const Point *)b;
        if (pa->ts < pb->ts)
                return -1;
        if (pa->ts > pb->ts)
                return 1;
        return 0;
}
static char *agg_points(Point *pts, int n,
                        long start, long end,
                        int bucketSeconds,
                        const char *func)
{

        // allocate output buffer
        int out_cap = 2048;
        char *out = (char *)malloc(out_cap);
        if (!out)
                return NULL;
        out[0] = '\0';
        int out_len = 0;

        int buckets = 0;

        int distinctCount = 0;
        long prevTS = 0;
        qsort(pts, n, sizeof(Point), cmp_point_ts);
        // iterate buckets in [start, end) stepping by bucketSeconds
        for (long bstart = start; bstart < end; bstart += bucketSeconds)
        {
                long bend = bstart + bucketSeconds;

                // aggregate points with ts in [bstart, bend)
                int count = 0;
                double sum = 0.0;
                double minv = 0.0, maxv = 0.0;

                for (int i = 0; i < n; i++)
                {

                        long ts = pts[i].ts;
                        if (ts < bstart)
                                continue;
                        if (ts >= bend)
                                break; // because sorted
                        double v = pts[i].val;

                        if (count == 0)
                        {
                                minv = maxv = v;
                        }
                        else
                        {
                                if (v < minv)
                                        minv = v;
                                if (v > maxv)
                                        maxv = v;
                        }
                        sum += v;
                        count++;
                        if (pts[i].ts != prevTS)
                                ++distinctCount;
                        prevTS = pts[i].ts;
                }

                // Per spec, you typically output buckets that have points.
                // (Doc example shows only buckets with data.)
                if (count == 0)
                        continue;

                double result = 0.0;
                if (strcmp(func, "avg") == 0)
                {
                        result = sum / (double)count;
                }
                else if (strcmp(func, "min") == 0)
                {
                        result = minv;
                }
                else if (strcmp(func, "max") == 0)
                {
                        result = maxv;
                }
                else if (strcmp(func, "sum") == 0)
                {
                        result = sum;
                }
                else if (strcmp(func, "count") == 0)
                {
                        result = (double)count;
                }
                else if (strcmp(func, "Dcount") == 0)
                {
                        result = (double)distinctCount;
                }
                else
                {
                        // unknown func
                        free(out);
                        char *err = (char *)malloc(64);
                        if (err)
                                snprintf(err, 64, "ERR invalid func\n");
                        return err;
                }

                char line[128];
                // doc shows: start-end value
                // keep formatting simple; for count we print as integer-ish but using %.2f is ok too
                if (strcmp(func, "count") == 0)
                {
                        snprintf(line, sizeof(line), "%ld-%ld %d\n", bstart, bend, count);
                }
                else
                {
                        snprintf(line, sizeof(line), "%ld-%ld %.6f\n", bstart, bend, result);
                }

                int need = (int)strlen(line);
                if (out_len + need + 64 > out_cap)
                {
                        out_cap = (out_len + need + 64) * 2;
                        char *tmp = (char *)realloc(out, out_cap);
                        if (!tmp)
                        {
                                free(out);
                                return NULL;
                        }
                        out = tmp;
                }

                memcpy(out + out_len, line, need);
                out_len += need;
                out[out_len] = '\0';
                buckets++;
        }

        // footer like doc: "(N buckets)"
        char footer[64];
        snprintf(footer, sizeof(footer), "(%d buckets)\n", buckets);

        int need = (int)strlen(footer);
        if (out_len + need + 1 > out_cap)
        {
                out_cap = out_len + need + 1;
                char *tmp = (char *)realloc(out, out_cap);
                if (!tmp)
                {
                        free(out);
                        return NULL;
                }
                out = tmp;
        }
        memcpy(out + out_len, footer, need + 1);

        return out;
}

char *Head_AGG_EVAL(char *metricName,
                    long startTimestamp,
                    long endTimestamp,
                    int bucketSeconds,
                    const char *func)
{
        // validate
        if (bucketSeconds <= 0 || startTimestamp >= endTimestamp)
        {
                char *err = (char *)malloc(64);
                if (err)
                        snprintf(err, 64, "ERR invalid range or bucket\n");
                return err;
        }

        pthread_mutex_lock(&registry_lock);
        metric_registry *entry;
        HASH_FIND_STR(registry, metricName, entry);
        pthread_mutex_unlock(&registry_lock);

        if (!entry)
        {
                // no metric -> empty result
                char *out = (char *)malloc(32);
                if (out)
                        snprintf(out, 32, "(0 buckets)\n");
                return out;
        }

        Point *pts = NULL;
        int n = 0, cap = 0;

        // 1) disk chunks — use coarse (_1m) data for large ranges if available
        int use_coarse = (g_dataDir[0] != '\0') &&
                         downsample_should_use(g_dataDir, metricName,
                                               startTimestamp, endTimestamp,
                                               bucketSeconds);

        if (use_coarse)
        {
                /* Scan the coarse directory for chunk files. */
                char coarseDir[512];
                downsample_coarse_dir(g_dataDir, metricName,
                                      coarseDir, sizeof(coarseDir));

                DIR *d = opendir(coarseDir);
                if (d)
                {
                        struct dirent *de;
                        while ((de = readdir(d)) != NULL)
                        {
                                if (!strstr(de->d_name, ".chunk"))
                                        continue;
                                char path[768];
                                snprintf(path, sizeof(path), "%s/%s",
                                         coarseDir, de->d_name);
                                /* collect_points_from_chunk filters by [start,end) internally */
                                collect_points_from_chunk(path,
                                                          startTimestamp, endTimestamp,
                                                          &pts, &n, &cap);
                        }
                        closedir(d);
                }
        }
        else
        {
                for (int i = 0; i < entry->chunkCount; i++)
                {
                        if (entry->chunks[i].start_ts < endTimestamp &&
                            entry->chunks[i].end_ts >= startTimestamp)
                        {
                                if (collect_points_from_chunk(entry->chunks[i].filename,
                                                              startTimestamp, endTimestamp,
                                                              &pts, &n, &cap) != 0)
                                {
                                        free(pts);
                                        char *err = (char *)malloc(64);
                                        if (err)
                                                snprintf(err, 64, "ERR disk read\n");
                                        return err;
                                }
                        }
                }
        }

        // 2) RAM head
        pthread_mutex_lock(&entry->head->lock);
        if (collect_points_from_head(entry->head,
                                     startTimestamp, endTimestamp,
                                     &pts, &n, &cap) != 0)
        {
                pthread_mutex_unlock(&entry->head->lock);
                free(pts);
                char *err = (char *)malloc(64);
                if (err)
                        snprintf(err, 64, "ERR no memory\n");
                return err;
        }
        pthread_mutex_unlock(&entry->head->lock);

        if (n == 0)
        {
                char *out = (char *)malloc(32);
                if (out)
                        snprintf(out, 32, "(0 buckets)\n");
                free(pts);
                return out;
        }

        // sort by timestamp
        qsort(pts, n, sizeof(Point), cmp_point_ts);

        // aggregate
        char *out = agg_points(pts, n, startTimestamp, endTimestamp, bucketSeconds, func);
        free(pts);
        return out;
}