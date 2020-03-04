#define NETDEV_TYPE_ETHERNET  (0x0001)
#define NETDEV_TYPE_SLIP      (0x0002)

#define NETDEV_FLAG_BROADCAST (0x0001)
#define NETDEV_FLAG_MULTICAST (0x0002)
#define NETDEV_FLAG_P2P       (0x0004)
#define NETDEV_FLAG_LOOPBACK  (0x0008)
#define NETDEV_FLAG_NOARP     (0x0010)
#define NETDEV_FLAG_PROMISC   (0x0020)
#define NETDEV_FLAG_RUNNING   (0x0040)
#define NETDEV_FLAG_UP        (0x0080)

#define NETDEV_PROTO_IP       (0x0800)
#define NETDEV_PROTO_ARP      (0x0806)
#define NETDEV_PROTO_IPV6     (0x86dd)

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

struct netdev;

struct netdev_ops {
    int (*open)(struct netdev *dev);
    int (*stop)(struct netdev *dev);
    int (*xmit)(struct netdev *dev, uint16_t type, const uint8_t *packet, size_t size, const void *dst);
};

struct netdev {
    struct netdev *next;
    char name[IFNAMSIZ];
    uint16_t type;
    uint16_t mtu;
    uint16_t flags;
    uint16_t hlen;
    uint16_t alen;
    uint8_t addr[16];
    uint8_t peer[16];
    uint8_t broadcast[16];
    struct netdev_ops *ops;
    void *priv;
};
