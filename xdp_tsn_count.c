// SPDX-License-Identifier: GPL-2.0
// XDP packet counter for TSN experiment verification.
// Classifies arriving packets into TT (UDP/9999), BE (TCP|UDP/5201), OTHER.
// Always returns XDP_PASS — purely observational.
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define KEY_TT     0
#define KEY_BE     1
#define KEY_OTHER  2

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 3);
    __type(key, __u32);
    __type(value, __u64);
    __uint(pinning, LIBBPF_PIN_BY_NAME);
} tsn_pkts SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 3);
    __type(key, __u32);
    __type(value, __u64);
    __uint(pinning, LIBBPF_PIN_BY_NAME);
} tsn_bytes SEC(".maps");

SEC("xdp")
int xdp_tsn_count(struct xdp_md *ctx) {
    void *end = (void *)(long)ctx->data_end;
    void *cur = (void *)(long)ctx->data;
    __u64 plen = (__u64)(end - cur);

    struct ethhdr *eth = cur;
    if ((void *)(eth + 1) > end) return XDP_PASS;
    if (eth->h_proto != bpf_htons(ETH_P_IP)) return XDP_PASS;

    struct iphdr *iph = (void *)(eth + 1);
    if ((void *)(iph + 1) > end) return XDP_PASS;
    if (iph->ihl < 5) return XDP_PASS;

    __u32 k = KEY_OTHER;
    void *l4 = (void *)iph + (iph->ihl * 4);

    if (iph->protocol == IPPROTO_UDP) {
        struct udphdr *u = l4;
        if ((void *)(u + 1) > end) return XDP_PASS;
        __u16 dp = bpf_ntohs(u->dest);
        if (dp == 9999) k = KEY_TT;
        else if (dp == 5201) k = KEY_BE;
    } else if (iph->protocol == IPPROTO_TCP) {
        struct tcphdr *t = l4;
        if ((void *)(t + 1) > end) return XDP_PASS;
        __u16 dp = bpf_ntohs(t->dest);
        if (dp == 5201) k = KEY_BE;
    }

    __u64 *p = bpf_map_lookup_elem(&tsn_pkts, &k);
    if (p) __sync_fetch_and_add(p, 1);
    __u64 *b = bpf_map_lookup_elem(&tsn_bytes, &k);
    if (b) __sync_fetch_and_add(b, plen);

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
