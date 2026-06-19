#include "../../prog.h"
#include "../../lib/stdio.h"
#include "../../../lib/string.h"
#include "../../lib/stdlib.h"

#include "netdev.h"
#include "ethernet.h"
#include "ip.h"

// ethernet + arp layer

extern netdev_t *rtl_init(); // linked against rtl driver

uint8_t *frame_buffer;

netdev_t *dev;

// support multiple (statically linked) drivers
// eventually: separate processes with ipc

typedef struct eth_driver_t {
   uint16_t vendor, device;
   netdev_t *(*init)(void);
} eth_driver_t;

static eth_driver_t drivers[] = {
   { 0x10EC, 0x8139, rtl_init }
};

void unpack_ip(uint32_t ip, uint8_t *a, uint8_t *b, uint8_t *c, uint8_t *d) {
   *a = ip & 0xFF;
   *b = (ip >> 8) & 0xFF;
   *c = (ip >> 16) & 0xFF;
   *d = (ip >> 24) & 0xFF;
}

typedef struct eth_arp_t {
   uint16_t htype; // hardware type (1 = ethernet)
   uint16_t ptype; // format of protocol address
   uint8_t hlen; // length of hardware address
   uint8_t plen; // length of protocol address
   uint16_t op; // ARP opcode (command)
   uint8_t sha[6]; // sender hardware address - 6 octet mac addr
   uint32_t spa; // sender IP address
   uint8_t tha[6]; // target hardware address - 6 octet mac addr
   uint32_t tpa; // target IP address
} __attribute__((packed)) eth_arp_t;

#define ARP_CACHE_SIZE 16
#define ARP_QUEUE_SIZE 8
#define ARP_RETRY_TICKS 100 // ticks between arp retransmits
#define ARP_MAX_TRIES 4

typedef enum { ARP_FREE, ARP_PENDING, ARP_RESOLVED } arp_state_t;

typedef struct arp_entry_t {
   uint32_t ip;
   uint8_t mac[6];
   arp_state_t state;
   uint32_t last_req; // tick of last request sent (retransmit timer)
   uint8_t tries; // arp requests sent while pending
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];

// packets waiting on an unresolved next-hop mac
typedef struct pending_pkt_t {
   bool used;
   uint32_t next_hop;
   uint16_t ethertype;
   uint16_t len;
   uint8_t data[ETH_MTU];
} pending_pkt_t;

static pending_pkt_t arp_queue[ARP_QUEUE_SIZE];

static void arp_drop_queue(uint32_t next_hop);

static int arp_find(uint32_t ip) {
   for(int i = 0; i < ARP_CACHE_SIZE; i++)
      if(arp_cache[i].state != ARP_FREE && arp_cache[i].ip == ip)
         return i;
   return -1;
}

static int arp_alloc_slot() {
   for(int i = 0; i < ARP_CACHE_SIZE; i++)
      if(arp_cache[i].state == ARP_FREE) return i;
   // table full - evict in round-robin
   static int next = 0;
   int slot = next;
   next = (next + 1) % ARP_CACHE_SIZE;
   arp_drop_queue(arp_cache[slot].ip); // drop packets queued for the evicted entry
   return slot;
}

static void arp_flush(netdev_t *dev, uint32_t next_hop);

int eth_send(netdev_t *dev, uint8_t dest_mac[6], uint16_t ethertype, uint8_t *payload, uint16_t len) {
   if(len > ETH_MTU) return -1;
   eth_header_t *eth = (eth_header_t*)frame_buffer;
   // ethernet header
   memcpy(eth->dest_addr, dest_mac, 6);
   memcpy(eth->src_addr, dev->mac, 6);
   eth->frame_type = htons(ethertype);
   memcpy(frame_buffer + sizeof(eth_header_t), payload, len);
   return dev->send(dev, frame_buffer, sizeof(eth_header_t) + len);
}

