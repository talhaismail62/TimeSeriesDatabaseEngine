#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static double now_seconds()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static int connect_tcp(const char *host, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
    {
        struct hostent *he = gethostbyname(host);
        if (!he || he->h_addrtype != AF_INET)
        {
            close(sock);
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        return -1;
    }

    return sock;
}

static int send_all(int sock, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    size_t off = 0;
    while (off < len)
    {
        ssize_t w = send(sock, p + off, len - off, 0);
        if (w <= 0)
            return -1;
        off += (size_t)w;
    }
    return 0;
}

static int recv_some(int sock, char *buf, int cap)
{
    ssize_t r = recv(sock, buf, cap - 1, 0);
    if (r <= 0)
        return -1;
    buf[r] = '\0';
    return (int)r;
}

static int count_ok_tokens(const char *buf, int len)
{
    int c = 0;
    for (int i = 0; i + 1 < len; i++)
    {
        if (buf[i] == 'o' && buf[i + 1] == 'k')
            c++;
    }
    return c;
}

static double rand_unit()
{
    return (double)rand() / (double)RAND_MAX;
}

static double rand_drift()
{
    return rand_unit() * 2.0 - 1.0;
}

static long long chunk_files_size_bytes(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir)
        return 0;

    long long total = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL)
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            total += chunk_files_size_bytes(full);
        }
        else if (S_ISREG(st.st_mode))
        {
            const char *dot = strrchr(ent->d_name, '.');
            if (dot && strcmp(dot, ".chunk") == 0)
            {
                total += (long long)st.st_size;
            }
        }
    }

    closedir(dir);
    return total;
}

