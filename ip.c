// Copyright (c) 2012-2020 YAMAMOTO Masaya
// SPDX-License-Identifier: MIT

#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "net.h"
#include "ethernet.h"
#include "ip.h"

#define IP_VERSION_IPV4 4

#define IP_ROUTE_TABLE_SIZE 8

struct ip_route {
    uint8_t used;
    ip_addr_t network;
    ip_addr_t netmask;
    ip_addr_t nexthop;
    struct netif *netif;
};

struct ip_protocol {
    struct ip_protocol *next;
    uint8_t type;
    void (*handler)(uint8_t *payload, size_t len, ip_addr_t *src, ip_addr_t *dst, struct netif *netif);
};

struct ip_hdr {
    uint8_t vhl;
    uint8_t tos;
    uint16_t len;
    uint16_t id;
    uint16_t offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t sum;
    ip_addr_t src;
    ip_addr_t dst;
    uint8_t options[0];
};

const ip_addr_t IP_ADDR_ANY       = 0x00000000;
const ip_addr_t IP_ADDR_BROADCAST = 0xffffffff;

static struct spinlock iplock;
static struct ip_route route_table[IP_ROUTE_TABLE_SIZE];
static struct ip_protocol *protocols;

int
ip_addr_pton (const char *p, ip_addr_t *n) {
    char *sp, *ep;
    int idx;
    long ret;

    sp = (char *)p;
    for (idx = 0; idx < 4; idx++) {
        ret = strtol(sp, &ep, 10);
        if (ret < 0 || ret > 255) {
            return -1;
        }
        if (ep == sp) {
            return -1;
        }
        if ((idx == 3 && *ep != '\0') || (idx != 3 && *ep != '.')) {
            return -1;
        }
        ((uint8_t *)n)[idx] = ret;
        sp = ep + 1;
    }
    return 0;
}

char *
ip_addr_ntop (const ip_addr_t *n, char *p, size_t size) {
    uint8_t *ptr;

    ptr = (uint8_t *)n;
    snprintf(p, size, "%d.%d.%d.%d",
        ptr[0], ptr[1], ptr[2], ptr[3]);
    return p;
}

void
ip_dump (struct netif *netif, uint8_t *packet, size_t plen) {
    struct netif_ip *iface;
    char addr[IP_ADDR_STR_LEN];
    struct ip_hdr *hdr;
    uint8_t hl;
    uint16_t offset;

    iface = (struct netif_ip *)netif;
    cprintf(" dev: %s (%s)\n", netif->dev->name, ip_addr_ntop(&iface->unicast, addr, sizeof(addr)));
    hdr = (struct ip_hdr *)packet;
    hl = hdr->vhl & 0x0f;
    cprintf("      vhl: %02x [v: %u, hl: %u (%u)]\n", hdr->vhl, (hdr->vhl & 0xf0) >> 4, hl, hl << 2);
    cprintf("      tos: %02x\n", hdr->tos);
    cprintf("      len: %u\n", ntoh16(hdr->len));
    cprintf("       id: %u\n", ntoh16(hdr->id));
    offset = ntoh16(hdr->offset);
    cprintf("   offset: 0x%04x [flags=%x, offset=%u]\n", offset, (offset & 0xe0) >> 5, offset & 0x1f);
    cprintf("      ttl: %u\n", hdr->ttl);
    cprintf(" protocol: %u\n", hdr->protocol);
    cprintf("      sum: 0x%04x\n", ntoh16(hdr->sum));
    cprintf("      src: %s\n", ip_addr_ntop(&hdr->src, addr, sizeof(addr)));
    cprintf("      dst: %s\n", ip_addr_ntop(&hdr->dst, addr, sizeof(addr)));
    hexdump(packet, plen);
}

/*
 * IP ROUTING
 */

static int
ip_route_add (ip_addr_t network, ip_addr_t netmask, ip_addr_t nexthop, struct netif *netif) {
    struct ip_route *route;

    for (route = route_table; route < array_tailof(route_table); route++) {
        if (!route->used) {
            route->used = 1;
            route->network = network;
            route->netmask = netmask;
            route->nexthop = nexthop;
            route->netif = netif;
            return 0;
        }
    }
    return -1;
}

static int
ip_route_del (struct netif *netif) {
    struct ip_route *route;

    for (route = route_table; route < array_tailof(route_table); route++) {
        if (route->used) {
            if (route->netif == netif) {
                route->used = 0;
            }
        }
    }
    return 0;
}

static struct ip_route *
ip_route_lookup (const struct netif *netif, const ip_addr_t *dst) {
    struct ip_route *route, *candidate = NULL;
    for (route = route_table; route < array_tailof(route_table); route++) {
        if (route->used && (*dst & route->netmask) == route->network && (!netif || route->netif == netif)) {
            if (!candidate || ntoh32(candidate->netmask) < ntoh32(route->netmask)) {
                candidate = route;
            }
        }
    }
    return candidate;
}

