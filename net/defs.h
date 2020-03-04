struct pci_func;
struct netdev;

// common.c
void            hexdump(void *data, size_t size);
uint16_t        hton16(uint16_t h);
uint16_t        ntoh16(uint16_t n);
uint32_t        hton32(uint32_t h);
uint32_t        ntoh32(uint32_t n);

// e1000.c
int             e1000_init(struct pci_func *pcif);
void            e1000intr(void);

// ethernet.c
int             ethernet_addr_pton(const char *p, uint8_t *n);
char *          ethernet_addr_ntop(const uint8_t *n, char *p, size_t size);
ssize_t         ethernet_rx_helper(struct netdev *dev, uint8_t *frame, size_t flen, void (*cb)(struct netdev*, uint16_t, uint8_t*, size_t));
ssize_t         ethernet_tx_helper(struct netdev *dev, uint16_t type, const uint8_t *payload, size_t plen, const void *dst, ssize_t (*cb)(struct netdev*, uint8_t*, size_t));
void            ethernet_netdev_setup(struct netdev *dev);

// net.c
struct netdev * netdev_alloc(void (*setup)(struct netdev *));
int             netdev_register(struct netdev *dev);
void            netdev_receive(struct netdev *dev, uint16_t type, uint8_t *packet, unsigned int plen);
