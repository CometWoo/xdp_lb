// User-space monitor for the TSN packet-counter BPF maps.
// Mirrors the lb_user.c pattern.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <bpf/bpf.h>

#define PIN_PKTS  "/sys/fs/bpf/tsn_pkts"
#define PIN_BYTES "/sys/fs/bpf/tsn_bytes"

static const char *LABELS[3] = {
    "TT (UDP/9999)",
    "BE (port 5201)",
    "OTHER",
};

static unsigned long long lookup(int fd, unsigned int key) {
    unsigned long long v = 0;
    bpf_map_lookup_elem(fd, &key, &v);
    return v;
}

int main(void) {
    int pfd = bpf_obj_get(PIN_PKTS);
    int bfd = bpf_obj_get(PIN_BYTES);
    if (pfd < 0 || bfd < 0) {
        fprintf(stderr, "ERROR: cannot open pinned maps (%s / %s).\n"
                        "Did you run `make count-load`?\n", PIN_PKTS, PIN_BYTES);
        return 1;
    }

    unsigned long long prev_p[3] = {0}, prev_b[3] = {0};
    int first = 1;
    printf("%-9s | %-15s | %12s | %14s | %10s\n",
           "time", "class", "packets", "bytes", "pps");
    printf("----------|-----------------|--------------|----------------|------------\n");

    while (1) {
        time_t now = time(NULL);
        char tb[10];
        strftime(tb, sizeof(tb), "%H:%M:%S", localtime(&now));
        for (int i = 0; i < 3; i++) {
            unsigned long long p = lookup(pfd, i);
            unsigned long long b = lookup(bfd, i);
            unsigned long long pps = first ? 0 : (p - prev_p[i]);
            printf("%-9s | %-15s | %12llu | %14llu | %10llu\n",
                   i == 0 ? tb : "", LABELS[i], p, b, pps);
            prev_p[i] = p;
            prev_b[i] = b;
        }
        printf("----------|-----------------|--------------|----------------|------------\n");
        fflush(stdout);
        first = 0;
        sleep(1);
    }
    return 0;
}
