struct pci_func;

// printfmt.c
void            vprintfmt(void (*)(int, void*), void*, const char*, va_list);
int             snprintf(char *buf, int n, const char *fmt, ...);

// string.c
int             strnlen(const char *s, size_t size);

// variable length arguments
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
