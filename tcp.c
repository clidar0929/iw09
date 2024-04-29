// Copyright (c) 2012-2020 YAMAMOTO Masaya
// SPDX-License-Identifier: MIT

#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "common.h"
#include "net.h"
#include "ip.h"
#include "socket.h"

#define TCP_CB_TABLE_SIZE 16
#define TCP_SOURCE_PORT_MIN 49152
#define TCP_SOURCE_PORT_MAX 65535

#define TCP_CB_STATE_CLOSED      0
#define TCP_CB_STATE_LISTEN      1
#define TCP_CB_STATE_SYN_SENT    2
#define TCP_CB_STATE_SYN_RCVD    3
#define TCP_CB_STATE_ESTABLISHED 4
#define TCP_CB_STATE_FIN_WAIT1   5
#define TCP_CB_STATE_FIN_WAIT2   6
#define TCP_CB_STATE_CLOSING     7
#define TCP_CB_STATE_TIME_WAIT   8
#define TCP_CB_STATE_CLOSE_WAIT  9
#define TCP_CB_STATE_LAST_ACK    10

#define TCP_FLG_FIN 0x01
#define TCP_FLG_SYN 0x02
#define TCP_FLG_RST 0x04
#define TCP_FLG_PSH 0x08
#define TCP_FLG_ACK 0x10
#define TCP_FLG_URG 0x20

//Encryption Negotiation Constants
#define INIT_MAGIC 0x15101a0e

//Diffie-Hellman Constants
#define PRIME 23
#define GENERATOR 5

#define TCP_FLG_IS(x, y) ((x & 0x3f) == (y))
#define TCP_FLG_ISSET(x, y) ((x & 0x3f) & (y))

static uint32_t private_key = 0;
static uint32_t shared_key = 0;

struct tcp_hdr {
    uint16_t src;
    uint16_t dst;
    uint32_t seq;
    uint32_t ack;
    uint8_t  off;
    uint8_t  flg;
    uint16_t win;
    uint16_t sum;
    uint16_t urg;
    uint8_t opt;
    uint8_t size;
    uint8_t tep;
};

struct tcp_txq_entry {
    struct tcp_hdr *segment;
    uint16_t len;
    //struct timeval timestamp;
    struct tcp_txq_entry *next;
};

struct tcp_txq_head {
    struct tcp_txq_entry *head;
    struct tcp_txq_entry *tail;
};

struct tcp_cb {
    uint8_t used;
    uint8_t state;
    struct netif *iface;
    uint16_t port;
    struct {
        ip_addr_t addr;
        uint16_t port;
    } peer;
    struct {
        uint32_t nxt;
        uint32_t una;
        uint16_t up;
        uint32_t wl1;
        uint32_t wl2;
        uint16_t wnd;
    } snd;
    uint32_t iss;
    struct {
        uint32_t nxt;
        uint16_t up;
        uint16_t wnd;
    } rcv;
    uint32_t irs;
    struct tcp_txq_head txq;
    uint8_t window[4096];
    struct tcp_cb *parent;
    struct queue_head backlog;
};

#define TCP_CB_LISTENER_SIZE 128

#define TCP_CB_STATE_RX_ISREADY(x) (x->state == TCP_CB_STATE_ESTABLISHED || x->state == TCP_CB_STATE_FIN_WAIT1 || x->state == TCP_CB_STATE_FIN_WAIT2)
#define TCP_CB_STATE_TX_ISREADY(x) (x->state == TCP_CB_STATE_ESTABLISHED || x->state == TCP_CB_STATE_CLOSE_WAIT)

#define TCP_SOCKET_ISINVALID(x) (x < 0 || x >= TCP_CB_TABLE_SIZE)

//static pthread_t timer_thread;
static struct spinlock tcplock;
struct tcp_cb cb_table[TCP_CB_TABLE_SIZE];

// Function to calculate modular exponentiation (base^exp % modulus)
uint32_t mod_exp(uint32_t base, uint32_t exp, uint32_t modulus) {
    uint32_t result = 1;
    base = base % modulus;
    while (exp > 0) {
        if (exp & 1) {
            result = (result * base) % modulus;
        }
        exp = exp >> 1;
        base = (base * base) % modulus;
    }
    return result;
}

// Function to generate initial public key
uint32_t get_public_key() {
    // Generate a random private key
    private_key = (uint32_t)random() % (PRIME - 1) + 1; 

    // Calculate public key: public_key = GENERATOR^private_key % PRIME
    uint32_t public_key = mod_exp(GENERATOR, private_key, PRIME);

    return public_key;
}

