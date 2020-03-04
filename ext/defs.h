struct pci_func;

// pci.c
void            pciinit(void);
void            pci_func_enable(struct pci_func *f);

// printfmt.c
void            vprintfmt(void (*)(int, void*), void*, const char*, va_list);
int             snprintf(char *buf, int n, const char *fmt, ...);

// string.c
int             strnlen(const char *s, size_t size);

// assert
#define _str(x) #x
#define _tostr(x) _str(x)
#define _assert_occurs " [" __FILE__ ":" _tostr(__LINE__) "] "
#define assert(x) \
        do { if (!(x)) panic("assertion failed" _assert_occurs #x); } while (0)

// variable length arguments
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
