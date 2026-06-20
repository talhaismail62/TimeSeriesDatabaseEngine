#ifndef RETENTION_H
#define RETENTION_H

#include <stdint.h>

/* Maximum number of per-metric retention rules. */
#define RETENTION_MAX_RULES 256

typedef struct {
    char   metric[64];
    long   max_age_seconds; /* delete chunks whose end_ts < now - max_age */
} RetentionRule;

typedef struct {
    RetentionRule rules[RETENTION_MAX_RULES];
    int           count;
} RetentionConfig;

/* Parse a "metric=seconds" string and add it to cfg.
   Returns 0 on success, -1 on bad format. */
int retention_add_rule(RetentionConfig *cfg, const char *spec);

/* Look up the max-age for a metric; returns -1 if no rule is set. */
long retention_get(const RetentionConfig *cfg, const char *metric);

/* Start the background expiry thread.
   Wakes every 60 s, removes stale chunk files, and updates the registry.
   dataDir is copied internally. */
void retention_start(const RetentionConfig *cfg, const char *dataDir);

/* Signal the background thread to stop and join it. */
void retention_stop(void);

#endif
