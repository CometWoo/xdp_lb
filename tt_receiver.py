#!/usr/bin/env python3
"""TT (Time-Triggered) UDP receiver — measures one-way latency & jitter.

Single-VM assumption: sender and receiver share the same monotonic clock,
so one-way latency = recv_ts - send_ts is meaningful.
"""
import socket
import struct
import sys
import time

LISTEN_IP = sys.argv[1] if len(sys.argv) > 1 else "0.0.0.0"
EXPECTED = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
LISTEN_PORT = 9999

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((LISTEN_IP, LISTEN_PORT))
sock.settimeout(15)

print(f"[TT-RECV] listen {LISTEN_IP}:{LISTEN_PORT} expecting {EXPECTED} pkts",
      flush=True)

latencies_ns = []
gaps_ns = []
prev_recv_ts = None
recv_count = 0
last_seq = -1
out_of_order = 0

try:
    while recv_count < EXPECTED:
        data, _ = sock.recvfrom(2048)
        recv_ts = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
        seq, send_ts = struct.unpack("!QQ", data[:16])
        if seq < last_seq:
            out_of_order += 1
        last_seq = max(last_seq, seq)
        latencies_ns.append(recv_ts - send_ts)
        if prev_recv_ts is not None:
            gaps_ns.append(recv_ts - prev_recv_ts)
        prev_recv_ts = recv_ts
        recv_count += 1
except socket.timeout:
    print(f"[TT-RECV] timeout — got {recv_count}/{EXPECTED}", flush=True)

if not latencies_ns:
    print("[TT-RECV] no packets received", flush=True)
    sys.exit(1)

def stats(xs):
    xs = sorted(xs)
    n = len(xs)
    return {
        "n": n,
        "min": xs[0],
        "avg": sum(xs) / n,
        "max": xs[-1],
        "p50": xs[n // 2],
        "p99": xs[min(n - 1, int(n * 0.99))],
    }

lat_us = [x / 1000 for x in latencies_ns]
s = stats(lat_us)
print()
print(f"[TT-RECV] received {recv_count}/{EXPECTED}  loss={EXPECTED - recv_count}"
      f"  out_of_order={out_of_order}", flush=True)
print(f"  latency  (us)  min={s['min']:8.2f}  avg={s['avg']:8.2f}"
      f"  p50={s['p50']:8.2f}  p99={s['p99']:8.2f}  max={s['max']:8.2f}", flush=True)
print(f"  jitter   (max-min, us): {s['max'] - s['min']:.2f}", flush=True)

if gaps_ns:
    gs = stats([g / 1000 for g in gaps_ns])
    print(f"  gap      (us)  min={gs['min']:8.2f}  avg={gs['avg']:8.2f}"
          f"  p99={gs['p99']:8.2f}  max={gs['max']:8.2f}", flush=True)
