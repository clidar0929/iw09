#include "../types.h"
#include "../defs.h"

#include "../ext/types.h"
#include "../ext/defs.h"
#include "../ext/pci.h"

int
e1000_init(struct pci_func *pcif)
{
    cprintf("e1000_init\n");
    pci_func_enable(pcif);
    return 0;
}
