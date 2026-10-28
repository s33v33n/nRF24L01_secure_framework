#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "nrf_packet.h"
#include "../user_config/user_config.h"
#include "../encryption/Chacha20-Poly1305.h"

#define ALL_DEVICES (sizeof(nrf_nodes) / sizeof((nrf_nodes)[0]))

static uint32_t my_tx_counter = 1;
static uint32_t last_rx_counter[ALL_DEVICES] = {0};

int send_packet(int fd, uint8_t sender_id, const char *message) {

    if (fd < 0 || message == NULL) {
        return -1;
    }

    // Packet
    uint8_t package_to_send[32] = {0};
    uint32_t current_counter = my_tx_counter++;

    // counter value
    package_to_send[0] = (uint8_t)(current_counter);
    package_to_send[1] = (uint8_t)(current_counter >> 8);
    package_to_send[2] = (uint8_t)(current_counter >> 16);
    package_to_send[3] = (uint8_t)(current_counter >> 24);

    // node ID
    package_to_send[4] = sender_id;

    // check mess length
    size_t msg_len = strlen(message);
    if (msg_len > 11) {
        msg_len = 11;
        fprintf(stderr, "Warning: Message exceeds 11 chars (got %zu), truncating!\n", msg_len);
    }

    // generate chacha blocks
    const uint32_t nonce[NONCE_BLOCKS] = {0, 0, current_counter};
    uint32_t chacha_block[CHACHA_BLOCKS] = {0};
    generate_chacha20_block(NRF_NETWORK_KEY, 0, nonce, chacha_block);

    // encrypt message (all 11 bytes padded with 0 to prevent garbage on decryption and hide length)
    uint8_t *keystream = (uint8_t *)chacha_block;
    for (size_t i = 0; i < 11; i++) {
        uint8_t byte_to_enc = (i < msg_len) ? (uint8_t)message[i] : 0;
        package_to_send[5 + i] = byte_to_enc ^ keystream[32 + i];
    }

    // create poly tag
    uint8_t tag[POLY_BLOCKS] = {0};
    poly1305(chacha_block, package_to_send, 16, tag);
    memcpy(&package_to_send[16], tag, POLY_BLOCKS);

    // send package
    ssize_t bytes_sent = write(fd, package_to_send, sizeof(package_to_send));
    if (bytes_sent != (ssize_t)sizeof(package_to_send)) {
        perror("write: Failed to send packet");
        return -1;
    }

    return 0;
}

int receive_packet(int fd, uint8_t *out_sender_id, char *out_message, size_t max_len) {

    if (fd < 0 || out_message == NULL || max_len == 0) {
        return -1;
    }

    uint8_t packet[32] = {0};

    ssize_t bytes_read = read(fd, packet, sizeof(packet));
    if (bytes_read != (ssize_t)sizeof(packet)) {
        return -1;
    }

    uint32_t rx_counter = (uint32_t)packet[0] |
                          ((uint32_t)packet[1] << 8) |
                          ((uint32_t)packet[2] << 16) |
                          ((uint32_t)packet[3] << 24);

    uint8_t sender_id = packet[4];

    // check sender ID
    if (sender_id >= ALL_DEVICES) {
        fprintf(stderr, "Received packet from unknown node ID=%u!\n", sender_id);
        return -1;
    }

    // generate chacha blocks
    const uint32_t nonce[NONCE_BLOCKS] = {0, 0, rx_counter};
    uint32_t chacha_block[CHACHA_BLOCKS] = {0};
    generate_chacha20_block(NRF_NETWORK_KEY, 0, nonce, chacha_block);

    // Poly authenticate
    uint8_t expected_tag[POLY_BLOCKS] = {0};
    poly1305(chacha_block, packet, 16, expected_tag);

    // tag verification
    if (!verify_tag(&packet[16], expected_tag)) {
        fprintf(stderr, "Error! Poly1305 tag mismatch! Packet corrupted or tampered.\n");
        return -1;
    }

    // Reply attackt shield
    if (rx_counter <= last_rx_counter[sender_id]) {
        fprintf(stderr, "Replay attack detected from node ID=%u!\n", sender_id);
        return -1;
    }

    // counter update
    last_rx_counter[sender_id] = rx_counter;

    // decrypt message
    uint8_t *keystream = (uint8_t *)chacha_block;
    size_t copy_len = (max_len - 1 < 11) ? (max_len - 1) : 11;

    for (size_t i = 0; i < copy_len; i++) {
        out_message[i] = (char)(packet[5 + i] ^ keystream[32 + i]);
    }
    out_message[copy_len] = '\0';

    if (out_sender_id != NULL) {
        *out_sender_id = sender_id;
    }

    return 0;
}