uint32_t prng_helper (uint32_t seed) {
    // Define parameters for the LCG algorithm
    uint32_t a = 1664525;
    uint32_t c = 1013904223;
    int m = 4294967295; // 2^32

    return (a * seed + c) % m;
}

void encdec(uint8_t *buf, size_t len) {
    // Shared key from Diffie-Hellman key exchange
    size_t i;
    
    for (i = 0; i < len; ++i) {
        // Perform XOR operation with the next byte of the key
        buf[i] ^= (uint8_t)shared_key;
        // Update the key using a share pseudorandom number generator. 
        shared_key = prng_helper(shared_key)
    }

}

static int
tcp_txq_add (struct tcp_cb *cb, struct tcp_hdr *hdr, size_t len) {
    struct tcp_txq_entry *txq;

    txq = (struct tcp_txq_entry *)kalloc();
    if (!txq) {
        return -1;
    }
    txq->segment = (struct tcp_hdr *)kalloc();
    if (!txq->segment) {
        kfree((char*)txq);
        return -1;
    }
    memcpy(txq->segment, hdr, len);
    txq->len = len;
    //gettimeofday(&txq->timestamp, NULL);
    txq->next = NULL;

    // set txq to next of tail entry
    if (cb->txq.head == NULL) {
        cb->txq.head = txq;
    } else {
        cb->txq.tail->next = txq;
    }
    // update tail entry
    cb->txq.tail = txq;

    return 0;
}

static int
tcp_cb_clear (struct tcp_cb *cb) {
    struct tcp_txq_entry *txq;
    struct queue_entry *entry;
    struct tcp_cb *backlog;

    while (cb->txq.head) {
        txq = cb->txq.head;
        cb->txq.head = txq->next;
        kfree((char*)txq->segment);
        kfree((char*)txq);
    }
    while (1) {
        entry = queue_pop(&cb->backlog);
        if (!entry) {
            break;
        }
        backlog = entry->data;
        kfree((char*)entry);
        tcp_cb_clear(backlog);
    }
    memset(cb, 0, sizeof(*cb));
    return 0;
}

static ssize_t
tcp_tx (struct tcp_cb *cb, uint32_t seq, uint32_t ack, uint8_t flg, uint8_t *buf, size_t len) {
    uint8_t segment[1500];
    struct tcp_hdr *hdr;
    ip_addr_t self, peer;
    uint32_t pseudo = 0;

    memset(&segment, 0, sizeof(segment));
    hdr = (struct tcp_hdr *)segment;
    hdr->src = cb->port;
    hdr->dst = cb->peer.port;
    hdr->seq = hton32(seq);
    hdr->ack = hton32(ack);
    hdr->off = (sizeof(struct tcp_hdr) >> 2) << 4;
    hdr->flg = flg;
    hdr->win = hton16(cb->rcv.wnd);
    hdr->sum = 0;
    hdr->urg = 0;

    if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_SYN)){
        hdr->opt = 69;
        hdr->size = 3;
        hdr->tep = 0x99;
    }

    memcpy(hdr + 1, buf, len);
    self = ((struct netif_ip *)cb->iface)->unicast;
    peer = cb->peer.addr;
    pseudo += (self >> 16) & 0xffff;
    pseudo += self & 0xffff;
    pseudo += (peer >> 16) & 0xffff;
    pseudo += peer & 0xffff;
    pseudo += hton16((uint16_t)IP_PROTOCOL_TCP);
    pseudo += hton16(sizeof(struct tcp_hdr) + len);
    hdr->sum = cksum16((uint16_t *)hdr, sizeof(struct tcp_hdr) + len, pseudo);
    hexdump(&peer, sizeof(ip_addr_t));
    ip_tx(cb->iface, IP_PROTOCOL_TCP, (uint8_t *)hdr, sizeof(struct tcp_hdr) + len, &peer);
    tcp_txq_add(cb, hdr, sizeof(struct tcp_hdr) + len);
    return len;
}

