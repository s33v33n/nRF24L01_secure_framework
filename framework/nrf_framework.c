#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../include/nrf_ioctl.h"
#include "../user_config/user_config.h"
#include "../encryption/Chacha20-Poly1305.h"
#include "nrf_packet.h"

#ifndef NODE_ID
    #define NODE_ID 0      
#endif
#define MY_NODE_ID NODE_ID  

#ifndef MASTER_ID
    #define MASTER_ID 0    
#endif
#define MY_MASTER_ID MASTER_ID

#define ALL_DEVICES (sizeof(nrf_nodes) / sizeof((nrf_nodes)[0]))

static struct nrf_node_config master_config = {0};
static struct nrf_node_config node_config = {0};
int nrf_fd = -1;

// functions for nrf manage:

int nrf_init(){

    // find master config
    int ret = 1;
    for(size_t i=0; i < (sizeof(nrf_nodes)/sizeof(nrf_nodes[0])); i++){
        if(nrf_nodes[i].id == MY_MASTER_ID){
            master_config = nrf_nodes[i];
            ret = 0;
        }
    }

    if(ret){
        fprintf(stderr, "ERROR: Master ID=%d does not exist in network_config!\n", MY_MASTER_ID);
        return -1;
    }

    //find node number for init
    ret = 1;
    for(size_t i=0; i < ALL_DEVICES; i++){
        if(nrf_nodes[i].id == MY_NODE_ID){
            node_config = nrf_nodes[i];
            ret = 0;
        }
    }
    if(ret){
        fprintf(stderr, "ERROR: Node ID=%d does not exist in network_config!\n", MY_NODE_ID);
        return -1;
    }

    //------ configure node (master or slave) ------

    char *dev_file = NULL;
    if (asprintf(&dev_file, "/dev/%s", node_config.name) == -1) {
        fprintf(stderr, "Not enough memory for device allocation!\n");
        return -1;
    }

    // device open
    nrf_fd = open(dev_file, O_RDWR);
    if (nrf_fd < 0) {
        perror("ERROR: Failed to open device");
        fprintf(stderr, "Could not open '%s' (try with sudo)\n", dev_file);
        free(dev_file);
        return -1;
    }

    free(dev_file);

    /* IOCTL module configuration */

    struct nrf_pipe_config rx_pipe_config ={
        .pipe_num = node_config.rx_pipe_num,
        .address = node_config.rx_pipe_addr
    };

    // TX default address (e.g. Master)
    if(ioctl(nrf_fd, NRF_IOCTL_SET_TX_ADDR, &master_config.rx_pipe_addr) < 0){
        perror("IOCTL: cannot write tx address");
        return -1;
    }

    // RX listening address for this node
    // Must be set AFTER SET_TX_ADDR because SET_TX_ADDR sets pipe 0 for Auto-ACK.
    // Setting RX_ADDR here guarantees that this node listens on its own rx_pipe_addr!
    if(ioctl(nrf_fd, NRF_IOCTL_SET_RX_ADDR, &rx_pipe_config) < 0){
        perror("IOCTL: cannot write rx pipe config");
        return -1;
    }

    // RF channel
    if (ioctl(nrf_fd, NRF_IOCTL_SET_CHANNEL, &node_config.channel) < 0) {
        perror("IOCTL: cannot set rf channel");
        return -1;
    }
    
    // RF power
    if (ioctl(nrf_fd, NRF_IOCTL_SET_POWER, &node_config.power) < 0) {
        perror("IOCTL: cannot set power level");
        return -1;
    }
    
    // RF speed
    if (ioctl(nrf_fd, NRF_IOCTL_SET_SPEED, &node_config.speed) < 0) {
        perror("IOCTL: cannot set speed level");
        return -1;
    }

    printf("Module %s init successfully\n",node_config.name);
    
    return 0;
}

