#ifndef WAL_H
#define WAL_H

#include "head.h"

/* Append one (timestamp, value) record to the metric's WAL.
   Creates the metric directory if it does not exist yet.
   Calls fsync before returning so the kernel durably orders the write. */
void wal_append(const char *dataDir, const char *metric, long ts, double val);

/* Replay all records in the metric's WAL into head.
   Skips records whose timestamps are <= head->lastTimestamp so a partial
   WAL that overlaps an already-flushed chunk does not produce duplicates. */
void wal_replay(const char *dataDir, const char *metric, HeadBlock *head);

/* Truncate (delete) the metric's WAL after a successful flush. */
void wal_truncate(const char *dataDir, const char *metric);

#endif
