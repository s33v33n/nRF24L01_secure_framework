#!/bin/bash
# ==============================================================================
# nRF24L01+ Secure Network Configuration Generator
# Generates default configuration in user_config/user_config.h
# - Cryptographically secure 256-bit ChaCha20-Poly1305 key from /dev/urandom
# - Random 40-bit Master address
# - Node 0: "master", NRF_MASTER_ADDRESS, ID 0, channel 1, power 1, speed 1
# - Node 1..N: "nrf_dev_1".. "nrf_dev_N", 0x0000000001ULL.. , channel 1, power 1, speed 1
# ==============================================================================

set -euo pipefail

TARGET_FILE="user_config/user_config.h"
NUM_NODES=${1:-}

# how many devices
if [ -z "$NUM_NODES" ]; then
    read -p "Enter number of nodes in network [default: 2]: " input_nodes
    NUM_NODES=${input_nodes:-2}
fi

if ! [[ "$NUM_NODES" =~ ^[0-9]+$ ]] || [ "$NUM_NODES" -lt 2 ]; then
    echo "Warning: Minimum 2 nodes required (Master + at least 1 Node). Defaulting to 2."
    NUM_NODES=2
fi

generate_rf_addr() {
    while true; do
        raw=$(hexdump -vn 5 -e '5/1 "%02X"' /dev/urandom)
        first_b="${raw:0:2}"
        if [ "$first_b" != "55" ] && [ "$first_b" != "AA" ] && \
           [ "$first_b" != "00" ] && [ "$first_b" != "FF" ] && \
           [ "$raw" != "0000000000" ] && [ "$raw" != "FFFFFFFFFF" ]; then
            echo "0x${raw}ULL"
            break
        fi
    done
}

# 1. random master address
MASTER_ADDR=$(generate_rf_addr)

# 2. random 256-bit key
KEY_WORDS=($(od -vAn -N32 -tx4 /dev/urandom))

# 3. create original file backup
if [ -f "$TARGET_FILE" ]; then
    cp "$TARGET_FILE" "${TARGET_FILE}.bak"
fi

# 4. save config file
cat <<EOF > "$TARGET_FILE"
#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include <stdint.h>
#include "hardware_config.h"

#define NRF_MASTER_ADDRESS ${MASTER_ADDR}

static const uint32_t NRF_NETWORK_KEY[8] = {
    0x${KEY_WORDS[0]}, 0x${KEY_WORDS[1]}, 0x${KEY_WORDS[2]}, 0x${KEY_WORDS[3]},
    0x${KEY_WORDS[4]}, 0x${KEY_WORDS[5]}, 0x${KEY_WORDS[6]}, 0x${KEY_WORDS[7]}
};

struct nrf_node_config{
    char *name;             // must be uniqe
    uint64_t tx_address;
    uint64_t rx_pipe_addr;  // must be uniqe
    uint8_t rx_pipe_num;    // only 0 allowed
    uint8_t id;             // must be uniqe and starts from 0. 0 - is only for master
    uint8_t channel;
    uint8_t power;
    uint8_t speed;
};


// ------ My nodes ------
static const struct nrf_node_config nrf_nodes[] = {
EOF

for ((i = 0; i < NUM_NODES; i++)); do
    comma=","
    if [ "$i" -eq "$((NUM_NODES - 1))" ]; then
        comma=""
    fi

    if [ "$i" -eq 0 ]; then
        echo "    {DEV_NAME_SPI0, 0x00, NRF_MASTER_ADDRESS, 0, 0, 15, 0, 1}${comma}" >> "$TARGET_FILE"
    else
        while true; do
            NODE_ADDR=$(generate_rf_addr)
            if [ "$NODE_ADDR" != "$MASTER_ADDR" ]; then
                break
            fi
        done

        if [ "$i" -eq 1 ]; then
            echo "    {DEV_NAME_SPI1, 0x00, ${NODE_ADDR}, 0, ${i}, 15, 0, 1}${comma}" >> "$TARGET_FILE"
        else
            echo "    {DEV_NAME_SPI0, 0x00, ${NODE_ADDR}, 0, ${i}, 15, 0, 1}${comma}" >> "$TARGET_FILE"
        fi
    fi
done

cat <<EOF >> "$TARGET_FILE"
};

#endif // USER_CONFIG_H
EOF

echo "Generated default secure configuration in ${TARGET_FILE}:"
echo "  - Master Address : ${MASTER_ADDR}"
echo "  - Network Key    : 256 bits random (ChaCha20-Poly1305)"
echo "  - Total Nodes    : ${NUM_NODES} (Node 0: DEV_NAME_SPI0, Node 1: DEV_NAME_SPI1..)"
echo "  - Channels/Power : channel=15, power=0, speed=1, pipe=0"