/*
 * IP INTERFACE
 */

struct netif *
ip_netif_alloc (ip_addr_t unicast, ip_addr_t netmask, ip_addr_t gateway) {
    struct netif_ip *iface;
    ip_addr_t gw;

    iface = (struct netif_ip *)kalloc();
    if (!iface) {
        return NULL;
    }
    ((struct netif *)iface)->next = NULL;
    ((struct netif *)iface)->family = NETIF_FAMILY_IPV4;
    ((struct netif *)iface)->dev = NULL;
    iface->unicast = unicast;
    iface->netmask = netmask;
    iface->network = iface->unicast & iface->netmask;
    iface->broadcast = iface->network | ~iface->netmask;
    if (ip_route_add(iface->network, iface->netmask, IP_ADDR_ANY, (struct netif *)iface) == -1) {
        kfree((char*)iface);
        return NULL;
    }
    if (gateway) {
        if (ip_route_add(IP_ADDR_ANY, IP_ADDR_ANY, gateway, (struct netif *)iface) == -1) {
            kfree((char*)iface);
            return NULL;
        }
    }
    return (struct netif *)iface;
}

struct netif *
ip_netif_register (struct netdev *dev, const char *addr, const char *netmask, const char *gateway) {
    struct netif *netif;
    ip_addr_t unicast, mask, gw = 0;

    if (ip_addr_pton(addr, &unicast) == -1) {
        return NULL;
    }
    if (ip_addr_pton(netmask, &mask) == -1) {
        return NULL;
    }
    if (gateway) {
        if (ip_addr_pton(gateway, &gw) == -1) {
            return NULL;
        }
    }
    netif = ip_netif_alloc(unicast, mask, gw);
    if (!netif) {
        return NULL;
    }
    if (netdev_add_netif(dev, netif) == -1) {
        kfree((char*)netif);
        return NULL;
    }
    return netif;
}

int
ip_netif_reconfigure (struct netif *netif, ip_addr_t unicast, ip_addr_t netmask, ip_addr_t gateway) {
    struct netif_ip *iface;
    ip_addr_t gw;

    iface = (struct netif_ip *)netif;
    ip_route_del(netif);
    iface->unicast = unicast;
    iface->netmask = netmask;
    iface->network = iface->unicast & iface->netmask;
    iface->broadcast = iface->network | ~iface->netmask;
    if (ip_route_add(iface->network, iface->netmask, IP_ADDR_ANY, netif) == -1) {
        return -1;
    }
    if (gateway) {
        if (ip_route_add(IP_ADDR_ANY, IP_ADDR_ANY, gateway, netif) == -1) {
            return -1;
        }
    }
    return 0;
}

struct netif *
ip_netif_by_addr (ip_addr_t *addr) {
    struct netdev *dev;
    struct netif *entry;

    for (dev = netdev_root(); dev; dev = dev->next) {
        for (entry = dev->ifs; entry; entry = entry->next) {
            if (entry->family == NETIF_FAMILY_IPV4 && ((struct netif_ip *)entry)->unicast == *addr) {
                return entry;
            }
        }
    }
    return NULL;
}

struct netif *
ip_netif_by_peer (ip_addr_t *peer) {
    struct ip_route *route;

    route = ip_route_lookup(NULL, peer);
    if (!route) {
        return NULL;
    }
    return route->netif;
}

/*
 * IP CORE
 */

static void
ip_rx (uint8_t *dgram, size_t dlen, struct netdev *dev) {
    struct ip_hdr *hdr;
    uint16_t hlen, offset;
    struct netif_ip *iface;
    uint8_t *payload;
    size_t plen;
    struct ip_protocol *protocol;

    if (dlen < sizeof(struct ip_hdr)) {
        return;
    }
    hdr = (struct ip_hdr *)dgram;
    if ((hdr->vhl >> 4) != IP_VERSION_IPV4) {
        cprintf("not ipv4 packet.\n");
        return;
    }
    hlen = (hdr->vhl & 0x0f) << 2;
    if (dlen < hlen || dlen < ntoh16(hdr->len)) {
        cprintf("ip packet length error.\n");
        return;
    }
    if (cksum16((uint16_t *)hdr, hlen, 0) != 0) {
        cprintf("ip checksum error.\n");
        return;
    }
    if (!hdr->ttl) {
        cprintf("ip packet was dead (TTL=0).\n");
        return;
    }
    iface = (struct netif_ip *)netdev_get_netif(dev, NETIF_FAMILY_IPV4);
    if (!iface) {
        cprintf("ip unknown interface.\n");
        return;
    }
    if (hdr->dst != iface->unicast) {
        if (hdr->dst != iface->broadcast && hdr->dst != IP_ADDR_BROADCAST) {
            /* for other host */
            return;
        }
    }
#ifdef DEBUG
    cprintf(">>> ip_rx <<<\n");
    ip_dump((struct netif *)iface, dgram, dlen);
#endif
    payload = (uint8_t *)hdr + hlen;
    plen = ntoh16(hdr->len) - hlen;
    offset = ntoh16(hdr->offset);
    if (offset & 0x2000 || offset & 0x1fff) {
        /* fragments */
        cprintf("don't support IP fragments\n");
        return;
    }
    for (protocol = protocols; protocol; protocol = protocol->next) {
        if (protocol->type == hdr->protocol) {
            protocol->handler(payload, plen, &hdr->src, &hdr->dst, (struct netif *)iface);
            break;
        }
    }
}

