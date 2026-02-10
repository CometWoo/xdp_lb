#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include <linux/in.h>

#ifndef memcpy
#define memcpy(dest, src, n) __builtin_memcpy((dest), (src), (n))
#endif

#define WEIGHT_S1 5
#define WEIGHT_S2 2
#define TOTAL_WEIGHT (WEIGHT_S1 + WEIGHT_S2)


#define IP_SERVER1 0x0A000102 
#define IP_SERVER2 0x0A000202 

#define VIP_IP 0x0A000001 


#define IFINDEX_S1 11  
#define IFINDEX_S2 13 

static unsigned char MAC_S1[ETH_ALEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x21};
static unsigned char MAC_S2[ETH_ALEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x22};





struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 3);
    __type(key, __u32);
    __type(value, __u32); 
    __uint(pinning, LIBBPF_PIN_BY_NAME);
} lb_stats SEC(".maps");


static __always_inline __u16 csum_fold_helper(__u64 csum) {
    int i;
    for (i = 0; i < 4; i++) {
        if (csum >> 16)
            csum = (csum & 0xffff) + (csum >> 16);
    }
    return ~csum;
}

static __always_inline void ipv4_csum_update(struct iphdr *iph, __be32 old_ip, __be32 new_ip) {
    __u32 size = sizeof(new_ip);
    __u64 l3sum = bpf_csum_diff(&old_ip, size, &new_ip, size, ~iph->check);
    iph->check = csum_fold_helper(l3sum);
}

static __always_inline void udp_csum_update(struct udphdr *udph, __be32 old_ip, __be32 new_ip) {
    __u32 size = sizeof(new_ip);
    __u64 csum = bpf_csum_diff(&old_ip, size, &new_ip, size, ~udph->check);    
    udph->check = csum_fold_helper(csum);
}

SEC("xdp")
int xdp_load_balancer(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) return XDP_PASS;
    if (eth->h_proto != bpf_htons(ETH_P_IP)) return XDP_PASS;

    struct iphdr *iph = (void *)(eth + 1);

    if ((void *)(iph + 1) > data_end) return XDP_PASS;

    
    if (iph->daddr != bpf_htonl(VIP_IP)) {
        return XDP_PASS; 
    }

    if (iph->protocol != IPPROTO_UDP) return XDP_PASS;

    
    struct udphdr *udph = (void *)(iph + 1);
    if ((void *)(udph + 1) > data_end) return XDP_PASS;

    
    __u32 key_idx = 0;
    __u32 *rr_idx = bpf_map_lookup_elem(&lb_stats, &key_idx);
    if (!rr_idx) return XDP_ABORTED;

    
    
    __u32 current_val = *rr_idx;
    *rr_idx += 1; 

    __u32 current_mod = current_val % TOTAL_WEIGHT;

    int target_ifindex;
    unsigned char *target_mac;
    __be32 new_ip;
    __u32 stat_key;

    
    if (current_mod < WEIGHT_S1) {
        target_ifindex = IFINDEX_S1;
        target_mac = MAC_S1;
        new_ip = bpf_htonl(IP_SERVER1);
        stat_key = 1; 
    } else {
        target_ifindex = IFINDEX_S2;
        target_mac = MAC_S2;
        new_ip = bpf_htonl(IP_SERVER2);
        stat_key = 2; 
    }

    
    __u32 *stat_cnt = bpf_map_lookup_elem(&lb_stats, &stat_key);
    if (stat_cnt) {
        *stat_cnt += 1;
    }


    
    memcpy(eth->h_dest, target_mac, ETH_ALEN);
    
    __be32 old_ip = iph->daddr; 
    iph->daddr = new_ip;        
    
    
    ipv4_csum_update(iph, old_ip, new_ip);
    
    
    udp_csum_update(udph, old_ip, new_ip);

    return bpf_redirect(target_ifindex, 0);
}

char _license[] SEC("license") = "GPL";
