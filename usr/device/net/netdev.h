// device neutral nic interface

#ifndef NETDEV_H
#define NETDEV_H

#include <stdint.h>

typedef struct netdev_t netdev_t;

struct netdev_t {
   uint8_t mac[6];
   void *state;

   // implemented by driver
   int (*send)(netdev_t *dev, uint8_t *frame, uint16_t len);
   void (*poll)(netdev_t *dev); // drain hardware, call dev->rx for each frame
   void (*free)(netdev_t *dev);

   // set by the upper layer (ethernet), called from poll()
   void (*rx)(netdev_t *dev, uint8_t *frame, uint16_t len);
};

#endif