// broadcast an ARP request to discover target_ip MAC address
void arp_request(netdev_t *dev, uint32_t target_ip) {
   eth_arp_t arp;
   // arp payload
   arp.htype = htons(1); // ethernet
   arp.ptype = htons(0x0800); // IPv4
   arp.hlen = 6;
   arp.plen = 4;
   arp.op = htons(1); // request
   memcpy(arp.sha, dev->mac, 6);
   arp.spa = pack_ip(LOCAL_IP);
   memset(arp.tha, 0, 6); // zero'd for receive
   arp.tpa = target_ip;

   uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
   if(eth_send(dev, bcast, 0x0806, (uint8_t*)&arp, sizeof(eth_arp_t)) < 0)
      debug_println("ARP failed\n");
}

// learn a resolved ip->mac mapping (upgrades a pending entry or allocates one)
void arp_update_cache(uint32_t ip, uint8_t mac[6]) {
   int i = arp_find(ip);
   if(i < 0) i = arp_alloc_slot();
   arp_cache[i].ip = ip;
   memcpy(arp_cache[i].mac, mac, 6);
   arp_cache[i].state = ARP_RESOLVED;
}

void arp_input(netdev_t *dev, uint8_t *frame, uint16_t len) {
   if(len < sizeof(eth_header_t) + sizeof(eth_arp_t)) return;
   eth_arp_t *arp = (eth_arp_t*)(frame + sizeof(eth_header_t));
   if(htons(arp->htype) != 1 || htons(arp->ptype) != 0x0800) return; // invalid - only allow ethernet + ipv4
   arp_update_cache(arp->spa, arp->sha);
   arp_flush(dev, arp->spa); // we just learned this mac - drain anything queued for it

   // print
   uint8_t a, b, c, d;
   unpack_ip(arp->spa, &a, &b, &c, &d);
   printf("arp %s from ip %u.%u.%u.%u addr %h:%h:%h:%h:%h:%h\n", htons(arp->op)==1?"request":"reply", a, b, c, d, arp->sha[0], arp->sha[1], arp->sha[2], arp->sha[3], arp->sha[4], arp->sha[5]);

   // handle requests for ip (op 1 = request)
   if(htons(arp->op) == 1 && arp->tpa == pack_ip(LOCAL_IP)) {
      // send
      eth_arp_t reply;
      reply.htype = htons(1); // ethernet
      reply.ptype = htons(0x0800); // ipv4
      reply.hlen = 6; // mac/always 6
      reply.plen = 4; // ipv4/always 4
      reply.op = htons(2); // op 2 = reply
      memcpy(reply.sha, dev->mac, 6); // sender (local) mac addr
      reply.spa = pack_ip(LOCAL_IP); // ip
      memcpy(reply.tha, arp->sha, 6); // target mac addr
      reply.tpa = arp->spa; // target ip
      eth_send(dev, arp->sha, 0x0806, (uint8_t*)&reply, sizeof(eth_arp_t));
   }
}

bool arp_lookup(uint32_t ip, uint8_t mac[6]) {
   int i = arp_find(ip);
   if(i < 0 || arp_cache[i].state != ARP_RESOLVED)
      return false;
   memcpy(mac, arp_cache[i].mac, 6);
   return true;
}

static void arp_enqueue(uint32_t next_hop, uint16_t ethertype, uint8_t *data, uint16_t len) {
   for(int i = 0; i < ARP_QUEUE_SIZE; i++) {
      if(!arp_queue[i].used) {
         arp_queue[i].used = true;
         arp_queue[i].next_hop = next_hop;
         arp_queue[i].ethertype = ethertype;
         arp_queue[i].len = len;
         memcpy(arp_queue[i].data, data, len);
         return;
      }
   }
   debug_println("arp queue full, dropping packet\n");
}

static void arp_drop_queue(uint32_t next_hop) {
   for(int i = 0; i < ARP_QUEUE_SIZE; i++)
      if(arp_queue[i].used && arp_queue[i].next_hop == next_hop)
         arp_queue[i].used = false;
}

// transmit any packets waiting on a resolved next hop
static void arp_flush(netdev_t *dev, uint32_t next_hop) {
   uint8_t mac[6];
   if(!arp_lookup(next_hop, mac)) return;
   for(int i = 0; i < ARP_QUEUE_SIZE; i++) {
      if(arp_queue[i].used && arp_queue[i].next_hop == next_hop) {
         eth_send(dev, mac, arp_queue[i].ethertype, arp_queue[i].data, arp_queue[i].len);
         arp_queue[i].used = false;
      }
   }
}

