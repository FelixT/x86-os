#include "../../prog.h"
#include "../../lib/stdio.h"
#include "../../lib/stdlib.h"
#include "../../../lib/string.h"

#include "netdev.h"
#include "ip.h"
#include "ethernet.h"

// ipv4 layer

#define IP_MTU 1500 // maximum transmission unit

static uint16_t icmp_id = 0x1234;
static uint16_t icmp_seq = 0;

uint8_t *packet = NULL;

typedef struct ip_header_t {
   uint8_t ihl : 4; // internet header length (in 32-bit words)
   uint8_t version : 4; // 4 for ipv4
   uint8_t tos; // type of service/dscp+ecn (unused)
   uint16_t total_len; // header + payload (min 20 - just header, max 65535 in theory/1520 here)
   uint16_t id; // fragment identifier
   uint16_t flags_frag; // flags + fragment offset
   uint8_t ttl; // time to live
   uint8_t protocol; // icmp/tcp/udp
   uint16_t checksum; // header checksum
   uint32_t src; // source ip
   uint32_t dst; // destination ip
} __attribute__((packed)) ip_header_t;

// RFC 1071 internet checksum
static uint16_t ip_checksum(void *data, uint16_t len) {
   uint32_t sum = 0;
   uint16_t *p = (uint16_t*)data;
   while(len > 1) {
      sum += *p++;
      len -= 2;
   }
   if(len) // odd trailing byte
      sum += *(uint8_t*)p;
   while(sum >> 16) // fold carries
      sum = (sum & 0xFFFF) + (sum >> 16);
   return (uint16_t)~sum;
}

// next-hop routing: on-subnet destinations go direct, everything else via the gateway
// single /24 + single gateway for now, routing table later
static uint32_t ip_next_hop(uint32_t dst) {
   uint32_t local = pack_ip(LOCAL_IP);
   if((dst & 0x00FFFFFF) == (local & 0x00FFFFFF)) // same a.b.c (/24) matches 
      return dst;
   return pack_ip(GATEWAY_IP);
}

int ip_send(netdev_t *dev, uint32_t dst_ip, uint8_t protocol, uint8_t *payload, uint16_t len) {
   if(len > IP_MTU - sizeof(ip_header_t)) {
      printf("ip_send: packet dropped length %u\n", len);
      return -1; // todo: fragmentation
   }

   if(!packet) {
      packet = malloc(sizeof(ip_header_t) + IP_MTU); // lazily alloc shared tx buffer
      if(!packet)
         exit(1);
   }
   ip_header_t *ip = (ip_header_t*)packet;

   ip->version = 4;
   ip->ihl = 5; // 5 * 4 = 20 bytes, no options
   ip->tos = 0;
   ip->total_len = htons(sizeof(ip_header_t) + len);
   ip->id = 0; // todo: increment per packet for fragmentation
   ip->flags_frag = 0;
   ip->ttl = 64; // standard default
   ip->protocol = protocol;
   ip->checksum = 0;
   ip->src = pack_ip(LOCAL_IP);
   ip->dst = dst_ip;
   ip->checksum = ip_checksum(ip, sizeof(ip_header_t));

   memcpy(packet + sizeof(ip_header_t), payload, len);
   // pass request to network driver, queues until arp completes
   return net_tx(dev, ip_next_hop(dst_ip), 0x0800, packet, sizeof(ip_header_t) + len);
}

// todo: separate out into files
// == ICMP ==
typedef struct icmp_header_t {
   uint8_t type;
   uint8_t code;
   uint16_t checksum;
   uint32_t rest_of_header;
} __attribute__((packed)) icmp_header_t;

void icmp_input(netdev_t *dev, uint32_t src_ip, uint8_t *payload, uint16_t len) {
   (void)dev;
   if(len < sizeof(icmp_header_t)) return;
   icmp_header_t *head = (icmp_header_t*)payload;
   if(ip_checksum(head, len) != 0) return; // bad checksum

   uint8_t a, b, c, d;
   unpack_ip(src_ip, &a, &b, &c, &d);

   if(head->type == 8) {
      // handle echo request (type 8) - turn it into a reply and send it back
      head->type = 0; // echo reply
      head->checksum = 0; // must zero before recomputing
      head->checksum = ip_checksum(head, len);
      ip_send(dev, src_ip, IP_PROTO_ICMP, (uint8_t*)head, len);
      printf("icmp request from %u.%u.%u.%u\n", a, b, c, d);
   } else if(head->type == 0) {
      // echo reply (type 0)
      uint16_t seq = htons(head->rest_of_header >> 16);
      uint8_t a, b, c, d;
      unpack_ip(src_ip, &a, &b, &c, &d);
      printf("icmp reply from %u.%u.%u.%u seq=%u\n", a, b, c, d, seq);
   } else {
      printf("icmp type %u from %u.%u.%u.%u\n", head->type, a, b, c, d);
   }
}

