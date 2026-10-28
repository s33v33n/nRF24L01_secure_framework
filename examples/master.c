#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../framework/nrf_framework.h"

int main(void) {
    printf("=== Master Application ===\n");

    if (nrf_init() < 0) {
        fprintf(stderr, "Master initialization failed!\n");
        return 1;
    }
    printf("Master initialized successfully.\n\n");

    // Wait for user input 
    char input[32] = {0};
    printf("Enter command to send to Slave (diag): ");
    if (fgets(input, sizeof(input), stdin) == NULL) {
        nrf_close();
        return 1;
    }
    input[strcspn(input, "\r\n")] = '\0';

    // Send message to Slave (Node ID = 1)
    uint8_t target_slave = 1;
    printf("\nSending '%s' to Slave (ID %u)...\n", input, target_slave);
    if (nrf_master_send(target_slave, input) < 0) {
        fprintf(stderr, "Failed to send message to Slave!\n");
        nrf_close();
        return 1;
    }
    printf("Message sent. Waiting for response from Slave...\n");

    // Wait for response from Slave
    uint8_t sender_id = 0;
    char response[32] = {0};
    if (nrf_receive(&sender_id, response, sizeof(response)) < 0) {
        fprintf(stderr, "Failed to receive response from Slave!\n");
        nrf_close();
        return 1;
    }

    printf("Received response from Slave (ID %u): '%s'\n\n", sender_id, response);

    nrf_close();
    printf("Master closed successfully. Program finished.\n");

    return 0;
}
