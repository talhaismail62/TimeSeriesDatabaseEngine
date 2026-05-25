#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/wal.h"
#include "../src/head.h"

#define DATA_DIR "/tmp/test_wal_data"
#define METRIC   "test.metric"

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
}

/* Test 1: append several records, replay into a fresh head block, verify. */
static void test_append_and_replay(void)
{
    setup();

    long   timestamps[] = {1000, 1001, 1002, 1003, 1004};
    double values[]     = {1.1,  2.2,  3.3,  4.4,  5.5};
    int N = 5;

    for (int i = 0; i < N; i++)
        wal_append(DATA_DIR, METRIC, timestamps[i], values[i]);

    HeadBlock *head = getNewHeadBlock();
    wal_replay(DATA_DIR, METRIC, head);

    assert(head->size == N);
    for (int i = 0; i < N; i++) {
        assert(head->timestamps[i] == timestamps[i]);
        assert(head->values[i]     == values[i]);
    }

    free(head->timestamps);
    free(head->values);
    free(head);
    printf("PASS: test_append_and_replay\n");
}

/* Test 2: replay skips records already covered by a flushed chunk
   (i.e., ts <= head->lastTimestamp). */
static void test_replay_skips_already_flushed(void)
{
    setup();

    /* Write 5 records to WAL. */
    for (int i = 0; i < 5; i++)
        wal_append(DATA_DIR, METRIC, 2000 + i, (double)(i + 1));

    /* Simulate that the first 3 have been flushed: set lastTimestamp = 2002. */
    HeadBlock *head = getNewHeadBlock();
    head->lastTimestamp = 2002;

    wal_replay(DATA_DIR, METRIC, head);

    /* Only records with ts > 2002 (i.e., 2003 and 2004) should be replayed. */
    assert(head->size == 2);
    assert(head->timestamps[0] == 2003);
    assert(head->timestamps[1] == 2004);

    free(head->timestamps);
    free(head->values);
    free(head);
    printf("PASS: test_replay_skips_already_flushed\n");
}

/* Test 3: wal_truncate removes the WAL file; a subsequent replay recovers nothing. */
static void test_truncate(void)
{
    setup();

    wal_append(DATA_DIR, METRIC, 3000, 9.9);
    wal_truncate(DATA_DIR, METRIC);

    HeadBlock *head = getNewHeadBlock();
    wal_replay(DATA_DIR, METRIC, head);

    assert(head->size == 0);

    free(head->timestamps);
    free(head->values);
    free(head);
    printf("PASS: test_truncate\n");
}

/* Test 4: replay on a missing WAL file is a no-op (no crash). */
static void test_replay_no_wal(void)
{
    setup();

    HeadBlock *head = getNewHeadBlock();
    wal_replay(DATA_DIR, METRIC, head);   /* wal.log does not exist */

    assert(head->size == 0);

    free(head->timestamps);
    free(head->values);
    free(head);
    printf("PASS: test_replay_no_wal\n");
}

int main(void)
{
    test_append_and_replay();
    test_replay_skips_already_flushed();
    test_truncate();
    test_replay_no_wal();

    rmdir_recursive(DATA_DIR);
    printf("All WAL tests passed.\n");
    return 0;
}
