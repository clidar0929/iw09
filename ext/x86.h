static inline uint
inl(int port)
{
  uint data;
  asm volatile("inl %w1,%0" : "=a" (data) : "d" (port));
  return data;
}

static inline void
outl(int port, uint data)
{
  asm volatile("outl %0,%w1" : : "a" (data), "d" (port));
}
