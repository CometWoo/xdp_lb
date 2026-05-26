#!/usr/bin/env bash
# TSN experiment runner — assumes `make create` already built the topology.
# Use `make tsn` first for protected run, `make tsn-off` for baseline.
set -u

NS_C=client
NS_S1=server1
DST=10.0.1.2
COUNT=${COUNT:-2000}
BE_DUR=${BE_DUR:-8}

echo "[*] cleanup leftover processes"
ip netns exec $NS_S1 pkill -f tt_receiver.py 2>/dev/null
ip netns exec $NS_S1 pkill -f iperf3        2>/dev/null
ip netns exec $NS_C  pkill -f iperf3        2>/dev/null
sleep 1

echo "[*] current qdisc on veth-rs1:"
ip netns exec router tc qdisc show dev veth-rs1

echo "[*] start TT receiver on $NS_S1"
ip netns exec $NS_S1 python3 tt_receiver.py 0.0.0.0 $COUNT > recv.log 2>&1 &
RECV_PID=$!
sleep 1

echo "[*] start iperf3 server on $NS_S1 (one-shot, BE sink)"
ip netns exec $NS_S1 iperf3 -s -1 -p 5201 > iperf-srv.log 2>&1 &
sleep 1

echo "[*] start iperf3 UDP BE load: $NS_C -> $DST for ${BE_DUR}s"
ip netns exec $NS_C iperf3 -c $DST -p 5201 -u -b 0 -l 1400 -t $BE_DUR \
    > iperf-cli.log 2>&1 &
IPERF_PID=$!

echo "[*] start TT sender on $NS_C (count=$COUNT, 1ms cycle)"
ip netns exec $NS_C python3 tt_sender.py $DST $COUNT

wait $RECV_PID 2>/dev/null
wait $IPERF_PID 2>/dev/null

echo
echo "===================== TT RESULT ====================="
cat recv.log
echo
echo "===================== BE RESULT ====================="
tail -n 10 iperf-cli.log
echo "====================================================="
