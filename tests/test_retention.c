#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/retention.h"

/* ------------------------------------------------------------------ */
/* Config parsing tests                                                 */
/* ------------------------------------------------------------------ */

static void test_add_rule_valid(void)
{
    RetentionConfig cfg = {.count = 0};
    assert(retention_add_rule(&cfg, "cpu.usage=86400") == 0);
    assert(cfg.count == 1);
    assert(strcmp(cfg.rules[0].metric, "cpu.usage") == 0);
    assert(cfg.rules[0].max_age_seconds == 86400);
    printf("PASS: test_add_rule_valid\n");
}

static void test_add_rule_invalid(void)
{
    RetentionConfig cfg = {.count = 0};
    assert(retention_add_rule(&cfg, "no-equals-sign") == -1);
    assert(retention_add_rule(&cfg, "=3600")          == -1); /* empty metric */
    assert(retention_add_rule(&cfg, "cpu.usage=0")    == -1); /* zero age */
    assert(retention_add_rule(&cfg, "cpu.usage=-1")   == -1); /* negative age */
    assert(cfg.count == 0);
    printf("PASS: test_add_rule_invalid\n");
}

static void test_get_rule(void)
{
    RetentionConfig cfg = {.count = 0};
    retention_add_rule(&cfg, "cpu.usage=3600");
    retention_add_rule(&cfg, "mem.used=7200");

    assert(retention_get(&cfg, "cpu.usage") == 3600);
    assert(retention_get(&cfg, "mem.used")  == 7200);
    assert(retention_get(&cfg, "disk.io")   == -1); /* no rule */
    printf("PASS: test_get_rule\n");
}

static void test_multiple_rules(void)
{
    RetentionConfig cfg = {.count = 0};
    char spec[64];
    /* Add several rules */
    for (int i = 1; i <= 10; i++) {
        snprintf(spec, sizeof(spec), "metric%d=%d", i, i * 1000);
        assert(retention_add_rule(&cfg, spec) == 0);
    }
    assert(cfg.count == 10);
    for (int i = 1; i <= 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "metric%d", i);
        assert(retention_get(&cfg, name) == i * 1000);
    }
    printf("PASS: test_multiple_rules\n");
}

/* ------------------------------------------------------------------ */
/* Expiry logic test (unit-level, no real thread)                       */
/* We verify that chunk files whose end_ts is past the cutoff are      */
/* identified correctly using retention_get.                            */
/* ------------------------------------------------------------------ */

static void test_expiry_cutoff_logic(void)
{
    RetentionConfig cfg = {.count = 0};
    retention_add_rule(&cfg, "sensor.temp=3600"); /* 1-hour retention */

    long now    = (long)time(NULL);
    long max_age = retention_get(&cfg, "sensor.temp");
    long cutoff  = now - max_age;

    /* A chunk that ended 2 hours ago should be expired. */
    long old_end_ts  = now - 7200;
    /* A chunk that ended 30 minutes ago should be kept. */
    long new_end_ts  = now - 1800;

    assert(old_end_ts < cutoff); /* should be deleted */
    assert(new_end_ts >= cutoff); /* should be kept   */
    printf("PASS: test_expiry_cutoff_logic\n");
}

int main(void)
{
    test_add_rule_valid();
    test_add_rule_invalid();
    test_get_rule();
    test_multiple_rules();
    test_expiry_cutoff_logic();

    printf("All retention tests passed.\n");
    return 0;
}
