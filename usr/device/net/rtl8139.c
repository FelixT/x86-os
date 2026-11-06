#include "../../prog.h"
#include "../../lib/stdio.h"
#include "../../lib/stdlib.h"
#include "../../../lib/string.h"

#include "netdev.h"

// rtl8139 usermode driver
// implements a netdev_t interface (netdev.h)

static volatile uint8_t *io; // pci mapped io buffer

static uint8_t mac_addr[6];

#define ETHERNET_MAX 1518 // ethernet frame max size

// 8K buffer
#define RX_BUF_LEN 8192
#define RX_SIZE (RX_BUF_LEN + 16 + 1536) // includes extra padding

static uint8_t *rx_dma; // read/rx dma buffer
static uint16_t rx_offset = 0;

static netdev_t device;

#define RTL_CONFIG_1 0x52
#define RTL_CMD 0x37
#define RTL_RBSTART 0x30
#define RTL_IMR 0x3C
#define RTL_RCR 0x44
#define RTL_CAPR 0x38 // read ptr
#define RTL_CBR 0x3A // write ptr
#define RTL_TSAD0 0x20 // +4*i : buffer phys address
#define RTL_TSD0  0x10 // +4*i : status/length (write to fire)

// 4x2K write/tx buffers

#define TX_BUF_SIZE 2048 

static uint8_t *tx_dma[4];

static int tx_buf_i = 0;

#define TX_WAIT_MAX 1000

int rtl_send(netdev_t *dev, uint8_t *data, uint16_t len) {
   (void)dev;
   if(len > TX_BUF_SIZE) return -1; // tx buffer overflow

   int i = tx_buf_i;
   tx_buf_i = (tx_buf_i + 1) & 3;

   memcpy((void*)tx_dma[i], data, len);
   if(len < 60) {
      memset((void*)tx_dma[i] + len, 0, 60 - len); // zero-pad, don't leak stale bytes
      len = 60; // minimum ethernet frame
   }

   // start DMA
   *(volatile uint32_t*)(io + RTL_TSD0 + i*4) = len;

   // wait card to read/drain buffer (bit 13 = OWN, set when dma completes)
   volatile int c = 0;
   while(!(*(volatile uint32_t*)(io + RTL_TSD0 + i*4) & (1<<13))) {
      if(++c >= TX_WAIT_MAX)
         return -1; // give up
   }

   return len;
}

void rtl_poll(netdev_t *dev) {
   // poll for packets waiting (bit 0 => 0)
   while(!(io[RTL_CMD] & 0x01)) {
      uint16_t status = *(volatile uint16_t*)(rx_dma + rx_offset);
      uint16_t len = *(volatile uint16_t*)(rx_dma + rx_offset + 2);
      if(len < 4 || len > ETHERNET_MAX) { // received bad length
         // skip bad headers, set read position rx_offset (fix desync by reading from device counter)
         uint16_t cbr = *(volatile uint16_t*)(io + RTL_CBR);
         rx_offset = cbr % RX_BUF_LEN;
         *(volatile uint16_t*)(io + RTL_CAPR) = rx_offset - 0x10;
         break;
      }
      uint8_t *frame = (uint8_t*)(rx_dma + rx_offset + 4);
      uint16_t frame_len = len - 4; // strip crc at end of frame (already validated by device)

      if((status & 0x01) && dev->rx) // rok - receive ok
         dev->rx(dev, frame, frame_len);

      // advance: +4 header, 32bit-aligned, wrap at 8K boundary
      rx_offset = (rx_offset + len + 4 + 3) & ~3; // note: can underflow
      rx_offset %= RX_BUF_LEN;
      // tell card read position
      *(volatile uint16_t*)(io + RTL_CAPR) = rx_offset - 0x10;
   }
}

void rtl_free(netdev_t *dev) {
   io[RTL_CMD] = 0x10; // reset device
   io = NULL;
   hrelease(dev->device); // stop bus mastering before freeing buffers
   unmap((uint32_t)rx_dma);
   for(int i = 0; i < 4; i++)
      unmap((uint32_t)tx_dma[i]);
}

// rtl8139 usermode driver
netdev_t *rtl_init() {
   handle_t h;
   io = pci_map(0x10EC, 0x8139, &h);
   if(io == NULL) {
      printf("PCI map failed\n");
      exit(1);
   }
   
   // power on, soft reset
   io[RTL_CONFIG_1] = 0x00; // config1: out of low-power
   io[RTL_CMD] = 0x10; // cr: rst
   while(io[RTL_CMD] & 0x10);

   // get continuous physical memory for rx dma buffer
   uint32_t rx_dma_phys;
   handle_t rx_dma_h = dma(RX_SIZE, &rx_dma_phys);
   if(rx_dma_h < 0) {
      hrelease(h);
      return NULL;
   }

   rx_dma = map(rx_dma_h);
   hrelease(rx_dma_h);
   if(!rx_dma) {
      hrelease(h);
      return NULL;
   }

   // allocate tx buffers
   handle_t tx_dma_h[4];
   uint32_t tx_dma_phys[4];
   for(int i = 0; i < 4; i++) {
      tx_dma_h[i] = dma(TX_BUF_SIZE, &tx_dma_phys[i]);
      if(tx_dma_h[i] < 0) {
         for(int j = 0; j < i; j++)
            unmap((uint32_t)tx_dma[j]);
         unmap((uint32_t)rx_dma);
         hrelease(h);
         return NULL;
      }
      tx_dma[i] = map(tx_dma_h[i]);
      hrelease(tx_dma_h[i]);
      if(!tx_dma[i]) {
         unmap((uint32_t)rx_dma);
         for(int j = 0; j < i; j++)
            unmap((uint32_t)tx_dma[j]);
         hrelease(h);
         return NULL;
      }
   }

   // read MAC
   for(int i = 0; i < 6; i++) mac_addr[i] = io[i];
   memcpy(device.mac, mac_addr, 6);
   printf("MAC %h:%h:%h:%h:%h:%h\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

   *(volatile uint32_t*)(io+RTL_RBSTART) = (uint32_t)rx_dma_phys;

   // set tx buffer physical addresses
   for(int i = 0; i < 4; i++)
      *(volatile uint32_t*)(io + RTL_TSAD0 + i*4) = (uint32_t)tx_dma_phys[i];

   *(volatile uint16_t*)(io+RTL_IMR) = 0; // disable interrupts (use polling)

   *(volatile uint32_t*)(io+RTL_RCR) = 0xf | (1 << 7); // set 'receive' config AB+AM+APM+AAP + WRAP (ensure rx mem is continuous - requires padding)

   io[RTL_CMD] = 0x0C; // re + te: enable receiver & transmitter

   device.state = NULL;
   device.send = &rtl_send;
   device.poll = &rtl_poll;
   device.free = &rtl_free;
   device.device = h;
   return &device;
}