static void
tcp_incoming_event (struct tcp_cb *cb, struct tcp_hdr *hdr, size_t len) {
    uint32_t seq, ack;
    size_t hlen, plen;
    uint8_t init[8] = {0};

    hlen = ((hdr->off >> 4) << 2);
    plen = len - hlen;
    switch (cb->state) {
        case TCP_CB_STATE_CLOSED:
            if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_RST)) {
                return;
            }
            if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_ACK)) {
                seq = ntoh32(hdr->ack);
                ack = 0;
            } else {
                seq = 0;
                ack = ntoh32(hdr->seq);
                if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_SYN)) {
                    ack++;
                }
                if (plen) {
                    ack += plen;
                }
                if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_FIN)) {
                    ack++;
                }
            }
            tcp_tx(cb, seq, ack, TCP_FLG_RST, NULL, 0);
            return;
        case TCP_CB_STATE_LISTEN:
            if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_RST)) {
                return;
            }
            if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_ACK)) {
                seq = ntoh32(hdr->ack);
                ack = 0;
                tcp_tx(cb, seq, ack, TCP_FLG_RST, NULL, 0);
                return;
            }
            if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_SYN)) { 
                if (hdr->opt != 69){ 
                    cprintf("SYN without TCP-ENO");
                    seq = ntoh32(hdr->ack);
                    ack = 0;
                    tcp_tx(cb, seq, ack, TCP_FLG_RST, NULL, 0);
                    return;
                }
                cb->rcv.nxt = ntoh32(hdr->seq) + 1;
                cb->irs = ntoh32(hdr->seq);
                cb->iss = (uint32_t)random();
                seq = cb->iss;
                ack = cb->rcv.nxt;
                tcp_tx(cb, seq, ack, TCP_FLG_SYN | TCP_FLG_ACK, NULL, 0);
                cb->snd.nxt = cb->iss + 1;
                cb->snd.una = cb->iss;
                cb->state = TCP_CB_STATE_SYN_RCVD;
            }
            return;
        case TCP_CB_STATE_SYN_SENT:
            if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_ACK)) {
                if (ntoh32(hdr->ack) <= cb->iss || ntoh32(hdr->ack) > cb->snd.nxt) {
                    if (!TCP_FLG_ISSET(hdr->flg, TCP_FLG_RST)) {
                        seq = ntoh32(hdr->ack);
                        ack = 0;
                        tcp_tx(cb, seq, ack, TCP_FLG_RST, NULL, 0);
                    }
                    return;
                }
            }
            if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_RST)) {
                if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_ACK)) {
                    // TCB close
                }
                return;
            }
            if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_SYN)) {
                cb->rcv.nxt = ntoh32(hdr->seq) + 1;
                cb->irs = ntoh32(hdr->seq);
                if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_ACK)) {
                    cb->snd.una = ntoh32(hdr->ack);
                    // delete TX queue
                    if (cb->snd.una > cb->iss) {
                        cb->state = TCP_CB_STATE_ESTABLISHED;
                        seq = cb->snd.nxt;
                        ack = cb->rcv.nxt;
                        *((uint32_t*)init) = INIT_MAGIC;
                        *((uint32_t*)(init + 4)) = get_public_key();
                        tcp_tx(cb, seq, ack, TCP_FLG_ACK, init, 0);
                        wakeup(cb);
                    }
                    return;
                }
                seq = cb->iss;
                ack = cb->rcv.nxt;
                tcp_tx(cb, seq, ack, TCP_FLG_ACK, NULL, 0);
            }
            return;
        default:
            break;
    }
    if (ntoh32(hdr->seq) != cb->rcv.nxt) {
        // TODO
        return;
    }
    if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_RST | TCP_FLG_SYN)) {
        // TODO
        return;
    }
    if (!TCP_FLG_ISSET(hdr->flg, TCP_FLG_ACK)) {
        // TODO
        return;
    }
    switch (cb->state) {
        case TCP_CB_STATE_SYN_RCVD:
            if (cb->snd.una <= ntoh32(hdr->ack) && ntoh32(hdr->ack) <= cb->snd.nxt) {
                cb->state = TCP_CB_STATE_ESTABLISHED;
                queue_push(&cb->parent->backlog, cb, sizeof(*cb));
                wakeup(cb->parent);
            } else {
                tcp_tx(cb, ntoh32(hdr->ack), 0, TCP_FLG_RST, NULL, 0);
                break;
            }
        case TCP_CB_STATE_ESTABLISHED:
        case TCP_CB_STATE_FIN_WAIT1:
        case TCP_CB_STATE_FIN_WAIT2:
        case TCP_CB_STATE_CLOSE_WAIT:
        case TCP_CB_STATE_CLOSING:
            if (cb->snd.una < ntoh32(hdr->ack) && ntoh32(hdr->ack) <= cb->snd.nxt) {
                cb->snd.una = ntoh32(hdr->ack);
            } else if (ntoh32(hdr->ack) > cb->snd.nxt) {
                tcp_tx(cb, cb->snd.nxt, cb->rcv.nxt, TCP_FLG_ACK, NULL, 0);
                return;
            }
            // send window update
            if (cb->state == TCP_CB_STATE_FIN_WAIT1) {
                if (ntoh32(hdr->ack) == cb->snd.nxt) {
                    cb->state = TCP_CB_STATE_FIN_WAIT2;
                }
            } else if (cb->state == TCP_CB_STATE_CLOSING) {
                if (ntoh32(hdr->ack) == cb->snd.nxt) {
                    cb->state = TCP_CB_STATE_TIME_WAIT;
                    wakeup(cb);
                }
                return;
            }
            break;
        case TCP_CB_STATE_LAST_ACK:
            wakeup(cb);
            tcp_cb_clear(cb); /* TCP_CB_STATE_CLOSED */
            return;
    }
    if (plen) {
        switch (cb->state) {
            case TCP_CB_STATE_ESTABLISHED:
            case TCP_CB_STATE_FIN_WAIT1:
            case TCP_CB_STATE_FIN_WAIT2:
                if (private_key && !shared_key) {
                    if (*((uint32_t*)((uint8_t *)hdr + hlen)) != INIT_MAGIC){
                        tcp_tx(cb, ntoh32(hdr->ack), 0, TCP_FLG_RST, NULL, 0);
                        break
                    }
                    shared_key = mod_exp(*((uint32_t*)((uint8_t *)hdr + hlen + sizeof(uint32_t))), private_key, PRIME);
                }
                memcpy(cb->window + (sizeof(cb->window) - cb->rcv.wnd), (uint8_t *)hdr + hlen, plen);
                cb->rcv.nxt = ntoh32(hdr->seq) + plen;
                cb->rcv.wnd -= plen;
                seq = cb->snd.nxt;
                ack = cb->rcv.nxt;
                tcp_tx(cb, seq, ack, TCP_FLG_ACK, NULL, 0);
                wakeup(cb);
                break;
            default:
                break;
        }
    }
    if (TCP_FLG_ISSET(hdr->flg, TCP_FLG_FIN)) {
        cb->rcv.nxt++;
        tcp_tx(cb, cb->snd.nxt, cb->rcv.nxt, TCP_FLG_ACK, NULL, 0);
        switch (cb->state) {
            case TCP_CB_STATE_SYN_RCVD:
            case TCP_CB_STATE_ESTABLISHED:
                cb->state = TCP_CB_STATE_CLOSE_WAIT;
                wakeup(cb);
                break;
            case TCP_CB_STATE_FIN_WAIT1:
                cb->state = TCP_CB_STATE_FIN_WAIT2;
                break;
            case TCP_CB_STATE_FIN_WAIT2:
                cb->state = TCP_CB_STATE_TIME_WAIT;
                wakeup(cb);
                break;
            default:
                break;
        }
        return;
    }
    return;
}

