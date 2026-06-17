#include "../../prog.h"
#include "../../lib/stdio.h"
#include "../../../lib/string.h"

#include "netdev.h"

// ethernet + arp layer (part of IP)

// assumes SLIRP setup - todo: DHCP i.e. discovery
#define GATEWAY_IP 10, 0, 2, 2
#define LOCAL_IP 10, 0, 2, 15

extern netdev_t *rtl_init(); // linked against rtl driver

// support multiple (statically linked) drivers
// eventually: separate processes with ipc

typedef struct eth_driver_t {
   uint16_t vendor, device;
   netdev_t *(*init)(void);
} eth_driver_t;

static eth_driver_t drivers[] = {
   { 0x10EC, 0x8139, rtl_init }
};

// host byte order to network byte order (x86 is little endian, network is big endian)
static inline uint16_t htons(uint16_t x) {
   return (x >> 8) | (x << 8);
}

// pack ipv4 into uint32_t in network order to be copied into packet (each octet = 1byte)
static inline uint32_t pack_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
   return (uint32_t)((a) | ((b)<<8) | ((c)<<16) | ((d)<<24));
}

typedef struct {
   uint8_t dest_addr[6]; // destination hardware address
   uint8_t src_addr[6]; // source hardware address
   uint16_t frame_type; // ethernet frame type
} __attribute__((packed)) eth_header_t;

typedef struct {
   uint16_t htype; // hardware type (1 = ethernet)
   uint16_t ptype; // format of protocol address
   uint8_t hlen; // length of hardware address
   uint8_t plen; // length of protocol address
   uint16_t op; // ARP opcode (command)
   uint8_t sha[6]; // sender hardware address - 6 octets
   uint32_t spa; // sender IP address
   uint8_t tha[6]; // target hardware address - 6 octets
   uint32_t tpa; // target IP address
} __attribute__((packed)) eth_arp_t;

// broadcast an ARP request to discover target_ip MAC address
void arp_request(netdev_t *dev, uint32_t target_ip) {
   uint8_t packet[sizeof(eth_header_t) + sizeof(eth_arp_t)];
   eth_header_t *eth = (eth_header_t*)packet;
   eth_arp_t *arp = (eth_arp_t*)(packet + sizeof(eth_header_t));

   // ethernet header
   memset(eth->dest_addr, 0xFF, 6); // broadcast
   memcpy(eth->src_addr, dev->mac, 6);
   eth->frame_type = htons(0x0806); // ARP

   // arp payload
   arp->htype = htons(1); // ethernet
   arp->ptype = htons(0x0800); // IPv4
   arp->hlen = 6;
   arp->plen = 4;
   arp->op = htons(1); // request
   memcpy(arp->sha, dev->mac, 6);
   arp->spa = pack_ip(LOCAL_IP);
   memset(arp->tha, 0, 6); // zero'd for receive
   arp->tpa = target_ip; // already in packet format

   if(dev->send(dev, packet, sizeof(packet)) < 0) { // send() returns -1 on failure
      debug_println("ARP request failed to send");
   }
}

void eth_input(netdev_t *dev, uint8_t *frame, uint16_t len) {
   (void)dev;
   if(len < sizeof(eth_header_t)) return; // runt frame - too short for an ethernet header
   eth_header_t *eth = (eth_header_t*)frame;
   uint16_t ethertype = htons(eth->frame_type); // inverse swap

   printf("RX len=%u type=0x%h from %h:%h:%h:%h:%h:%h\n", len, ethertype,
      eth->src_addr[0], eth->src_addr[1], eth->src_addr[2],
      eth->src_addr[3], eth->src_addr[4], eth->src_addr[5]);

   // todo: switch(ethertype) -> arp_input (0x0806) / ip_input (0x0800)
}

void _start() {
   if(!escalate()) {
      exit(1);
   }

   // find device
   int len = sizeof(drivers)/sizeof(drivers[0]);
   eth_driver_t *driver = NULL;
   for(int i = 0; i < len; i++) {
      if(pci_exists(drivers[i].vendor, drivers[i].device)) {
         driver = &drivers[i];
         break;
      }
   }
   if(!driver) {
      printf("Couldn't find supported device\n");
      exit(1);
   }

   netdev_t *dev = driver->init();
   if(!dev) {
      printf("Driver init failed\n");
      exit(1);
   }

   dev->rx = eth_input; // wire received frames up into the ethernet layer

   // provoke a reply so network gives us an RX frame
   arp_request(dev, pack_ip(GATEWAY_IP)); // ask for the gateway's MAC

   printf("Polling\n");
   while(true) {
      dev->poll(dev); // drain any frames the card received
      yield();
   }

   // currently unreachable
   dev->free(dev);
}
