#include "types.h"

int
strnlen(const char *s, size_t size)
{
        int n;

        for (n = 0; size > 0 && *s != '\0'; s++, size--)
                n++;
        return n;
}