static void arp_resolve(netdev_t *dev, uint32_t ip) {
   if(arp_find(ip) >= 0) return; // already resolved or pending
   int i = arp_alloc_slot();
   arp_cache[i].ip = ip;
   arp_cache[i].state = ARP_PENDING;
   arp_cache[i].tries = 1;
   arp_cache[i].last_req = get_tick();
   arp_request(dev, ip);
}

int net_tx(netdev_t *dev, uint32_t next_hop, uint16_t ethertype, uint8_t *payload, uint16_t len) {
   uint8_t mac[6];
   if(arp_lookup(next_hop, mac)) // already know mac, send immediately
      return eth_send(dev, mac, ethertype, payload, len);
   // otherwise queue next hop ip & wait for reply
   arp_enqueue(next_hop, ethertype, payload, len);
   arp_resolve(dev, next_hop);
   return 0; // queued
}

// retransmit/expire pending arp requests
void arp_tick(netdev_t *dev) {
   uint32_t now = get_tick();
   for(int i = 0; i < ARP_CACHE_SIZE; i++) {
      arp_entry_t *e = &arp_cache[i];
      if(e->state != ARP_PENDING) continue;
      if(now - e->last_req < ARP_RETRY_TICKS) continue;
      if(e->tries >= ARP_MAX_TRIES) {
         arp_drop_queue(e->ip); // give up & drop from queue
         e->state = ARP_FREE;
      } else {
         e->tries++;
         e->last_req = now;
         arp_request(dev, e->ip);
      }
   }
}

void eth_input(netdev_t *dev, uint8_t *frame, uint16_t len) {
   (void)dev;
   if(len < sizeof(eth_header_t)) return; // runt frame - too short for an ethernet header
   eth_header_t *eth = (eth_header_t*)frame;
   uint16_t ethertype = htons(eth->frame_type); // inverse swap

   switch(ethertype) {
      case 0x0806: // ARP
         arp_input(dev, frame, len);
         break;
      case 0x0800: // IPv4
         ip_input(dev, frame, len);
         break;
      //case 0x86DD: // IPv6
   }
}

static bool parse_ip(const char *s, uint32_t *out) {
   uint8_t oct[4];
   int idx = 0, digits = 0, val = 0;
   for(;; s++) {
      char c = *s;
      if(c >= '0' && c <= '9') {
         val = val * 10 + (c - '0');
         if(val > 255) return false;
         digits++;
      } else if(c == '.' || c == '\0') {
         if(digits == 0 || idx > 3) return false;
         oct[idx++] = val;
         val = 0; digits = 0;
         if(c == '\0') break;
      } else {
         return false;
      }
   }
   if(idx != 4) return false;
   *out = pack_ip(oct[0], oct[1], oct[2], oct[3]);
   return true;
}

void callback(char *buffer) {
   char cmd[16], arg[64];
   if(!strsplit(cmd, arg, buffer, ' ')) {
      strcpy(cmd, buffer); // no argument
      arg[0] = '\0';
   }

   if(strequ(cmd, "ping")) {
      uint32_t ip;
      if(parse_ip(arg, &ip))
         icmp_send_echo(dev, ip);
      else
         printf("usage: ping <a.b.c.d>\n");
   }

   end_subroutine();
}

void _start() {
   if(!escalate()) {
      exit(1);
   }
   frame_buffer = malloc(sizeof(eth_header_t) + ETH_MTU);
   if(!frame_buffer)
      exit(1);

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

   dev = driver->init();
   if(!dev) {
      printf("Driver init failed\n");
      exit(1);
   }

   dev->rx = &eth_input; // setup callback

   // prime the gateway mac (tracked, so it retransmits if the request is lost)
   arp_resolve(dev, pack_ip(GATEWAY_IP));

   printf("Polling\n");

   override_term_checkcmd(&callback); // debugging commands

   while(true) {
      dev->poll(dev); // drain any frames the card received
      arp_tick(dev); // retransmit/expire pending arp requests
      yield();
   }

   // currently unreachable
   dev->free(dev);
   free(frame_buffer);
}
