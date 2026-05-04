#include "../../kernel/apicore/kapi.h"

// ============================================================
// NETWIRE — Ethernet Network Driver for DOS64
// ============================================================
// Supports basic NIC initialization and packet transmission
// Registers as DEV_NETWORK device
inline void* operator new(unsigned long, void* p) { return p; }
// Intel 82540EM/82545EM emulated NIC (QEMU)
#define E1000_REG_CTRL      0x00000  // Device Control
#define E1000_REG_STATUS    0x00008  // Device Status
#define E1000_REG_EECD      0x00010  // EEPROM Control
#define E1000_REG_RCTRL     0x00100  // Receive Control
#define E1000_REG_RDBAL     0x02800  // RX Descriptor Base (Low)
#define E1000_REG_RDBAH     0x02804  // RX Descriptor Base (High)
#define E1000_REG_RDLEN     0x02808  // RX Descriptor Length
#define E1000_REG_RDH       0x02810  // RX Descriptor Head
#define E1000_REG_RDT       0x02818  // RX Descriptor Tail
#define E1000_REG_TCTRL     0x00400  // Transmit Control
#define E1000_REG_TDBAL     0x03800  // TX Descriptor Base (Low)
#define E1000_REG_TDBAH     0x03804  // TX Descriptor Base (High)
#define E1000_REG_TDLEN     0x03808  // TX Descriptor Length
#define E1000_REG_TDH       0x03810  // TX Descriptor Head
#define E1000_REG_TDT       0x03818  // TX Descriptor Tail
#define E1000_REG_RAL       0x05400  // Receive Address Low
#define E1000_REG_RAH       0x05404  // Receive Address High
#define E1000_REG_ICR       0x000C0  // Interrupt Cause

#define E1000_CTRL_FD       0x00000001  // Full Duplex
#define E1000_CTRL_LRST     0x00000008  // Link Reset
#define E1000_CTRL_ASDE     0x00000020  // Auto Speed Detection
#define E1000_CTRL_SLU      0x00000040  // Set Link Up
#define E1000_CTRL_RST      0x04000000  // Software Reset

#define ETHERNET_MAX_PACKET 1500
#define RX_RING_SIZE        8
#define TX_RING_SIZE        8

struct RxDescriptor {
    unsigned long long buffer_addr;
    unsigned short length;
    unsigned short checksum;
    unsigned char status;
    unsigned char errors;
    unsigned short special;
};
// stupid bullshit
struct TxDescriptor {
    unsigned long long buffer_addr;
    unsigned short length;
    unsigned char cso;
    unsigned char cmd;
    unsigned char status;
    unsigned char css;
    unsigned short special;
};

struct EthernetHeader {
    unsigned char dest_mac[6];
    unsigned char src_mac[6];
    unsigned short eth_type;
};

class EthernetNIC {
private:
    unsigned long long bar0;  // Base Address Register 0 (memory mapped)
    unsigned char mac_addr[6];
    RxDescriptor* rx_ring;
    TxDescriptor* tx_ring;
    unsigned char* rx_buffers[RX_RING_SIZE];
    unsigned char* tx_buffers[TX_RING_SIZE];
    int rx_index;
    int tx_index;
    KernelAPI* api;

    inline void write_reg(unsigned long offset, unsigned int value) {
        *(volatile unsigned int*)(bar0 + offset) = value;
    }

    inline unsigned int read_reg(unsigned long offset) {
        return *(volatile unsigned int*)(bar0 + offset);
    }

public:
    EthernetNIC() : bar0(0), rx_index(0), tx_index(0), api(nullptr) {
        for (int i = 0; i < 6; i++) mac_addr[i] = 0;
        for (int i = 0; i < RX_RING_SIZE; i++) rx_buffers[i] = nullptr;
        for (int i = 0; i < TX_RING_SIZE; i++) tx_buffers[i] = nullptr;
    }

    bool init(unsigned long long bar0_addr, KernelAPI* kernel_api) {
        api = kernel_api;
        bar0 = bar0_addr;
        
        if (!api) return false;
        
        api->println("[NETWIRE] Initializing Ethernet NIC...");

        // Reset the NIC
        unsigned int ctrl = read_reg(E1000_REG_CTRL);
        ctrl |= E1000_CTRL_RST;
        write_reg(E1000_REG_CTRL, ctrl);
        
        // Wait for reset to complete
        for (int i = 0; i < 1000000; i++) {
            if (!(read_reg(E1000_REG_CTRL) & E1000_CTRL_RST)) break;
        }

        api->println("[NETWIRE] NIC reset complete");
        
        // Read MAC address from NIC
        read_mac_address();
        
        // Setup RX ring
        if (!setup_rx_ring()) {
            api->println("[NETWIRE] RX ring setup failed");
            return false;
        }
        
        // Setup TX ring
        if (!setup_tx_ring()) {
            api->println("[NETWIRE] TX ring setup failed");
            return false;
        }

        // Configure device
        ctrl = read_reg(E1000_REG_CTRL);
        ctrl |= (E1000_CTRL_FD | E1000_CTRL_SLU | E1000_CTRL_ASDE);
        write_reg(E1000_REG_CTRL, ctrl);

        // Enable RX
        unsigned int rctrl = read_reg(E1000_REG_RCTRL);
        rctrl |= 0x00000002;  // RCTRL_EN
        write_reg(E1000_REG_RCTRL, rctrl);

        api->println("[NETWIRE] Ethernet NIC initialized successfully");
        return true;
    }