int nrf_master_send(uint8_t target_node_id, char *message){

    if(nrf_fd < 0 || message == NULL){
        return -1;
    }

    // find node ID
    uint64_t target_addr = 0;
    uint8_t target_channel = 0;
    uint8_t target_speed = 0;
    bool found = false;
    for(size_t i=0; i < ALL_DEVICES; i++){
        if(nrf_nodes[i].id == target_node_id){
            target_addr = nrf_nodes[i].rx_pipe_addr;
            target_channel = nrf_nodes[i].channel;
            target_speed = nrf_nodes[i].speed;
            found = true;
            break;
        }
    }

    if(!found) {
        fprintf(stderr, "ERROR: Target node ID=%d not found in config!\n", target_node_id);
        return -1;
    }

    if(ioctl(nrf_fd, NRF_IOCTL_SET_TX_ADDR, &target_addr) < 0) {
        perror("IOCTL: Failed to set TX address");
        return -1;
    }

    if(ioctl(nrf_fd, NRF_IOCTL_SET_CHANNEL, &target_channel) < 0) {
        perror("IOCTL: Failed to set TX RF channel");
        return -1;
    }

    if(ioctl(nrf_fd, NRF_IOCTL_SET_SPEED, &target_speed) < 0) {
        perror("IOCTL: Failed to set TX speed");
        return -1;
    }

    // send package
    int send_ret = send_packet(nrf_fd, master_config.id, message);

    // return to master config (restore Master listening RX pipe 0)
    struct nrf_pipe_config master_rx_pipe = {
        .pipe_num = master_config.rx_pipe_num,
        .address = master_config.rx_pipe_addr
    };
    if (ioctl(nrf_fd, NRF_IOCTL_SET_RX_ADDR, &master_rx_pipe) < 0) {
        perror("IOCTL: Failed to restore Master RX pipe");
    }

    if(ioctl(nrf_fd, NRF_IOCTL_SET_CHANNEL, &master_config.channel) < 0) {
        perror("IOCTL: Failed to return to TX RF master channel");
        return -1;
    }

    if(ioctl(nrf_fd, NRF_IOCTL_SET_SPEED, &master_config.speed) < 0) {
        perror("IOCTL: Failed to return to TX master speed");
        return -1;
    }

    //verify if package was sent
    if (send_ret < 0) {
        return -1;
    }

    return 0;
}

int nrf_node_send(char *message) {

    if (nrf_fd < 0 || message == NULL) {
        return -1;
    }

    // Set TX address to Master (and pipe 0 to master address for receiving ACK)
    if (ioctl(nrf_fd, NRF_IOCTL_SET_TX_ADDR, &master_config.rx_pipe_addr) < 0) {
        perror("IOCTL: Failed to set TX address to Master");
        return -1;
    }

    // rf master config
    if (ioctl(nrf_fd, NRF_IOCTL_SET_CHANNEL, &master_config.channel) < 0) {
        perror("IOCTL: Failed to set master RF channel");
        return -1;
    }

    if (ioctl(nrf_fd, NRF_IOCTL_SET_SPEED, &master_config.speed) < 0) {
        perror("IOCTL: Failed to set master speed");
        return -1;
    }

    // send package
    int send_ret = send_packet(nrf_fd, node_config.id, message);

    // Restore node's own listening address on RX pipe 0
    struct nrf_pipe_config node_rx_pipe = {
        .pipe_num = node_config.rx_pipe_num,
        .address = node_config.rx_pipe_addr
    };
    if (ioctl(nrf_fd, NRF_IOCTL_SET_RX_ADDR, &node_rx_pipe) < 0) {
        perror("IOCTL: Failed to return to node RX pipe");
    }

    // return to node rf config
    if (ioctl(nrf_fd, NRF_IOCTL_SET_CHANNEL, &node_config.channel) < 0) {
        perror("IOCTL: Failed to return to node RF channel");
        return -1;
    }

    if (ioctl(nrf_fd, NRF_IOCTL_SET_SPEED, &node_config.speed) < 0) {
        perror("IOCTL: Failed to return to node speed");
        return -1;
    }

    if (send_ret < 0) {
        return -1;
    }

    return 0;
}

int nrf_receive(uint8_t *sender_id, char *out_message, size_t max_len) {

    if (nrf_fd < 0 || out_message == NULL) {
        return -1;
    }

    if(receive_packet(nrf_fd, sender_id, out_message, max_len) < 0){
        return -1;
    }

    return 0;
}

// ... and setters + getters

// Setters

int nrf_set_channel(uint8_t channel) {
    if (nrf_fd < 0 || channel < 1 || channel > 124){
        return -1;
    } 

    return ioctl(nrf_fd, NRF_IOCTL_SET_CHANNEL, &channel);
}

int nrf_set_power(uint8_t power) {
    if (nrf_fd < 0 || power > 3){
        return -1;
    } 

    return ioctl(nrf_fd, NRF_IOCTL_SET_POWER, &power);
}

int nrf_set_speed(uint8_t speed) {
    if (nrf_fd < 0 || (speed != 1 && speed != 2)){
        return -1;
    } 
    return ioctl(nrf_fd, NRF_IOCTL_SET_SPEED, &speed);
}

// Getters

int nrf_get_status(uint8_t *status) {
    if (nrf_fd < 0 || status == NULL){
        return -1;
    }

    return ioctl(nrf_fd, NRF_IOCTL_GET_STATUS, status);
}

int nrf_get_channel(uint8_t *channel) {
    if (nrf_fd < 0 || channel == NULL){
        return -1;
    } 

    return ioctl(nrf_fd, NRF_IOCTL_GET_RF_CHANNEL, channel);
}

int nrf_get_rf_setup(uint8_t *setup) {
    if (nrf_fd < 0 || setup == NULL){
        return -1;
    } 

    return ioctl(nrf_fd, NRF_IOCTL_GET_RF_SETUP, setup);
}

int nrf_close() {
    if (nrf_fd >= 0) {
        close(nrf_fd); 
        nrf_fd = -1;
    }

    return 0;
}