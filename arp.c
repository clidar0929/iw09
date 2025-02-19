// Copyright (c) 2012-2020 YAMAMOTO Masaya
// SPDX-License-Identifier: MIT

#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "net.h"
#include "ethernet.h"
#include "arp.h"
#include "ip.h"

#define ARP_HRD_ETHERNET 0x0001

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

#define ARP_TABLE_SIZE 4096
#define ARP_TABLE_TIMEOUT_SEC 300

#define DEBUG

struct arp_hdr {
    uint16_t hrd;
    uint16_t pro;
    uint8_t hln;
    uint8_t pln;
    uint16_t op;
};

struct arp_ethernet {
    struct arp_hdr hdr;
    uint8_t sha[ETHERNET_ADDR_LEN];
    ip_addr_t spa;
    uint8_t tha[ETHERNET_ADDR_LEN];
    ip_addr_t tpa;
} __attribute__ ((packed));

struct arp_entry {
    unsigned char used;
    ip_addr_t pa;
    uint8_t ha[ETHERNET_ADDR_LEN];
    time_t timestamp;
    void *data;
    size_t len;
    struct netif *netif;
};

static struct spinlock arplock;
static struct arp_entry arp_table[ARP_TABLE_SIZE];
static time_t timestamp;

static char *
arp_opcode_ntop (uint16_t opcode) {
    switch (ntoh16(opcode)) {
    case ARP_OP_REQUEST:
        return "REQUEST";
    case ARP_OP_REPLY:
        return "REPLY";
    }
    return "UNKNOWN";
}

void
arp_dump (uint8_t *packet, size_t plen) {
    struct arp_ethernet *message;
    char addr[128];

    message = (struct arp_ethernet *)packet;
    cprintf(" hrd: 0x%04x\n", ntoh16(message->hdr.hrd));
    cprintf(" pro: 0x%04x\n", ntoh16(message->hdr.pro));
    cprintf(" hln: %u\n", message->hdr.hln);
    cprintf(" pln: %u\n", message->hdr.pln);
    cprintf("  op: %u (%s)\n", ntoh16(message->hdr.op), arp_opcode_ntop(message->hdr.op));
    cprintf(" sha: %s\n", ethernet_addr_ntop(message->sha, addr, sizeof(addr)));
    cprintf(" spa: %s\n", ip_addr_ntop(&message->spa, addr, sizeof(addr)));
    cprintf(" tha: %s\n", ethernet_addr_ntop(message->tha, addr, sizeof(addr)));
    cprintf(" tpa: %s\n", ip_addr_ntop(&message->tpa, addr, sizeof(addr)));
}

static struct arp_entry *
arp_table_select (const ip_addr_t *pa) {
    struct arp_entry *entry;

    for (entry = arp_table; entry < array_tailof(arp_table); entry++) {
        if (entry->used && entry->pa == *pa) {
            return entry;
        }
    }
    return NULL;
}

// static int
// arp_table_update (struct netdev *dev, const ip_addr_t *pa, const uint8_t *ha) {
//     struct arp_entry *entry;

//     entry = arp_table_select(pa);
//     if (!entry) {
//         return -1;
//     }
//     memcpy(entry->ha, ha, ETHERNET_ADDR_LEN);
//     time(&entry->timestamp);
//     if (entry->data) {
//         if (entry->netif->dev != dev) {
//             /* warning: receive response from unintended device */
//             dev = entry->netif->dev;
//         }
//         dev->ops->xmit(dev, ETHERNET_TYPE_IP, (uint8_t *)entry->data, entry->len, entry->ha);
//         kfree(entry->data);
//         entry->data = NULL;
//         entry->len = 0;
//     }
//     //pthread_cond_broadcast(&entry->cond);
//     return 0;
// }

static int
arp_table_update (struct netdev *dev, const ip_addr_t *pa, const uint8_t *ha) {
    struct arp_entry *entry;

    entry = arp_table_select(pa);
    if (!entry) {
        return -1;
    }
    memcpy(entry->ha, ha, ETHERNET_ADDR_LEN);
    time(&entry->timestamp);
    if (entry->data) {
        if (entry->netif->dev != dev) {
            /* warning: receive response from unintended device */
            dev = entry->netif->dev;
        }
        dev->ops->xmit(dev, ETHERNET_TYPE_IP, (uint8_t *)entry->data, entry->len, entry->ha);
        kfree(entry->data);
        entry->data = NULL;
        entry->len = 0;
    }
    cprintf("\nHMMM\n");
    return 0;
}


