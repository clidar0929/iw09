#include "../types.h"
#include "../defs.h"

#include "../ext/types.h"

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
