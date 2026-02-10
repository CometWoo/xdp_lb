# 가상 네트워크 토폴로지 구축용 Makefile

# --- 네임스페이스 정의 ---
NS_C := client
NS_R := router
NS_S1 := server1
NS_S2 := server2

# Client <-> Router
VETH_C := veth-c
VETH_RC := veth-rc
# Server1 <-> Router
VETH_S1 := veth-s1
VETH_RS1 := veth-rs1
# Server2 <-> Router
VETH_S2 := veth-s2
VETH_RS2 := veth-rs2

# --- IP 대역 정의 ---
IP_NET_C := 10.0.0
IP_NET_S1 := 10.0.1
IP_NET_S2 := 10.0.2

.PHONY: all create teardown test info

all: create info

create:
	@echo "Creating namespaces..."
	ip netns add $(NS_C)
	ip netns add $(NS_R)
	ip netns add $(NS_S1)
	ip netns add $(NS_S2)

	@echo "Creating veth pairs..."
	# Link: Client <-> Router
	ip link add $(VETH_C) type veth peer name $(VETH_RC)
	ip link set $(VETH_C) netns $(NS_C)
	ip link set $(VETH_RC) netns $(NS_R)

	# Link: Server1 <-> Router
	ip link add $(VETH_S1) type veth peer name $(VETH_RS1)
	ip link set $(VETH_S1) netns $(NS_S1)
	ip link set $(VETH_RS1) netns $(NS_R)

	# Link: Server2 <-> Router
	ip link add $(VETH_S2) type veth peer name $(VETH_RS2)
	ip link set $(VETH_S2) netns $(NS_S2)
	ip link set $(VETH_RS2) netns $(NS_R)

	@echo "Configuring Interfaces (IPs & MACs)..."
	# --- Client ---
	ip netns exec $(NS_C) ip link set dev $(VETH_C) address 02:00:00:00:00:01
	ip netns exec $(NS_C) ip addr add $(IP_NET_C).2/24 dev $(VETH_C)
	ip netns exec $(NS_C) ip link set $(VETH_C) up
	ip netns exec $(NS_C) ip route add default via $(IP_NET_C).1

	# --- Router ---
	# Client facing
	ip netns exec $(NS_R) ip link set dev $(VETH_RC) address 02:00:00:00:00:11
	ip netns exec $(NS_R) ip addr add $(IP_NET_C).1/24 dev $(VETH_RC)
	ip netns exec $(NS_R) ip link set $(VETH_RC) up
	# Server1 facing
	ip netns exec $(NS_R) ip link set dev $(VETH_RS1) address 02:00:00:00:00:12
	ip netns exec $(NS_R) ip addr add $(IP_NET_S1).1/24 dev $(VETH_RS1)
	ip netns exec $(NS_R) ip link set $(VETH_RS1) up
	# Server2 facing
	ip netns exec $(NS_R) ip link set dev $(VETH_RS2) address 02:00:00:00:00:13
	ip netns exec $(NS_R) ip addr add $(IP_NET_S2).1/24 dev $(VETH_RS2)
	ip netns exec $(NS_R) ip link set $(VETH_RS2) up
	# Enable Forwarding
	ip netns exec $(NS_R) sysctl -w net.ipv4.ip_forward=1 > /dev/null

	# --- Server 1 ---
	ip netns exec $(NS_S1) ip link set dev $(VETH_S1) address 02:00:00:00:00:21
	ip netns exec $(NS_S1) ip addr add $(IP_NET_S1).2/24 dev $(VETH_S1)
	ip netns exec $(NS_S1) ip link set $(VETH_S1) up
	ip netns exec $(NS_S1) ip route add default via $(IP_NET_S1).1

	# --- Server 2 ---
	ip netns exec $(NS_S2) ip link set dev $(VETH_S2) address 02:00:00:00:00:22
	ip netns exec $(NS_S2) ip addr add $(IP_NET_S2).2/24 dev $(VETH_S2)
	ip netns exec $(NS_S2) ip link set $(VETH_S2) up
	ip netns exec $(NS_S2) ip route add default via $(IP_NET_S2).1

	@echo "Applying Traffic Control (Bandwidth Limits)..."
	# XDP 실험을 위해 Offloading 끄기 (권장)
	ip netns exec $(NS_R) ethtool -K $(VETH_RC) tx off rx off gro off gso off > /dev/null 2>&1 || true
	ip netns exec $(NS_R) ethtool -K $(VETH_RS1) tx off rx off gro off gso off > /dev/null 2>&1 || true
	ip netns exec $(NS_R) ethtool -K $(VETH_RS2) tx off rx off gro off gso off > /dev/null 2>&1 || true

	# Bandwidth Limit 
	# Server 1 Link: 2.5Gbps
	ip netns exec $(NS_R) tc qdisc add dev $(VETH_RS1) root tbf rate 2.5gbit burst 32kbit latency 400ms
	# Server 2 Link: 1Gbps
	ip netns exec $(NS_R) tc qdisc add dev $(VETH_RS2) root tbf rate 1gbit burst 32kbit latency 400ms
	# Client Link: 3Gbps
	ip netns exec $(NS_C) tc qdisc add dev $(VETH_C) root tbf rate 3gbit burst 32kbit latency 400ms

	@echo "Topology Setup Complete."

teardown:
	@echo "Cleaning up namespaces..."
	-ip netns delete $(NS_C)
	-ip netns delete $(NS_R)
	-ip netns delete $(NS_S1)
	-ip netns delete $(NS_S2)
	@echo "Cleanup Complete."

test:
	@echo "Testing connectivity (Ping)..."
	@echo "1. Client -> Router (Gateway)"
	@ip netns exec $(NS_C) ping -c 2 $(IP_NET_C).1
	@echo "2. Client -> Server 1"
	@ip netns exec $(NS_C) ping -c 2 $(IP_NET_S1).2
	@echo "3. Client -> Server 2"
	@ip netns exec $(NS_C) ping -c 2 $(IP_NET_S2).2

info:
	@echo "========================================"
	@echo " [ Topology Info ]"
	@echo " Client ($(NS_C)): $(VETH_C) IP: $(IP_NET_C).2"
	@echo " Router ($(NS_R)): $(VETH_RC) / $(VETH_RS1) / $(VETH_RS2)"
	@echo " Server1 ($(NS_S1)): $(VETH_S1) IP: $(IP_NET_S1).2 (BW: 2.5Gbps)"
	@echo " Server2 ($(NS_S2)): $(VETH_S2) IP: $(IP_NET_S2).2 (BW: 1Gbps)"
	@echo "========================================"
