#ifndef DOWNSAMPLE_H
#define DOWNSAMPLE_H

#include <stddef.h>

/*
 * Downsampling — 14.3
 *
 * For every raw chunk whose end_ts < (now - 3600), produce a parallel
 * coarse chunk in <dataDir>/<metric>_1m/ containing one point per minute
 * (the average of all raw points in that minute).  A <chunkid>.done marker
 * file prevents re-processing the same chunk on subsequent wakeups.
 *
 * Dispatch strategy (used by Head_AGG):
 *   - If the query range >= 3600 s AND bucket_seconds >= 60 AND the coarse
 *     directory exists, read coarse chunks for the old portion of the range
 *     (end_ts < now - 3600) and raw chunks + head block for the recent
 *     portion.  This dramatically reduces decompression work for long-range
 *     aggregation queries.
 */

/* Build the coarse directory path for a metric into out. */
void downsample_coarse_dir(const char *dataDir, const char *metric,
                           char *out, size_t outsz);

/*
 * Return 1 if Head_AGG should prefer coarse data for this query, 0 otherwise.
 * Criteria: range >= 3600 s, bucket_seconds >= 60, coarse dir exists.
 */
int downsample_should_use(const char *dataDir, const char *metric,
                          long from, long to, int bucket_seconds);

/* Start the background downsampling thread (wakes every 5 minutes). */
void downsample_start(const char *dataDir);

/* Signal the background thread to stop and join it. */
void downsample_stop(void);

/* Run one downsampling pass immediately (used by tests). */
void downsample_run_now(const char *dataDir);

#endif
