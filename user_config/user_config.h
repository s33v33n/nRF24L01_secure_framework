#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include <stdint.h>
#include "hardware_config.h"

#define NRF_MASTER_ADDRESS 0x03E883573AULL

static const uint32_t NRF_NETWORK_KEY[8] = {
    0x7a65caab, 0xc30ad215, 0x0f20fb9a, 0x5e1aac19,
    0x2a0356d0, 0xc5eab59b, 0x7050829c, 0xd37e962e
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
    {DEV_NAME_SPI0, 0x00, NRF_MASTER_ADDRESS, 0, 0, 15, 0, 1},
    {DEV_NAME_SPI1, 0x00, 0xCEA3DD3F97ULL, 0, 1, 15, 0, 1}
};

#endif // USER_CONFIG_H
