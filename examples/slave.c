#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "../framework/nrf_framework.h"

int main(void) {
    printf("=== Slave Application ===\n");

    if (nrf_init() < 0) {
        fprintf(stderr, "Slave initialization failed!\n");
        return 1;
    }
    printf("Slave initialized successfully.\n");
    printf("Waiting for command from Master...\n\n");

    // Wait for incoming message from Master
    uint8_t sender_id = 0;
    char message[32] = {0};
    if (nrf_receive(&sender_id, message, sizeof(message)) < 0) {
        fprintf(stderr, "Failed to receive message!\n");
        nrf_close();
        return 1;
    }

    printf("Received message from Master (Node ID %u): '%s'\n", sender_id, message);

    // Check condition
    if (strcmp(message, "diag") == 0) {
        uint8_t channel = 0;
        if (nrf_get_channel(&channel) < 0) {
            fprintf(stderr, "Failed to read RF channel!\n");
        } else {
            printf("Diagnostic command matched! Current RF channel: %u\n", channel);

            // Format response containing channel value (max 11 bytes!)
            char reply[12] = {0};
            snprintf(reply, sizeof(reply), "CH: %u", channel);

            // Short delay to ensure Master has returned to RX mode and is ready to receive (short distance between nodes)
            usleep(50000); // 50 ms

            // Send channel value back to Master
            printf("Sending channel value ('%s') to Master...\n", reply);
            if (nrf_node_send(reply) < 0) {
                fprintf(stderr, "Failed to send response to Master!\n");
            } else {
                printf("Response sent successfully.\n\n");
            }
        }
    } else {
        printf("Received unknown command '%s'. No diagnostic performed.\n\n", message);
    }

    nrf_close();
    printf("Slave closed successfully. Program finished.\n");

    return 0;
}
