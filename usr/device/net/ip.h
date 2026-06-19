// ipv4 layer

#ifndef IP_H
#define IP_H

#include <stdint.h>
#include "netdev.h"

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

void ip_input(netdev_t *dev, uint8_t *frame, uint16_t len);
int ip_send(netdev_t *dev, uint32_t dst_ip, uint8_t protocol, uint8_t *payload, uint16_t len);

void icmp_send_echo(netdev_t *dev, uint32_t dst_ip);

#endif
