struct pci_func;

// e1000.c
int             e1000_init(struct pci_func *pcif);
void            e1000intr(void);
