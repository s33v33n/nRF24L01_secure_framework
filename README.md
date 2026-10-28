# Secure nRF24L01+ Framework

Secure RF communication framework for **nRF24L01+** transceivers on Linux platforms (e.g. Raspberry Pi). The project integrates a custom Linux SPI kernel driver with authenticated encryption (**ChaCha20-Poly1305**), providing a secure star-topology wireless network for up to 256 devices.

---

## Key Features

* **Star Network Topology:**
  * Single **Master** (`NODE_ID = 0`).
  * Multiple **Slaves** (`NODE_ID = 1..N`).
  * Communication is strictly point-to-point between Master and Slaves (Slaves do not communicate directly with each other).
* **Maximum User Payload:** **11 bytes** per 32-byte radio packet.
* **Authenticated Encryption (AEAD):** Based on **ChaCha20-Poly1305**.
* **Dedicated Linux Kernel Driver:** Custom SPI driver with Device Tree Overlay support, non-blocking `poll()` mechanism, and full `ioctl` configuration interface.

---

## Radio Packet Structure (32 bytes)

The fixed 32-byte hardware payload of the nRF24L01+ is structured as follows:

```text
| Replay Counter (4 B) | Sender ID (1 B) | Encrypted Payload (11 B) | Poly1305 Tag (16 B) |
```
*(Byte offsets: `0..3` Replay Counter, `4` Sender ID, `5..15` Encrypted Payload, `16..31` Poly1305 Tag)*. The fixed hardware frame is 32 bytes, accommodating a maximum plaintext payload of 11 bytes.


---

## Repository Structure

```text
├── driver/           # Linux kernel driver (nrf24l01.ko) and Device Tree source (.dts/.dtbo)
├── encryption/       # ChaCha20-Poly1305 cryptographic implementation
├── framework/        # High-level library API (nrf_framework.c, nrf_packet.c)
├── include/          # Internal header files (IOCTL definitions)
├── user_config/      # User configurations (hardware_config.h, user_config.h)
├── examples/         # Application examples (master.c, slave.c)
├── tools/            # Utilities (auto_config.sh generator, data_validator.c)
├── Makefile          # Main build, permanent install, and packaging rules
└── build.sh          # Interactive build script (validation, compilation, loading)
```

---

## Hardware Description (`hardware_config.h`)

The provided `user_config/hardware_config.h` and `driver/nrf24l01.dts.template` serve as a reference example for **two nRF24L01+ modules connected simultaneously** to a Raspberry Pi (using SPI0 and SPI1).

Hardware wiring depends on your target board:
* **Using the provided example:** If your hardware wiring matches the pins defined in `hardware_config.h`, you can use the template as-is.
* **Single SPI module (e.g. only SPI0):** If you only use one module on SPI0:
  * In `user_config/hardware_config.h`: Set `#define RADIO_SPI1_ENABLE 0` and adjust `#define NRF_NDEVICES 1`.
  * In `driver/nrf24l01.dts.template`: Comment out or remove all SPI1-related sections (comments are marked above the appropriate sections).
* **Custom pinout / different board:** Adjust the pin numbers (CE, IRQ, CS, SPI) in `hardware_config.h` and adapt `driver/nrf24l01.dts.template` to match your physical connections.

---

## Network Configuration Rules (`user_config.h`)

The user must configure the network (up to 256 nodes) in `user_config/user_config.h` (or choose automatic configuration generation during `./build.sh`). 

> **Note:** Choosing the default automatic configuration generator (`tools/auto_config.sh`) automatically creates the 256-bit network key, a unique Master address, and completely populates all fields of `struct nrf_node_config` in the `nrf_nodes[]` array for all configured nodes.

When providing manual configuration data, keep the following requirements in mind:

### 1. Security & Master Address
* **Generate a Unique 256-bit Network Key (`NRF_NETWORK_KEY`):** You must generate a cryptographically secure 256-bit key (array of 8 `uint32_t` values). This shared secret must be identical across all nodes for ChaCha20-Poly1305 AEAD encryption.
* **Master Address (`NRF_MASTER_ADDRESS`):** Define a unique 40-bit (5-byte) radio address for the Master node (e.g. `0x1122334455ULL`).

### 2. Node Array (`nrf_nodes[]`) and `struct nrf_node_config`
When filling the `nrf_nodes[]` array, pay close attention to each field in `struct nrf_node_config`:

