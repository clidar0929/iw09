struct stat;
struct rtcdate;
struct sockaddr;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);

int ioctl(int, int, ...);
int socket(int, int, int);
int connect(int, struct sockaddr*, int);
int bind(int, struct sockaddr*, int);
int listen(int, int);
int accept(int, struct sockaddr*, int*);
int recv(int, char*, int);
int send(int, char*, int);
int recvfrom(int, char*, int, struct sockaddr*, int*);
int sendto(int, char*, int, struct sockaddr*, int);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);

// additional functions
void hexdump(void *data, size_t size);
uint16_t hton16(uint16_t h);
uint16_t ntoh16(uint16_t n);
uint32_t hton32(uint32_t h);
uint32_t ntoh32(uint32_t n);
long strtol(const char *s, char **endptr, int base);
int ip_addr_pton(const char *p, ip_addr_t *n);

#define IP_ADDR_LEN 4
#define IP_ADDR_STR_LEN 16 /* "ddd.ddd.ddd.ddd\0" */