static int
ip_tx_netdev (struct netif *netif, uint8_t *packet, size_t plen, const ip_addr_t *dst) {
    uint8_t ha[128] = {};
    ssize_t ret;

    if (!(netif->dev->flags & NETDEV_FLAG_NOARP)) {
        if (dst) {
            ret = arp_resolve(netif, dst, (void *)ha, packet, plen);
            if (ret != 1) {
                return ret;
            }
        } else {
            memcpy(ha, netif->dev->broadcast, netif->dev->alen);
        }
    }
    if (netif->dev->ops->xmit(netif->dev, ETHERNET_TYPE_IP, packet, plen, (void *)ha) != (ssize_t)plen) {
        return -1;
    }
    return 1;
}

static int
ip_tx_core (struct netif *netif, uint8_t protocol, const uint8_t *buf, size_t len, const ip_addr_t *src, const ip_addr_t *dst, const ip_addr_t *nexthop, uint16_t id, uint16_t offset) {
    uint8_t packet[4096];
    struct ip_hdr *hdr;
    uint16_t hlen;

    hdr = (struct ip_hdr *)packet;
    hlen = sizeof(struct ip_hdr);
    hdr->vhl = (IP_VERSION_IPV4 << 4) | (hlen >> 2);
    hdr->tos = 0;
    hdr->len = hton16(hlen + len);
    hdr->id = hton16(id);
    hdr->offset = hton16(offset);
    hdr->ttl = 0xff;
    hdr->protocol = protocol;
    hdr->sum = 0;
    hdr->src = src ? *src : ((struct netif_ip *)netif)->unicast;
    hdr->dst = *dst;
    hdr->sum = cksum16((uint16_t *)hdr, hlen, 0);
    memcpy(hdr + 1, buf, len);
#ifdef DEBUG
    cprintf(">>> ip_tx_core <<<\n");
    ip_dump(netif, (uint8_t *)packet, hlen + len);
#endif
    return ip_tx_netdev(netif, (uint8_t *)packet, hlen + len, nexthop);
}

static uint16_t
ip_generate_id (void) {
    static uint16_t id = 128;
    uint16_t ret;

    acquire(&iplock);
    ret = id++;
    release(&iplock);
    return ret;
}

ssize_t
ip_tx (struct netif *netif, uint8_t protocol, const uint8_t *buf, size_t len, const ip_addr_t *dst) {
    struct ip_route *route;
    ip_addr_t *nexthop = NULL, *src = NULL;
    uint16_t id, flag, offset;
    size_t done, slen;

    if (netif && *dst == IP_ADDR_BROADCAST) {
        nexthop = NULL;
    } else {
        route = ip_route_lookup(NULL, dst);
        if (!route) {
            cprintf("ip no route to host.\n");
            return -1;
        }
        if (netif) {
            src = &((struct netif_ip *)netif)->unicast;
        }
        netif = route->netif;
        nexthop = (ip_addr_t *)(route->nexthop ? &route->nexthop : dst);
    }
    id = ip_generate_id();
    for (done = 0; done < len; done += slen) {
        slen = MIN((len - done), (size_t)(netif->dev->mtu - IP_HDR_SIZE_MIN));
        flag = ((done + slen) < len) ? 0x2000 : 0x0000;
        offset = flag | ((done >> 3) & 0x1fff);
        if (ip_tx_core(netif, protocol, buf + done, slen, src, dst, nexthop, id, offset) == -1) {
            return -1;
        }
    }
    return len;
}

int
ip_add_protocol (uint8_t type, void (*handler)(uint8_t *payload, size_t len, ip_addr_t *src, ip_addr_t *dst, struct netif *netif)) {
    struct ip_protocol *p;

    p = (struct ip_protocol *)kalloc();
    if (!p) {
        return -1;
    }
    p->next = protocols;
    p->type = type;
    p->handler = handler;
    protocols = p;
    return 0;
}

int
ip_init (void) {
    initlock(&iplock, "ip");
    netproto_register(NETPROTO_TYPE_IP, ip_rx);
    return 0;
}
