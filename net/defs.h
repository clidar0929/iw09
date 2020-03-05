struct pci_func;
struct netdev;
struct netif;

// arp.c
int             arp_init(void);

// common.c
void            hexdump(void *data, size_t size);
uint16_t        hton16(uint16_t h);
uint16_t        ntoh16(uint16_t n);
uint32_t        hton32(uint32_t h);
uint32_t        ntoh32(uint32_t n);
time_t          time(time_t *t);

// e1000.c
int             e1000_init(struct pci_func *pcif);
void            e1000intr(void);

// ethernet.c
int             ethernet_addr_pton(const char *p, uint8_t *n);
char *          ethernet_addr_ntop(const uint8_t *n, char *p, size_t size);
ssize_t         ethernet_rx_helper(struct netdev *dev, uint8_t *frame, size_t flen, void (*cb)(struct netdev*, uint16_t, uint8_t*, size_t));
ssize_t         ethernet_tx_helper(struct netdev *dev, uint16_t type, const uint8_t *payload, size_t plen, const void *dst, ssize_t (*cb)(struct netdev*, uint8_t*, size_t));
void            ethernet_netdev_setup(struct netdev *dev);

// ip.c
int             ip_addr_pton (const char *p, ip_addr_t *n);
char *          ip_addr_ntop (const ip_addr_t *n, char *p, size_t size);

// net.c
struct netdev * netdev_alloc(void (*setup)(struct netdev *));
int             netdev_register(struct netdev *dev);
void            netdev_receive(struct netdev *dev, uint16_t type, uint8_t *packet, unsigned int plen);
int             netdev_add_netif(struct netdev *dev, struct netif *netif);
struct netif *  netdev_get_netif(struct netdev *dev, int family);
int             netproto_register(unsigned short type, void (*handler)(uint8_t *packet, size_t plen, struct netdev *dev));
void            netinit(void);

#define sizeof_member(s, m) sizeof(((s *)NULL)->m)
#define array_tailof(x) (x + (sizeof(x) / sizeof(*x)))
#define array_offset(x, y) (((uintptr_t)y - (uintptr_t)x) / sizeof(*y))
