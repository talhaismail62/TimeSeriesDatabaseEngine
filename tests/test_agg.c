#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/registry.h"

int main()
{
        // NOTE: This test assumes registry is initialized when you call Head_PUT.
        // Use a dummy data dir; we won’t flush in this test.
        char dataDir[] = "./data_test";

        // insert 4 points
        Head_PUT("m", 0, 1.0, dataDir);
        Head_PUT("m", 10, 2.0, dataDir);
        Head_PUT("m", 20, 3.0, dataDir);
        Head_PUT("m", 30, 4.0, dataDir);

        // bucket = 20 seconds, avg over [0,40)
        char *out = Head_AGG("m", 0, 40, 20, "avg");
        if (!out)
        {
                printf("Failed: out is NULL\n");
                return 1;
        }

        // expected buckets:
        // 0-20 avg(1,2)=1.5
        // 20-40 avg(3,4)=3.5
        if (strstr(out, "0-20") == NULL || strstr(out, "20-40") == NULL)
        {
                printf("Failed: missing buckets\nOutput:\n%s\n", out);
                free(out);
                return 1;
        }

        if (strstr(out, "(2 buckets)") == NULL)
        {
                printf("Failed: wrong bucket count\nOutput:\n%s\n", out);
                free(out);
                return 1;
        }

        printf("Success\n");
        free(out);
        return 0;
}