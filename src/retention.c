#include "retention.h"
#include "registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/* Config helpers                                                        */
/* ------------------------------------------------------------------ */

int retention_add_rule(RetentionConfig *cfg, const char *spec)
{
    if (cfg->count >= RETENTION_MAX_RULES)
        return -1;

    /* Expected format: "metric_name=seconds" */
    const char *eq = strchr(spec, '=');
    if (!eq || eq == spec)
        return -1;

    size_t name_len = (size_t)(eq - spec);
    if (name_len >= sizeof(cfg->rules[0].metric))
        return -1;

    long secs = atol(eq + 1);
    if (secs <= 0)
        return -1;

    RetentionRule *r = &cfg->rules[cfg->count];
    memcpy(r->metric, spec, name_len);
    r->metric[name_len] = '\0';
    r->max_age_seconds  = secs;
    cfg->count++;
    return 0;
}

long retention_get(const RetentionConfig *cfg, const char *metric)
{
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->rules[i].metric, metric) == 0)
            return cfg->rules[i].max_age_seconds;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Background expiry thread                                             */
/* ------------------------------------------------------------------ */

static RetentionConfig g_cfg;
static char            g_dataDir[512];
static pthread_t       g_thread;
static volatile int    g_stop = 0;

/* Remove a chunk file from disk and from the registry's chunk list.
   Must be called with registry_lock held. */
static void expire_chunk(metric_registry *entry, int idx)
{
    ChunkMetadata *cm = &entry->chunks[idx];

    if (remove(cm->filename) != 0)
        perror("retention: remove chunk");
    else
        printf("retention: expired chunk %s\n", cm->filename);

    /* Compact the array by shifting tail entries left by one. */
    int tail = entry->chunkCount - idx - 1;
    if (tail > 0)
        memmove(&entry->chunks[idx], &entry->chunks[idx + 1],
                sizeof(ChunkMetadata) * (size_t)tail);
    entry->chunkCount--;
}

static void run_expiry(void)
{
    long now = (long)time(NULL);

    pthread_mutex_lock(&registry_lock);

    metric_registry *entry, *tmp;
    HASH_ITER(hh, registry, entry, tmp) {
        long max_age = retention_get(&g_cfg, entry->key);
        if (max_age <= 0)
            continue; /* no rule for this metric */

        long cutoff = now - max_age;

        /* Walk backwards so index shifts from expire_chunk don't skip entries. */
        for (int i = entry->chunkCount - 1; i >= 0; i--) {
            if (entry->chunks[i].end_ts < cutoff)
                expire_chunk(entry, i);
        }
    }

    pthread_mutex_unlock(&registry_lock);
}

static void *retention_thread(void *arg)
{
    (void)arg;
    while (!g_stop) {
        /* Sleep in 1-second increments so we can respond to g_stop quickly. */
        for (int i = 0; i < 60 && !g_stop; i++)
            sleep(1);
        if (!g_stop)
            run_expiry();
    }
    return NULL;
}

void retention_start(const RetentionConfig *cfg, const char *dataDir)
{
    if (cfg->count == 0)
        return; /* nothing to enforce */

    g_cfg  = *cfg;
    strncpy(g_dataDir, dataDir, sizeof(g_dataDir) - 1);
    g_dataDir[sizeof(g_dataDir) - 1] = '\0';
    g_stop = 0;

    if (pthread_create(&g_thread, NULL, retention_thread, NULL) != 0)
        perror("retention_start: pthread_create");
    else
        printf("retention: background expiry thread started (%d rule(s))\n",
               cfg->count);
}

void retention_stop(void)
{
    if (g_cfg.count == 0)
        return;
    g_stop = 1;
    pthread_join(g_thread, NULL);
}