static void
tcp_rx (uint8_t *segment, size_t len, ip_addr_t *src, ip_addr_t *dst, struct netif *iface) {
    struct tcp_hdr *hdr;
    uint32_t pseudo = 0;
    struct tcp_cb *cb, *fcb = NULL, *lcb = NULL;
    if (*dst != ((struct netif_ip *)iface)->unicast) {
        return;
    }
    if (len < sizeof(struct tcp_hdr)) {
        return;
    }
    hdr = (struct tcp_hdr *)segment;
    pseudo += *src >> 16;
    pseudo += *src & 0xffff;
    pseudo += *dst >> 16;
    pseudo += *dst & 0xffff;
    pseudo += hton16((uint16_t)IP_PROTOCOL_TCP);
    pseudo += hton16(len);
    if (cksum16((uint16_t *)hdr, len, pseudo) != 0) {
        cprintf("tcp checksum error!\n");
        //return;
    }
    acquire(&tcplock);
    for (cb = cb_table; cb < array_tailof(cb_table); cb++) {
        if (!cb->used) {
            if (!fcb) {
                fcb = cb;
            }
        }
        else if ((!cb->iface || cb->iface == iface) && cb->port == hdr->dst) {
            if (cb->peer.addr == *src && cb->peer.port == hdr->src) {
                if (TCP_FLG_IS(hdr->flg, TCP_FLG_SYN | TCP_FLG_ACK)){
                    cb->used = 1;
                    cb->iface = iface;
                }
                break;
            }
            if (cb->state == TCP_CB_STATE_LISTEN && !lcb) {
                lcb = cb;
            }
        }
    }
    if (cb == array_tailof(cb_table)) {
        if (!lcb || !fcb || !TCP_FLG_IS(hdr->flg, TCP_FLG_SYN)) {
            tcp_tx(cb, ntoh32(hdr->ack), 0, TCP_FLG_RST, NULL, 0);
            release(&tcplock);
            return;
        }
        cb = fcb;
        cb->used = 1;
        cb->state = lcb->state;
        cb->iface = iface;
        cb->port = lcb->port;
        cb->peer.addr = *src;
        cb->peer.port = hdr->src;
        cb->rcv.wnd = sizeof(cb->window);
        cb->parent = lcb;
    }
 
    tcp_incoming_event(cb, hdr, len);
    release(&tcplock);
    return;
}

