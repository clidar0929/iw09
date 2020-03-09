// Copyright (c) 2012-2020 YAMAMOTO Masaya
// SPDX-License-Identifier: MIT

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "socket.h"

struct socket {
    int type;
    int desc;
};

struct file*
socketalloc(int domain, int type, int protocol) {
    struct file *f;
    struct socket *s;

    if (domain != AF_INET || (type != SOCK_STREAM && type != SOCK_DGRAM) || protocol != 0) {
        return NULL;
    }
    f = filealloc();
    if (!f) {
        return NULL;
    }
    s = (struct socket *)kalloc();
    if (!s) {
        fileclose(f);
        return NULL;
    }
    s->type = type;
    s->desc = (type == SOCK_STREAM ? tcp_api_open() : udp_api_open());
    f->type = FD_SOCKET;
    f->readable = 1;
    f->writable = 1;
    f->socket = s;
    return f;
}

void
socketclose(struct socket *s) {
    if (s->type == SOCK_STREAM)
        tcp_api_close(s->desc);
    else
        udp_api_close(s->desc);
}

int
socketconnect(struct socket *s, struct sockaddr *addr, int addrlen) {
    if (s->type != SOCK_STREAM)
      return -1;
    return tcp_api_connect(s->desc, addr, addrlen);
}

int
socketbind(struct socket *s, struct sockaddr *addr, int addrlen) {
    if (s->type == SOCK_STREAM)
        return tcp_api_bind(s->desc, addr, addrlen);
    else
        return udp_api_bind(s->desc, addr, addrlen);
}

int
socketlisten(struct socket *s, int backlog) {
    if (s->type != SOCK_STREAM)
        return -1;
    return tcp_api_listen(s->desc, backlog);
}

struct file *
socketaccept(struct socket *s, struct sockaddr *addr, int *addrlen) {
    int adesc;
    struct file *f;
    struct socket *as;
    if (s->type != SOCK_STREAM)
        return NULL;
    f = filealloc();
    if (!f) {
        return NULL;
    }
    as = (struct socket *)kalloc();
    if (!as) {
        fileclose(f);
        return NULL;
    }
    adesc = tcp_api_accept(s->desc, addr, addrlen);
    if (adesc == -1) {
        fileclose(f);
        kfree((void*)as);
        return NULL;
    }
    as->type = s->type;
    as->desc = adesc;
    f->type = FD_SOCKET;
    f->readable = 1;
    f->writable = 1;
    f->socket = as;
    return f;
}

int
socketread(struct socket *s, char *addr, int n) {
    if (s->type != SOCK_STREAM)
        return -1;
    return tcp_api_recv(s->desc, (uint8_t *)addr, n);
}

int
socketwrite(struct socket *s, char *addr, int n) {
    if (s->type != SOCK_STREAM)
        return -1;
    return tcp_api_send(s->desc, (uint8_t *)addr, n);
}

int
socketrecvfrom(struct socket *s, char *buf, int n, struct sockaddr *addr, int *addrlen) {
    if (s->type != SOCK_DGRAM)
        return -1;
    return udp_api_recvfrom(s->desc, (uint8_t *)buf, n, addr, addrlen);
}

int
socketsendto(struct socket *s, char *buf, int n, struct sockaddr *addr, int addrlen) {
    if (s->type != SOCK_DGRAM)
        return -1;
    return udp_api_sendto(s->desc, (uint8_t *)buf, n, addr, addrlen);
}
