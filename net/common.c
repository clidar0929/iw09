#include "../types.h"
#include "../defs.h"

#include "../ext/types.h"
#include "../net/types.h"

#define isascii(x) ((x >= 0x00) && (x <= 0x7f))
#define isprint(x) ((x >= 0x20) && (x <= 0x7e))

void
hexdump(void *data, size_t size)
{
    int offset, index;
    unsigned char *src;

    src = (unsigned char *)data;
    cprintf("+------+-------------------------------------------------+------------------+\n");
    for (offset = 0; offset < (int)size; offset += 16) {
        cprintf("| %04x | ", offset);
        for (index = 0; index < 16; index++) {
            if (offset + index < (int)size) {
                cprintf("%02x ", 0xff & src[offset + index]);
            } else {
                cprintf("   ");
            }
        }
        cprintf("| ");
        for (index = 0; index < 16; index++) {
            if (offset + index < (int)size) {
                if (isascii(src[offset + index]) && isprint(src[offset + index])) {
                    cprintf("%c", src[offset + index]);
                } else {
                    cprintf(".");
                }
            } else {
                cprintf(" ");
            }
        }
        cprintf(" |\n");
    }
    cprintf("+------+-------------------------------------------------+------------------+\n");
}

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif

static int endian;

static int
byteorder (void)
{
    uint32_t x = 0x00000001;
    return *(uint8_t *)&x ? __LITTLE_ENDIAN : __BIG_ENDIAN;
}

static uint16_t
byteswap16(uint16_t v)
{
    return (v & 0x00ff) << 8 | (v & 0xff00 ) >> 8;
}

static uint32_t
byteswap32(uint32_t v)
{
    return (v & 0x000000ff) << 24 | (v & 0x0000ff00) << 8 | (v & 0x00ff0000) >> 8 | (v & 0xff000000) >> 24;
}

uint16_t
hton16 (uint16_t h)
{
    if (!endian)
        endian = byteorder();
    return endian == __LITTLE_ENDIAN ? byteswap16(h) : h;
}

uint16_t
ntoh16 (uint16_t n)
{
    if (!endian)
        endian = byteorder();
    return endian == __LITTLE_ENDIAN ? byteswap16(n) : n;
}

uint32_t
hton32(uint32_t h)
{
    if (!endian)
        endian = byteorder();
    return endian == __LITTLE_ENDIAN ? byteswap32(h) : h;
}

uint32_t
ntoh32(uint32_t n)
{
    if (!endian)
        endian = byteorder();
    return endian == __LITTLE_ENDIAN ? byteswap32(n) : n;
}

uint16_t
cksum16 (uint16_t *data, uint16_t size, uint32_t init) {
    uint32_t sum;

    sum = init;
    while(size > 1) {
        sum += *(data++);
        size -= 2;
    }
    if(size) {
        sum += *(uint8_t *)data;
    }
    sum  = (sum & 0xffff) + (sum >> 16);
    sum  = (sum & 0xffff) + (sum >> 16);
    return ~(uint16_t)sum;
}

time_t
time(time_t *t)
{
    acquire(&tickslock);
    *t = ticks / 100;
    release(&tickslock);
    return *t;
}