int
tcp_api_open (void) {
    struct tcp_cb *cb;

    acquire(&tcplock);
    for (cb = cb_table; cb < array_tailof(cb_table); cb++) {
        if (!cb->used) {
            cb->used = 1;
            release(&tcplock);
            return array_offset(cb_table, cb);
        }
    }
    release(&tcplock);
    return -1;
}

int
tcp_api_close (int soc) {
    struct tcp_cb *cb;

    if (TCP_SOCKET_ISINVALID(soc)) {
        return -1;
    }
    acquire(&tcplock);
    cb = &cb_table[soc];
    if (!cb->used) {
        release(&tcplock);
        return -1;
    }
    switch (cb->state) {
        case TCP_CB_STATE_SYN_RCVD:
        case TCP_CB_STATE_ESTABLISHED:
            tcp_tx(cb, cb->snd.nxt, cb->rcv.nxt, TCP_FLG_FIN | TCP_FLG_ACK, NULL, 0);
            cb->state = TCP_CB_STATE_FIN_WAIT1;
            cb->snd.nxt++;
            sleep(cb, &tcplock);
            break;
        case TCP_CB_STATE_CLOSE_WAIT:
            tcp_tx(cb, cb->snd.nxt, cb->rcv.nxt, TCP_FLG_FIN | TCP_FLG_ACK, NULL, 0);
            cb->state = TCP_CB_STATE_LAST_ACK;
            cb->snd.nxt++;
            sleep(cb, &tcplock);
            break;
        default:
            break;
    }
    tcp_cb_clear(cb); /* TCP_CB_STATE_CLOSED */
    release(&tcplock);
    return 0;
}

int
tcp_api_connect (int soc, struct sockaddr *addr, int addrlen) {
    struct sockaddr_in *sin;
    struct tcp_cb *cb, *tmp;
    uint32_t p;

    if (TCP_SOCKET_ISINVALID(soc)) {
        return -1;
    }
    if (addr->sa_family != AF_INET) {
        return -1;
    }
    sin = (struct sockaddr_in *)addr;
    acquire(&tcplock);
    cb = &cb_table[soc];
    if (!cb->used || cb->state != TCP_CB_STATE_CLOSED) {
        release(&tcplock);
        return -1;
    }
    if (!cb->port) {
        int offset = time(NULL) % 1024;
        for (p = TCP_SOURCE_PORT_MIN + offset; p <= TCP_SOURCE_PORT_MAX; p++) {
            for (tmp = cb_table; tmp < array_tailof(cb_table); tmp++) {
                if (tmp->used && tmp->port == hton16((uint16_t)p)) {
                    break;
                }
            }
            if (tmp == array_tailof(cb_table)) {
                cb->port = hton16((uint16_t)p);
                break;
            }
        }
        if (!cb->port) {
            release(&tcplock);
            return -1;
        }
    }
    cb->peer.addr = sin->sin_addr;
    cb->peer.port = sin->sin_port;
    cb->rcv.wnd = sizeof(cb->window);
    cb->iss = (uint32_t)random();
    tcp_tx(cb, cb->iss, 0, TCP_FLG_SYN, NULL, 0);
    cb->snd.nxt = cb->iss + 1;
    cb->state = TCP_CB_STATE_SYN_SENT;
    while (cb->state == TCP_CB_STATE_SYN_SENT) {
        sleep(&cb_table[soc], &tcplock);
    }

    release(&tcplock);
    return 0;
}

