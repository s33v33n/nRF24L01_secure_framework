#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../user_config/user_config.h"

int main(int argc, char *argv[]){

    if(sizeof(nrf_nodes) == 0){
        printf("Nodes were not added to the network\n");
        return -1;
    }

    int nodes_counter = sizeof(nrf_nodes) / sizeof(nrf_nodes[0]);

    for(int i=0; i < nodes_counter; i++){

        // check name
        char *uniqe_name = nrf_nodes[i].name;
        for(int j=i + 1; j < nodes_counter; j++){
            if(strcmp(uniqe_name, nrf_nodes[j].name) == 0){
                printf("The same name for nodes %d and %d\n", i, j);
                return -1;
            }
        }

        // check ID 
        uint8_t uniqe_id = nrf_nodes[i].id;
        if(uniqe_id != i){
            printf("Node %d ID exceeds allowed order. Allowed range <0, all devices - 1> in ascending order.\n", i);
            return -1;
        }

        // check rx pipe address
        uint64_t uniqe_addr = nrf_nodes[i].rx_pipe_addr;
        uint8_t msb = (uint8_t)((uniqe_addr >> 32) & 0xFF);
        if (msb == 0xAA || msb == 0x55 || msb == 0x00 || msb == 0xFF) {
            printf("Node: %d error - MSByte of rx_pipe_addr (0x%02X) cannot be 0xAA, 0x55, 0x00, or 0xFF (nRF24L01 specification)\n", i, msb);
            return -1;
        }

        for(int j=i + 1; j < nodes_counter; j++){
            if(uniqe_addr == nrf_nodes[j].rx_pipe_addr){
                printf("The same address for nodes %d and %d\n", i, j);
                return -1;
            }
        }

        // check pipe number 
        uint8_t pipe_number = nrf_nodes[i].rx_pipe_num;
        if(pipe_number != 0){
            printf("Node: %d error - only pipe 0 allowed. Current pipe number: %d\n",i, pipe_number);
            return -1;
        }

        // check channel 
        uint8_t channel = nrf_nodes[i].channel;
        if(channel < 1 || 124 < channel){
            printf("Node: %d error - enter channel as uint from range <1, 124>, current channel: %d\n",i, channel);
            return -1;
        }

        // check power 
        uint8_t power = nrf_nodes[i].power;
        if(3 < power){
            printf("Node: %d error - enter power as uint from range <0, 3>, current power: %d\n",i, power);
            return -1;
        }

        // check speed
        uint8_t speed = nrf_nodes[i].speed;
        if(speed != 1 && speed != 2){
            printf("Node: %d error - enter speed as uint from range <1, 2>, current speed: %d\n",i, speed);
            return -1;
        }
    }
 
    if (argc > 1) {
        char *endptr = NULL;
        long requested_id = strtol(argv[1], &endptr, 10);
        if (*endptr != '\0' || requested_id < 0 || requested_id >= nodes_counter) {
            fprintf(stderr, "Error: Selected NODE_ID=%ld is out of range! Configured nodes: <0, %d>\n",
                    requested_id, nodes_counter - 1);
            return -1;
        }
    }

    printf("User's data are correct (configured nodes: 0..%d)\n", nodes_counter - 1);
    return 0;
}