static struct arp_entry *
arp_table_freespace (void) {
    struct arp_entry *entry;

    for (entry = arp_table; entry < array_tailof(arp_table); entry++) {
        if (!entry->used) {
            return entry;
        }
    }
    return NULL;
}

static int
arp_table_insert (const ip_addr_t *pa, const uint8_t *ha) {
    struct arp_entry *entry;

    entry = arp_table_freespace();
    if (!entry) {
        return -1;
    }
    entry->used = 1;
    entry->pa = *pa;
    memcpy(entry->ha, ha, ETHERNET_ADDR_LEN);
    time(&entry->timestamp);
    //pthread_cond_broadcast(&entry->cond);

    return 0;
}

static void
arp_entry_clear (struct arp_entry *entry) {
    entry->used = 0;
    entry->pa = 0;
    memset(entry->ha, 0, ETHERNET_ADDR_LEN);
    //entry->timestamp = 0;
    if (entry->data) {
        kfree(entry->data);
        entry->data = NULL;
        entry->len = 0;
    }
    entry->netif = NULL;
    /* !!! Don't touch entry->cond !!! */
}

static void
arp_table_patrol (void) {
    struct arp_entry *entry;

    for (entry = arp_table; entry < array_tailof(arp_table); entry++) {
        if (entry->used && timestamp - entry->timestamp > ARP_TABLE_TIMEOUT_SEC) {
            arp_entry_clear(entry);
            //pthread_cond_broadcast(&entry->cond);
        }
    }
}

static int
arp_send_request (struct netif *netif, const ip_addr_t *tpa) {
    struct arp_ethernet request;

    if (!tpa) {
        return -1;
    }
    request.hdr.hrd = hton16(ARP_HRD_ETHERNET);
    request.hdr.pro = hton16(ETHERNET_TYPE_IP);
    request.hdr.hln = ETHERNET_ADDR_LEN;
    request.hdr.pln = IP_ADDR_LEN;
    request.hdr.op = hton16(ARP_OP_REQUEST);
    memcpy(request.sha, netif->dev->addr, ETHERNET_ADDR_LEN);
    request.spa = ((struct netif_ip *)netif)->unicast;
    memset(request.tha, 0, ETHERNET_ADDR_LEN);
    request.tpa = *tpa;
#ifdef DEBUG
    cprintf(">>> arp_send_request <<<\n");
    arp_dump((uint8_t *)&request, sizeof(request));
#endif
    if (netif->dev->ops->xmit(netif->dev, ETHERNET_TYPE_ARP, (uint8_t *)&request, sizeof(request), ETHERNET_ADDR_BROADCAST) == -1) {
        return -1;
    }
    return 0;
}

static int
arp_send_reply (struct netif *netif, const uint8_t *tha, const ip_addr_t *tpa, const uint8_t *dst) {
    struct arp_ethernet reply;

    if (!tha || !tpa) {
        return -1;
    }
    reply.hdr.hrd = hton16(ARP_HRD_ETHERNET);
    reply.hdr.pro = hton16(ETHERNET_TYPE_IP);
    reply.hdr.hln = ETHERNET_ADDR_LEN;
    reply.hdr.pln = IP_ADDR_LEN;
    reply.hdr.op = hton16(ARP_OP_REPLY);
    memcpy(reply.sha, netif->dev->addr, ETHERNET_ADDR_LEN);
    reply.spa = ((struct netif_ip *)netif)->unicast;
    memcpy(reply.tha, tha, ETHERNET_ADDR_LEN);
    reply.tpa = *tpa;
#ifdef DEBUG
    cprintf(">>> arp_send_reply <<<\n");
    arp_dump((uint8_t *)&reply, sizeof(reply));
#endif
    if (netif->dev->ops->xmit(netif->dev, ETHERNET_TYPE_ARP, (uint8_t *)&reply, sizeof(reply), dst) < 0) {
        return -1;
    }
    return 0;
}

