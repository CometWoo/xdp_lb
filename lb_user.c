#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <time.h>
#include <errno.h>

#define MAP_PATH "/sys/fs/bpf/lb_stats"


unsigned int get_sum_per_cpu(int map_fd, __u32 key, unsigned int num_cpus) {
    unsigned int total = 0;
    
    __u32 *values = calloc(num_cpus, sizeof(__u32));
    if (!values) {
        perror("calloc failed");
        return 0;
    }

    
    if (bpf_map_lookup_elem(map_fd, &key, values) != 0) {
        free(values);
        return 0; 
    }

    
    for (int i = 0; i < num_cpus; i++) {
        total += values[i];
    }

    free(values);
    return total;
}

int main() {
    int map_fd = bpf_obj_get(MAP_PATH); 
    if (map_fd < 0) {
        fprintf(stderr, "ERROR: 맵을 열 수 없습니다. (%s)\n", MAP_PATH);
        return 1;
    }

    
    int num_cpus = libbpf_num_possible_cpus();
    if (num_cpus < 0) {
        fprintf(stderr, "CPU 개수를 확인할 수 없습니다.\n");
        return 1;
    }

    printf("Monitoring (Per-CPU Mode) - CPUs: %d\n", num_cpus);
    printf("Expected Ratio -> S1(5) : S2(2)\n");
    printf("%-10s | %-15s | %-15s | %-15s\n", "Time", "Server 1", "Server 2", "Ratio");
    printf("-----------|-----------------|-----------------|----------------\n");

    while (1) {
        
        unsigned int s1_total = get_sum_per_cpu(map_fd, 1, num_cpus);
        unsigned int s2_total = get_sum_per_cpu(map_fd, 2, num_cpus);

        double ratio = 0.0;
        if (s2_total > 0) {
            ratio = (double)s1_total / (double)s2_total;
        }

        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        char time_buf[10];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm);

        printf("\r%s   | %-15u | %-15u | %.2f : 1        ", 
               time_buf, s1_total, s2_total, ratio);
        
        fflush(stdout);
        sleep(1);
    }

    return 0;
}