int
tcp_api_bind (int soc, struct sockaddr *addr, int addrlen) {
    struct sockaddr_in *sin;
    struct tcp_cb *cb;

    if (TCP_SOCKET_ISINVALID(soc)) {
        return -1;
    }
    if (addr->sa_family != AF_INET) {
        return -1;
    }
    sin = (struct sockaddr_in *)addr;
    acquire(&tcplock);
    for (cb = cb_table; cb < array_tailof(cb_table); cb++) {
        if (cb->port == sin->sin_port) {
            release(&tcplock);
            return -1;
        }
    }
    cb = &cb_table[soc];
    if (!cb->used || cb->state != TCP_CB_STATE_CLOSED) {
        release(&tcplock);
        return -1;
    }
    cb->port = sin->sin_port;
    release(&tcplock);
    return 0;
}

int
tcp_api_listen (int soc, int backlog) {
    struct tcp_cb *cb;

    if (TCP_SOCKET_ISINVALID(soc)) {
        return -1;
    }
    acquire(&tcplock);
    cb = &cb_table[soc];
    if (!cb->used || cb->state != TCP_CB_STATE_CLOSED || !cb->port) {
        release(&tcplock);
        return -1;
    }
    cb->state = TCP_CB_STATE_LISTEN;
    release(&tcplock);
    return 0;
}

int
tcp_api_accept (int soc, struct sockaddr *addr, int *addrlen) {
    struct tcp_cb *cb, *backlog;
    struct queue_entry *entry;
    struct sockaddr_in *sin = NULL;

    if (TCP_SOCKET_ISINVALID(soc)) {
        return -1;
    }
    if (addr) {
        if (!addrlen) {
            return -1;
        }
        if (*addrlen < sizeof(struct sockaddr_in)) {
            return -1;
        }
        *addrlen = sizeof(struct sockaddr_in);
        sin = (struct sockaddr_in *)addr;
    }
    acquire(&tcplock);
    cb = &cb_table[soc];
    if (!cb->used) {
        release(&tcplock);
        return -1;
    }
    if (cb->state != TCP_CB_STATE_LISTEN) {
        release(&tcplock);
        return -1;
    }
    while ((entry = queue_pop(&cb->backlog)) == NULL) {
        sleep(cb, &tcplock);
    }
    backlog = entry->data;
    kfree((char*)entry);
    if (sin) {
      sin->sin_family = AF_INET;
      sin->sin_addr = backlog->peer.addr;
      sin->sin_port = backlog->peer.port;
    }
    release(&tcplock);
    return array_offset(cb_table, backlog);
}

ssize_t
tcp_api_recv (int soc, uint8_t *buf, size_t size) {
    struct tcp_cb *cb;
    size_t total, len;

    if (TCP_SOCKET_ISINVALID(soc)) {
        return -1;
    }
    acquire(&tcplock);
    cb = &cb_table[soc];
    if (!cb->used) {
        release(&tcplock);
        return -1;
    }
    while (!(total = sizeof(cb->window) - cb->rcv.wnd)) {
        if (!TCP_CB_STATE_RX_ISREADY(cb)) {
            release(&tcplock);
            return 0;
        }
        sleep(cb, &tcplock);
    }
    len = size < total ? size : total;
    memcpy(buf, cb->window, len);
    encdec(buf, len);
    memmove(cb->window, cb->window + len, total - len);
    cb->rcv.wnd += len;
    release(&tcplock);
    return len;
}

ssize_t
tcp_api_send (int soc, uint8_t *buf, size_t len) {
    struct tcp_cb *cb;

    if (TCP_SOCKET_ISINVALID(soc)) {
        return -1;
    }
    acquire(&tcplock);
    cb = &cb_table[soc];
    if (!cb->used) {
        release(&tcplock);
        return -1;
    }
    if (!TCP_CB_STATE_TX_ISREADY(cb)) {
        release(&tcplock);
        return -1;
    }

    encdec(buf, len);

    tcp_tx(cb, cb->snd.nxt, cb->rcv.nxt, TCP_FLG_ACK | TCP_FLG_PSH, buf, len);
    cb->snd.nxt += len;
    release(&tcplock);
    return 0;
}

int
tcp_init (void) {
    struct tcp_cb *cb;

    initlock(&tcplock, "tcplock");
    ip_add_protocol(IP_PROTOCOL_TCP, tcp_rx);
    return 0;
}