void icmp_send_echo(netdev_t *dev, uint32_t dst_ip) {
   (void)dev;
   uint8_t buf[sizeof(icmp_header_t) + 32]; // small so just allocate on stack
   icmp_header_t *icmp = (icmp_header_t*)buf;
   icmp->type = 8; // echo request
   icmp->code = 0;
   icmp->rest_of_header = (uint32_t)htons(icmp_id) | ((uint32_t)htons(++icmp_seq) << 16);
   for(int i = 0; i < 32; i++) // arbitrary payload
      buf[sizeof(icmp_header_t) + i] = i;
   icmp->checksum = 0;
   icmp->checksum = ip_checksum(icmp, sizeof(buf));
   ip_send(dev, dst_ip, IP_PROTO_ICMP, buf, sizeof(buf));
}

// == UDP ==
typedef struct udp_header_t {
   uint16_t src_port;
   uint16_t dest_port;
   uint16_t length; // udp header + data
   uint16_t checksum; // optional for ipv4
} __attribute__((packed)) udp_header_t;

// ports passed in host byte order
int udp_send(netdev_t *dev, uint32_t dst_ip, uint16_t dst_port, uint16_t src_port, uint8_t *payload, uint16_t len) {
   uint16_t total = sizeof(udp_header_t) + len;
   uint8_t *packet_udp = malloc(total);
   if(!packet_udp)
      return -1;

   udp_header_t *udp = (udp_header_t*)packet_udp;
   udp->src_port = htons(src_port);
   udp->dest_port = htons(dst_port);
   udp->length = htons(total);
   udp->checksum = 0; // optional, todo
   memcpy(packet_udp+sizeof(udp_header_t), payload, len);
   int result = ip_send(dev, dst_ip, IP_PROTO_UDP, packet_udp, total);
   free(packet_udp);
   return result;
}

void udp_input(netdev_t *dev, uint32_t src_ip, uint8_t *payload, uint16_t len) {
   (void)dev;
   udp_header_t *udp = (udp_header_t*)payload;
   uint16_t udp_len = htons(udp->length);
   if(udp_len < sizeof(udp_header_t) || udp_len > len) return;
   // todo: if checksum != 0, check using pseudo header
   uint8_t a, b, c, d;
   unpack_ip(src_ip, &a, &b, &c, &d);
   printf("UDP length %i src %u.%u.%u.%u:%i dest :%i\n", udp_len, a, b, c, d, htons(udp->src_port), htons(udp->dest_port));
   for(int i = sizeof(udp_header_t); i < udp_len; i++)
      printf("%c", (char)payload[i]);
   printf("\n");
   // send test reply
   char msg[8];
   strcpy(msg, "test");
   udp_send(dev, src_ip, htons(udp->src_port), 7777, (uint8_t*)msg, strlen(msg)+1); // reply
}

void ip_input(netdev_t *dev, uint8_t *frame, uint16_t len) {
   (void)dev;
   ip_header_t *ip = (ip_header_t*)(frame + sizeof(eth_header_t));

   if(len < sizeof(eth_header_t) + sizeof(ip_header_t)) return; // runt
   size_t ihl = (ip->ihl & 0x0F) * 4;
   uint16_t total = htons(ip->total_len);
   if(ihl < sizeof(ip_header_t) || total < ihl || sizeof(eth_header_t) + total > len) return; // length sanity check
   if(ip->version != 4) return; // not ipv4
   if(ip_checksum(ip, ihl) != 0) return; // bad checksum
   if(ip->dst != pack_ip(LOCAL_IP)) return; // not for us (todo: broadcast)

   uint8_t *payload = (uint8_t*)ip + ihl;
   uint16_t payload_len = total - ihl;

   switch(ip->protocol) {
      case IP_PROTO_ICMP:
         icmp_input(dev, ip->src, payload, payload_len);
         break;
      case IP_PROTO_UDP:
         udp_input(dev, ip->src, payload, payload_len);
         break;
      case IP_PROTO_TCP:
         //tcp_input(dev, ip->src, payload, payload_len);
         //break;
      default:
         printf("ip_input.: unsupported protocol %u\n", ip->protocol);
   }
   (void)payload; (void)payload_len;
}