    void read_mac_address() {
        // Read MAC address (first 3 words from EEPROM via RAL/RAH)
        unsigned int ral = read_reg(E1000_REG_RAL);
        unsigned int rah = read_reg(E1000_REG_RAH);
        
        mac_addr[0] = (ral >> 0) & 0xFF;
        mac_addr[1] = (ral >> 8) & 0xFF;
        mac_addr[2] = (ral >> 16) & 0xFF;
        mac_addr[3] = (ral >> 24) & 0xFF;
        mac_addr[4] = (rah >> 0) & 0xFF;
        mac_addr[5] = (rah >> 8) & 0xFF;
    }

    bool setup_rx_ring() {
        // Allocate descriptor ring and buffers
        rx_ring = (RxDescriptor*)api->malloc(sizeof(RxDescriptor) * RX_RING_SIZE);
        if (!rx_ring) return false;

        for (int i = 0; i < RX_RING_SIZE; i++) {
            rx_buffers[i] = (unsigned char*)api->malloc(ETHERNET_MAX_PACKET + 16);
            if (!rx_buffers[i]) return false;

            rx_ring[i].buffer_addr = (unsigned long long)rx_buffers[i];
            rx_ring[i].length = 0;
            rx_ring[i].status = 0;
        }

        // Configure RX ring in NIC
        unsigned long long ring_addr = (unsigned long long)rx_ring;
        write_reg(E1000_REG_RDBAL, (unsigned int)(ring_addr & 0xFFFFFFFF));
        write_reg(E1000_REG_RDBAH, (unsigned int)((ring_addr >> 32) & 0xFFFFFFFF));
        write_reg(E1000_REG_RDLEN, RX_RING_SIZE * sizeof(RxDescriptor));
        write_reg(E1000_REG_RDH, 0);
        write_reg(E1000_REG_RDT, RX_RING_SIZE - 1);

        return true;
    }

    bool setup_tx_ring() {
        // Allocate descriptor ring and buffers
        tx_ring = (TxDescriptor*)api->malloc(sizeof(TxDescriptor) * TX_RING_SIZE);
        if (!tx_ring) return false;

        for (int i = 0; i < TX_RING_SIZE; i++) {
            tx_buffers[i] = (unsigned char*)api->malloc(ETHERNET_MAX_PACKET + 16);
            if (!tx_buffers[i]) return false;

            tx_ring[i].buffer_addr = (unsigned long long)tx_buffers[i];
            tx_ring[i].length = 0;
            tx_ring[i].status = 0;
        }

        // Configure TX ring in NIC
        unsigned long long ring_addr = (unsigned long long)tx_ring;
        write_reg(E1000_REG_TDBAL, (unsigned int)(ring_addr & 0xFFFFFFFF));
        write_reg(E1000_REG_TDBAH, (unsigned int)((ring_addr >> 32) & 0xFFFFFFFF));
        write_reg(E1000_REG_TDLEN, TX_RING_SIZE * sizeof(TxDescriptor));
        write_reg(E1000_REG_TDH, 0);
        write_reg(E1000_REG_TDT, 0);

        return true;
    }

    bool send_packet(const unsigned char* data, unsigned short length) {
        if (length > ETHERNET_MAX_PACKET) return false;

        // Get next available TX descriptor
        unsigned int tdt = read_reg(E1000_REG_TDT);
        unsigned int next_tdt = (tdt + 1) % TX_RING_SIZE;

        // Copy data to TX buffer
        for (unsigned short i = 0; i < length; i++) {
            tx_buffers[tdt][i] = data[i];
        }

        // Setup descriptor
        tx_ring[tdt].buffer_addr = (unsigned long long)tx_buffers[tdt];
        tx_ring[tdt].length = length;
        tx_ring[tdt].cmd = 0x0B;  // EOP | IFCS | RS
        tx_ring[tdt].status = 0;

        // Notify NIC
        write_reg(E1000_REG_TDT, next_tdt);

        return true;
    }

    unsigned short receive_packet(unsigned char* buffer, unsigned short max_len) {
        unsigned int rdt = read_reg(E1000_REG_RDT);
        unsigned int next_rdt = (rdt + 1) % RX_RING_SIZE;

        if (rx_ring[next_rdt].status == 0) return 0;  // No packet available

        unsigned short packet_len = rx_ring[next_rdt].length;
        if (packet_len > max_len) packet_len = max_len;

        // Copy packet data
        for (unsigned short i = 0; i < packet_len; i++) {
            buffer[i] = rx_buffers[next_rdt][i];
        }

        // Reset descriptor
        rx_ring[next_rdt].status = 0;
        write_reg(E1000_REG_RDT, next_rdt);

        return packet_len;
    }

    void print_mac_address() {
        if (!api) return;
        
        char buf[32];
        api->println("MAC Address: ");
        for (int i = 0; i < 6; i++) {
            if (i > 0) api->print(":");
            unsigned char byte = mac_addr[i];
            buf[0] = "0123456789ABCDEF"[(byte >> 4) & 0xF];
            buf[1] = "0123456789ABCDEF"[byte & 0xF];
            buf[2] = '\0';
            api->print(buf);
        }
        api->println("");
    }
};



// Entry point for the driver
static unsigned char nic_buf[sizeof(EthernetNIC)];
static EthernetNIC* nic = nullptr;

extern "C" int driver_entry(KernelAPI* api) {
    if (!api)          return -10;
    if (!api->print)   return -11;
    if (!api->malloc)  return -12;

    api->println("[NETWIRE] Loading Ethernet driver...");

    nic = new (nic_buf) EthernetNIC;

    unsigned long long bar0 = 0xFEBC0000;  // QEMU E1000 BAR0

    if (!nic->init(bar0, api)) {
        api->println("[NETWIRE] NIC init failed.");
        return -3;
    }

    nic->print_mac_address();
    api->register_device("NETWIRE0", nic, DEV_NETWORK);
    api->println(" NETWIRE I/O Internet | Rational Systems. ");
    return 0;
}
