#!/usr/bin/env python3
"""TT (Time-Triggered) UDP sender.

Sends one small UDP packet every CYCLE_NS with SO_PRIORITY=3 so that taprio
on the router maps it to TC0 (the TT gate). Embeds (seq, send_ts) in the
payload so the receiver can compute one-way latency.
"""
import socket
import struct
import sys
import time

DST_IP = sys.argv[1] if len(sys.argv) > 1 else "10.0.1.2"
COUNT = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
DST_PORT = 9999
PRIORITY = 3          # -> TC0 in our taprio map
CYCLE_NS = 1_000_000  # 1 ms

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_PRIORITY, PRIORITY)

print(f"[TT-SEND] dst={DST_IP}:{DST_PORT} count={COUNT} prio={PRIORITY} cycle={CYCLE_NS}ns",
      flush=True)

start = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
for seq in range(COUNT):
    target = start + seq * CYCLE_NS
    # short busy-wait for cycle precision
    while time.clock_gettime_ns(time.CLOCK_MONOTONIC) < target:
        pass
    send_ts = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    payload = struct.pack("!QQ", seq, send_ts) + b"X" * 48  # 64B
    sock.sendto(payload, (DST_IP, DST_PORT))

print("[TT-SEND] done", flush=True)
