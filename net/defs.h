struct pci_func;
struct netdev;

// common.c
void            hexdump(void *data, size_t size);

// e1000.c
int             e1000_init(struct pci_func *pcif);
void            e1000intr(void);

// net.c
struct netdev * netdev_alloc(void (*setup)(struct netdev *));
int             netdev_register(struct netdev *dev);
void            netdev_receive(struct netdev *dev, uint16_t type, uint8_t *packet, unsigned int plen);