```c
struct nrf_node_config {
    char *name;             // Unique node name (e.g. DEV_NAME_SPI0, "sensor_1")
    uint64_t tx_address;    // Default target address (or 0x00 / NRF_MASTER_ADDRESS)
    uint64_t rx_pipe_addr;  // Unique 40-bit listening address (must not collide)
    uint8_t rx_pipe_num;    // RX pipe number (ONLY pipe 0 is allowed)
    uint8_t id;             // Unique node ID (Master = 0, Slaves = 1, 2, ... N-1)
    uint8_t channel;        // RF channel: 1 to 124
    uint8_t power;          // TX power: 0 (-18 dBm), 1 (-12 dBm), 2 (-6 dBm), 3 (0 dBm)
    uint8_t speed;          // RF speed: 1 (1 Mbps) or 2 (2 Mbps)
};
```

* **Node ID Order:**
  * **Master is strictly ID 0** (the first entry in the array).
  * **Slaves must have contiguous ascending IDs:** `1, 2, 3, ..., N-1` (no gaps allowed; array index must match `id`).
* **Uniqueness:**
  * Each node must have a **unique name** (e.g. `"nrf_garden"`, `"nrf_garage"`).
  * Each node must have a **unique RX address** (`rx_pipe_addr`).
* **RF Constraints:**
  * **Pipe number:** Only pipe `0` is allowed (`rx_pipe_num = 0`).
  * **Channel:** Must be in range `1` to `124`.
  * **Power:** Must be in range `0` to `3` (0 = -18 dBm, 1 = -12 dBm, 2 = -6 dBm, 3 = 0 dBm).
  * **Speed:** Must be `1` (1 Mbps) or `2` (2 Mbps).

*(All entries are automatically verified before building by `tools/data_validator.c`)*.


---

## Building and Installation

The script guides you through the process:
1. **Optional Default Configuration Generation (`tools/auto_config.sh`):** The user can either fill `user_config.h` manually according to the rules above, or let the script auto-generate a valid setup.
2. **Automated Validation:** Validates network settings.
3. **Device Tree Compilation:** Generates and compiles the overlay based on **your** hardware SPI pin definitions. Hardware connections depend on your device, and `driver/nrf24l01.dts.template` must be created/adjusted according to wiring in `hardware_config.h`.
4. **Target Node Selection:** Prompts for `NODE_ID`:
   * `0` – compiles the **master** binary.
   * `1..N` – compiles the **slave** binary for the specified node ID.
5. **Installation and Immediate Activation:**
   * Permanent setup across reboots.
   * Dynamic loading of the Device Tree overlay and kernel module in the running system (no reboot needed).

You can pass the target `NODE_ID` to build binaries, default is 0:
```bash
./build.sh 0   # Build and configure as Master
./build.sh 1   # Build and configure as Slave (Node ID 1)
```
...and run your program (with root privileges to access `/dev/*`):
```bash
sudo ./slave   # Terminal 1: Run Slave first (waits for command)
sudo ./master  # Terminal 2: Run Master (prompts for input)
```

### Examples (`examples/`):
* **Master (`master.c`):** Initializes the radio, prompts the user for a command (e.g. `'diag'`), sends it to Slave (Node ID 1), waits for the reply, prints the received channel value, and closes.
* **Slave (`slave.c`):** Initializes the radio and waits for incoming message. Upon receiving `'diag'`, it checks its RF channel via `nrf_get_channel()`, prints it to the terminal, sends the channel back to Master (`"CH: <channel>"`), and closes.

---

## Framework API (`nrf_framework.h`)

User-space applications interact with the radio network via high-level API:

```c
#include "framework/nrf_framework.h"

// 1. Initialization and Cleanup
int nrf_init(void);    // Opens /dev/nrf24l01_* and configures radio according to node configuration
int nrf_close(void);   // Closes device descriptor cleanly

// 2. Transmission (handles AEAD encryption and tagging)
int nrf_master_send(uint8_t target_node_id, char *message); // Master sends to a specific Slave
int nrf_node_send(char *message);                           // Slave sends back to Master

// 3. Reception (handles AEAD decryption and authentication tag check)
int nrf_receive(uint8_t *sender_id, char *out_message, size_t max_len);

// 4. Runtime Radio Configuration
int nrf_set_channel(uint8_t channel);
int nrf_set_power(uint8_t power);
int nrf_set_speed(uint8_t speed);

// 5. Diagnostics
int nrf_get_channel(uint8_t *channel);
int nrf_get_status(uint8_t *status);
int nrf_get_rf_setup(uint8_t *setup);
```

During software development, the user also has access to the node list (`nrf_nodes` in `user_config/user_config.h`) to view node settings.