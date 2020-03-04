#include "../types.h"
#include "../defs.h"

#include "../ext/types.h"
#include "../ext/defs.h"

#include "net.h"

static struct netdev *devices;

struct netdev *
netdev_root(void)
{
    return devices;
}

struct netdev *
netdev_alloc(void (*setup)(struct netdev *))
{
    struct netdev *dev;
    static unsigned int index = 0;

    dev = (struct netdev *)kalloc();
    if (!dev) {
        return NULL;
    }
    memset(dev, 0, sizeof(struct netdev));
    snprintf(dev->name, sizeof(dev->name), "net%d", index++);
    setup(dev);
    return dev;
}

int
netdev_register(struct netdev *dev)
{
    cprintf("[net] netdev_register: '%s'\n", dev->name);
    dev->next = devices;
    devices = dev;
    return 0;
}

void
netdev_receive(struct netdev *dev, uint16_t type, uint8_t *packet, unsigned int plen) {
    cprintf("[net] netdev_receive: dev=%s, type=%04x, packet=%p, plen=%u\n", dev->name, type, packet, plen);
}