static void
arp_rx (uint8_t *packet, size_t plen, struct netdev *dev) {
    struct arp_ethernet *message;
    time_t now;
    int marge = 0;
    struct netif *netif;

    if (plen < sizeof(struct arp_ethernet)) {
        cprintf("ISSUE1\n");
        return;
    }
    message = (struct arp_ethernet *)packet;
    if (ntoh16(message->hdr.hrd) != ARP_HRD_ETHERNET) {
        cprintf("ISSUE2\n");
        return;
    }
    if (ntoh16(message->hdr.pro) != ETHERNET_TYPE_IP) {
        cprintf("ISSUE3\n");
        return;
    }
    if (message->hdr.hln != ETHERNET_ADDR_LEN) {
        cprintf("ISSUE4\n");
        return;
    }
    if (message->hdr.pln != IP_ADDR_LEN) {
        cprintf("ISSUE5\n");
        return;
    }
#ifdef DEBUG
    cprintf(">>> arp_rx <<<\n");
    arp_dump(packet, plen);
#endif
    acquire(&arplock);
    time(&now);
    if (now - timestamp > 10) {
        timestamp = now;
        arp_table_patrol();
    }
    marge = (arp_table_update(dev, &message->spa, message->sha) == 0) ? 1 : 0;
    release(&arplock);
    netif = netdev_get_netif(dev, NETIF_FAMILY_IPV4);
    //wakeup(netif); // Wake up any process sleeping on this entry
    if (netif && ((struct netif_ip *)netif)->unicast == message->tpa) {
        if (!marge) {
            acquire(&arplock);
            arp_table_insert(&message->spa, message->sha);
            release(&arplock);
        }
        if (ntoh16(message->hdr.op) == ARP_OP_REQUEST) {
            arp_send_reply(netif, message->sha, &message->spa, message->sha);
        }
    }
    return;
}

int
arp_resolve (struct netif *netif, const ip_addr_t *pa, uint8_t *ha, const void *data, size_t len) {
    struct arp_entry *entry;
    int ret;
    ip_addr_t addr1;
    ip_addr_t addr2;
    uint8_t MAC[ETHERNET_ADDR_LEN] = {"\x00\x00\x00\x00\x00\x00"};

    acquire(&arplock);

    entry = arp_table_select(pa);
    if (entry) {
        if (memcmp(entry->ha, ETHERNET_ADDR_ANY, ETHERNET_ADDR_LEN) == 0) {
            arp_send_request(netif, pa); /* just in case packet loss */
            cprintf("\nWAHHHH\n");
            sleep(netif, &arplock); // Sleep while waiting for ARP reply
            memcpy(ha, entry->ha, ETHERNET_ADDR_LEN);
            release(&arplock);
            return ARP_RESOLVE_FOUND;
        }
        memcpy(ha, entry->ha, ETHERNET_ADDR_LEN);
        release(&arplock);
        return ARP_RESOLVE_FOUND;
    }

    if(ip_addr_pton("192.168.0.8", &addr1) == -1) {
        cprintf("error parsing ip\n");
        exit();
    }
    if(ip_addr_pton("192.168.0.16", &addr2) == -1) {
        cprintf("error parsing ip\n");
        exit();
    }
    if (*pa == addr1){
        if(ethernet_addr_pton("e6:c8:ff:09:76:99", MAC) == -1) {
            cprintf("error parsing MAC\n");
            exit();
        }
        cprintf("\nSWISH\n");
        arp_table_insert(pa, MAC);
    }
    if (*pa == addr2){
        if(ethernet_addr_pton("e6:c8:ff:09:76:9c", MAC) == -1) {
            cprintf("error parsing MAC\n");
            exit();
        }
        cprintf("\nSWOOSH\n");
        arp_table_insert(pa, MAC);
    }

    entry = arp_table_select(pa);
    if (entry) {
        if (memcmp(entry->ha, ETHERNET_ADDR_ANY, ETHERNET_ADDR_LEN) == 0) {
            arp_send_request(netif, pa); /* just in case packet loss */
            cprintf("\nWOAHHHH\n");
            sleep(netif, &arplock); // Sleep while waiting for ARP reply
            memcpy(ha, entry->ha, ETHERNET_ADDR_LEN);
            release(&arplock);
            return ARP_RESOLVE_FOUND;
        }
        cprintf("\nWELL\n");
        memcpy(ha, entry->ha, ETHERNET_ADDR_LEN);
        release(&arplock);
        return ARP_RESOLVE_FOUND;
    }
    
    entry = arp_table_freespace();
    if (!entry) {
        release(&arplock);
        return ARP_RESOLVE_ERROR;
    }
    entry->used = 1;
    entry->pa = *pa;
    time(&entry->timestamp);
    entry->netif = netif;

    arp_send_request(netif, pa);
    // while (memcmp(entry->ha, ETHERNET_ADDR_ANY, ETHERNET_ADDR_LEN) == 0) {
    //     cprintf("\nWARIO\n");
    //     //sleep(netif, &arplock); // Sleep while waiting for ARP reply
    // }
    cprintf("\nWALUIGI\n");
    // After waking up
    memcpy(ha, entry->ha, ETHERNET_ADDR_LEN);
    release(&arplock);
    
    return ARP_RESOLVE_FOUND;
}