static int write_naive_file(const char *path, const long *timestamps, const double *values, int n)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;

    for (int i = 0; i < n; i++)
    {
        if (fwrite(&timestamps[i], 8, 1, f) != 1)
        {
            fclose(f);
            return -1;
        }
        if (fwrite(&values[i], 8, 1, f) != 1)
        {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <host> <port> <data_dir>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    const char *data_dir = argv[3];

    srand(1);

    const int metrics = 10;
    const int points_per_metric = 50000;
    const int total_points = metrics * points_per_metric;

    long *timestamps = (long *)malloc(sizeof(long) * total_points);
    double *values = (double *)malloc(sizeof(double) * total_points);
    if (!timestamps || !values)
    {
        fprintf(stderr, "OOM\n");
        return 1;
    }

    char metric_names[10][32];
    for (int m = 0; m < metrics; m++)
    {
        snprintf(metric_names[m], sizeof(metric_names[m]), "m%d", m);
    }

    long base_ts = 1728000000;

    int idx = 0;
    for (int m = 0; m < metrics; m++)
    {
        for (int i = 0; i < points_per_metric; i++)
        {
            long ts = base_ts + i;

            double v = 50.0 + (double)m;
            if (i > 15000 && i < 30000)
            {
                v += 1.5;
            }
            else if (i >= 30000)
            {
                v -= 0.75;
            }

            timestamps[idx] = ts;
            values[idx] = v;
            idx++;
        }
    }

    int sock = connect_tcp(host, port);
    if (sock < 0)
    {
        fprintf(stderr, "connect failed to %s:%d\n", host, port);
        return 1;
    }

    const int BATCH = 2000;
    char *outbuf = (char *)malloc(1024 * 1024);
    if (!outbuf)
    {
        fprintf(stderr, "OOM\n");
        close(sock);
        return 1;
    }

    char resp[1 << 15];
    double t0 = now_seconds();

    int pending = 0;
    int sent = 0;
    int acked = 0;
    int outlen = 0;

    for (int m = 0; m < metrics; m++)
    {
        for (int i = 0; i < points_per_metric; i++)
        {
            int current_array_idx = (m * points_per_metric) + i;

            long ts = timestamps[current_array_idx];
            double v = values[current_array_idx];

            if (sent < 5)
            {
                printf("BENCHMARK SENDING COMPLIANT: PUT %s %ld %.6f\n", metric_names[m], ts, v);
            }
            printf("INSERTING:  PUT %s %ld %.6f\n", metric_names[m], ts, v);

            char line[128];
            int n = snprintf(line, sizeof(line), "PUT %s %ld %.6f\n", metric_names[m], ts, v);

            if (outlen + n >= 1024 * 1024)
            {
                if (send_all(sock, outbuf, (size_t)outlen) != 0)
                {
                    fprintf(stderr, "send_all failed\n");
                    close(sock);
                    return 1;
                }
                outlen = 0;
            }

            memcpy(outbuf + outlen, line, (size_t)n);
            outlen += n;

            pending++;
            sent++;

            if (pending >= BATCH)
            {
                if (outlen > 0)
                {
                    if (send_all(sock, outbuf, (size_t)outlen) != 0)
                    {
                        fprintf(stderr, "send_all failed\n");
                        close(sock);
                        return 1;
                    }
                    outlen = 0;
                }

                int need = pending;
                while (need > 0)
                {
                    int r = recv_some(sock, resp, (int)sizeof(resp));
                    if (r < 0)
                    {
                        fprintf(stderr, "recv failed while waiting for acks\n");
                        close(sock);
                        return 1;
                    }
                    int got = count_ok_tokens(resp, r);
                    need -= got;
                    acked += got;
                }
                pending = 0;
            }
        }
    }

    if (outlen > 0)
    {
        if (send_all(sock, outbuf, (size_t)outlen) != 0)
        {
            fprintf(stderr, "send_all failed\n");
            close(sock);
            return 1;
        }
        outlen = 0;
    }

    int need = pending;
    while (need > 0)
    {
        int r = recv_some(sock, resp, (int)sizeof(resp));
        if (r < 0)
        {
            fprintf(stderr, "recv failed while waiting for final acks\n");
            close(sock);
            return 1;
        }
        int got = count_ok_tokens(resp, r);
        need -= got;
        acked += got;
    }

    double t1 = now_seconds();
    double elapsed = t1 - t0;
    double pps = (double)sent / elapsed;

    printf("Inserted %d points in %.3f seconds => %.0f points/sec\n", sent, elapsed, pps);

    for (int m = 0; m < metrics; m++)
    {
        char line[64];
        int n = snprintf(line, sizeof(line), "FLUSH %s\n", metric_names[m]);
        if (send_all(sock, line, (size_t)n) != 0)
        {
            fprintf(stderr, "flush send failed\n");
            break;
        }
        if (recv_some(sock, resp, (int)sizeof(resp)) < 0)
        {
            fprintf(stderr, "flush recv failed\n");
            break;
        }
    }

    long long disk_bytes = chunk_files_size_bytes(data_dir);
    long long naive_bytes = (long long)total_points * 16LL;

    (void)write_naive_file("./naive.bin", timestamps, values, total_points);

    printf("Naive bytes (16/point): %lld\n", naive_bytes);
    printf("Chunk bytes on disk:    %lld\n", disk_bytes);

    if (disk_bytes > 0)
    {
        double ratio = (double)naive_bytes / (double)disk_bytes;
        printf("Compression ratio: %.2fx\n", ratio);
    }
    else
    {
        printf("Compression ratio: N/A\n");
    }

    {
        char line[128];
        int n = snprintf(line, sizeof(line), "GET %s %ld %ld\n", metric_names[0], base_ts, base_ts + 10);
        if (send_all(sock, line, (size_t)n) == 0)
        {
            int r = recv_some(sock, resp, (int)sizeof(resp));
            if (r > 0)
            {
                printf("Sample GET response:\n%s\n", resp);
            }
        }
    }

    {   
        const char *q = "QUIT\n";
        (void)send_all(sock, q, strlen(q));
    }

    close(sock);
    free(outbuf);
    free(timestamps);
    free(values);
    return 0;
}