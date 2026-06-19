// ethernet + arp layer

#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <stdbool.h>
#include "netdev.h"

// network config - assumes SLIRP setup (todo: DHCP i.e. discovery)
#define GATEWAY_IP 10, 5, 0, 1
#define LOCAL_IP 10, 5, 0, 4

#define ETH_MTU 1500 // maximum transmission unit

// ethertypes
#define ETHERTYPE_IP  0x0800
#define ETHERTYPE_ARP 0x0806

typedef struct eth_header_t {
   uint8_t dest_addr[6]; // destination hardware address
   uint8_t src_addr[6];  // source hardware address
   uint16_t frame_type;  // ethernet frame type
} __attribute__((packed)) eth_header_t;

// host byte order to network byte order (x86 is little endian, network is big endian)
static inline uint16_t htons(uint16_t x) {
   return (x >> 8) | (x << 8);
}

static inline uint32_t pack_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
   return (uint32_t)((a) | ((b)<<8) | ((c)<<16) | ((d)<<24));
}

void unpack_ip(uint32_t ip, uint8_t *a, uint8_t *b, uint8_t *c, uint8_t *d);

int eth_send(netdev_t *dev, uint8_t dest_mac[6], uint16_t ethertype, uint8_t *payload, uint16_t len);
bool arp_lookup(uint32_t ip, uint8_t mac[6]);

int net_tx(netdev_t *dev, uint32_t next_hop, uint16_t ethertype, uint8_t *payload, uint16_t len); // queues requests until arp resolves
void arp_tick(netdev_t *dev); // handle arp retransmit/timeout

#endif