// int
// arp_resolve (struct netif *netif, const ip_addr_t *pa, uint8_t *ha, const void *data, size_t len) {
//     struct arp_entry *entry;
//     int ret;

//     acquire(&arplock);
//     entry = arp_table_select(pa);
//     if (entry) {
//         if (memcmp(entry->ha, ETHERNET_ADDR_ANY, ETHERNET_ADDR_LEN) == 0) {
//             arp_send_request(netif, pa); /* just in case packet loss */
//             release(&arplock);
//             return ARP_RESOLVE_QUERY;
//         }
//         memcpy(ha, entry->ha, ETHERNET_ADDR_LEN);
//         release(&arplock);
//         cprintf("FRESNO");
//         return ARP_RESOLVE_FOUND;
//     }

//     entry = arp_table_freespace();
//     if (!entry) {
//         release(&arplock);
//         return ARP_RESOLVE_ERROR;
//     }
//     entry->used = 1;
//     entry->pa = *pa;
//     time(&entry->timestamp);
//     entry->netif = netif;
//     arp_send_request(netif, pa);
//     cprintf("PLAIN\n");

//     release(&arplock);
    
//     //Polling until entry is resolved
//     while (1) {
//         acquire(&arplock);
//         entry = arp_table_select(pa);
//         if (entry){
//             cprintf("ANY\n");
//         } else {
//             cprintf("NOTFOUND\n");
//         }
//         if (entry && memcmp(entry->ha, ETHERNET_ADDR_ANY, ETHERNET_ADDR_LEN) != 0) {
//             memcpy(ha, entry->ha, ETHERNET_ADDR_LEN);
//             release(&arplock);
//             cprintf("DEALER");
//             return ARP_RESOLVE_FOUND;
//         }
//         release(&arplock);
//     }

//      return ARP_RESOLVE_QUERY;

//     // while (!entry){
//     //     sleep(netif, &arplock);
//     //     entry = arp_table_select(pa);
//     // }

//     // cprintf("DEALER\n");
    
//     // if (memcmp(entry->ha, ETHERNET_ADDR_ANY, ETHERNET_ADDR_LEN) == 0) {
//     //     arp_send_request(netif, pa); /* just in case packet loss */
//     //     release(&arplock);
//     //     return ARP_RESOLVE_QUERY;
//     // }
//     // memcpy(ha, entry->ha, ETHERNET_ADDR_LEN);
//     // release(&arplock);
//     // cprintf("FRESNO");
//     // return ARP_RESOLVE_FOUND;
   
// }

// /*
//     if (data) {
//         entry->data = (uint8_t*)kalloc();
//         if (!entry->data) {
//             release(&arplock);
//             return ARP_RESOLVE_ERROR;
//         }
//         memcpy(entry->data, data, len);
//         entry->len = len;
//     }
// */

int
arp_init (void) {
    struct arp_entry *entry;

    time(&timestamp);
    initlock(&arplock, "arp");
    netproto_register(NETPROTO_TYPE_ARP, arp_rx);
    return 0;
}
