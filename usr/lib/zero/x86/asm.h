#ifndef __ZERO_X86_ASM_H__
#define __ZERO_X86_ASM_H__

//#define frameisusr(tcb) ((tcb)->frame.cs == UTEXTSEL)

/* memory barrier */
#define m_membar()     __asm__ __volatile__ ("mfence\n" : : : "memory")
/* memory read barrier */
#define m_memrdbar()   __asm__ __volatile__ ("lfence\n" : : : "memory")
/* memory write barrier */
#define m_memwrbar()   __asm__ __volatile__ ("sfence\n" : : : "memory")
/* wait for an interrupt */
#define m_waitint()    __asm__ __volatile__ ("pause\n"  : : : "memory")
/* atomic fetch and add, 16-bit version */
#define m_fetchadd16(p, val)     m_xadd16(p, val)
#define m_fetchaddu16(p, val)    m_xaddu16(p, val)
/* atomic fetch and add, 32-bit version */
#define m_fetchadd32(p, val)     m_xadd32(p, val)
#define m_fetchaddu32(p, val)    m_xaddu32(p, val)
/* atomic compare and swap byte */
#define m_cmpswapb(p, want, val) m_cmpxchg8(p, want, val)

/* atomic increment operation */
static __inline__ void
m_atominc32(volatile int *p)
{
    __asm__ __volatile__ ("lock incl %0\n"
                          : "+m" (*(p))
                          :
                          : "memory");

    return;
}

/* atomic decrement operation */
static __inline__ void
m_atomdec32(volatile int *p)
{
    __asm__ __volatile__ ("lock decl %0\n"
                          : "+m" (*(p))
                          :
                          : "memory");

    return;
}

/* atomic exchange operation */
static __inline__ int
m_xchg32(volatile int *p,
         int val)
{
    int res;

    __asm__ __volatile__ ("lock xchgl %0, %2\n"
                          : "+m" (*p), "=a" (res)
                          : "r" (val)
                          : "cc", "memory");

    return res;
}

/*
 * atomic fetch and add
 * - let *p = *p + val
 * - return original *p
 */
static __inline__ int
m_xadd16(volatile short *p,
         short val)
{
    __asm__ __volatile__ ("lock xaddw %%ax, %2\n"
                          : "=a" (val)
                          : "a" (val), "m" (*(p))
                          : "memory");

    return val;
}

static __inline__ unsigned int
m_xaddu16(volatile unsigned short *p,
          unsigned short val)
{
    __asm__ __volatile__ ("lock xaddw %%ax, %2\n"
                          : "=a" (val)
                          : "a" (val), "m" (*(p))
                          : "memory");

    return val;
}

/*
 * atomic fetch and add
 * - let *p = *p + val
 * - return original *p
 */
static __inline__ int
m_xadd32(volatile int *p,
         int val)
{
    __asm__ __volatile__ ("lock xaddl %%eax, %2\n"
                          : "=a" (val)
                          : "a" (val), "m" (*(p))
                          : "memory");

    return val;
}

static __inline__ unsigned int
m_xaddu32(volatile unsigned int *p,
          unsigned int val)
{
    __asm__ __volatile__ ("lock xaddl %%eax, %2\n"
                          : "=a" (val)
                          : "a" (val), "m" (*(p))
                          : "memory");

    return val;
}

/*
 * atomic compare and exchange longword
 * - if *p == want, let *p = val
 * - return nonzero on success, zero on failure
 */
static __inline__ int
m_cmpxchg32(volatile int *p,
            int want,
            int val)
{
    int res;
    
    __asm__ __volatile__("lock cmpxchgl %1, %2\n"
                         : "=a" (res)
                         : "q" (val), "m" (*(p)), "0" (want)
                         : "memory");
    
    return (res == want);
}

/* atomic set bit operation */
static INLINE void
m_setbit(volatile int *p, int ndx)
{
    __asm__ __volatile__ ("lock btsq %1, %0\n"
                          : "=m" (*(p))
                          : "Ir" (ndx)
                          : "memory");

    return;
}

/* atomic reset/clear bit operation */
static INLINE void
m_clrbit(volatile int *p, int ndx)
{
    __asm__ __volatile__ ("btrb %1, %0\n"
                          : "=m" (*((uint8_t *)(p) + (ndx >> 3)))
                          : "Ir" (ndx));

    return;
}

static __inline__ void
m_unlkbit(volatile int *p, int ndx)
{
    m_membar();
    m_clrbit(p, ndx);

    return;
}

/* atomic flip/toggle bit operation */
static INLINE void
m_flipbit(volatile int *p, int ndx)
{
    __asm__ __volatile__ ("lock btcb %1, %0\n"
                          : "=m" (*((uint8_t *)(p) + (ndx >> 3)))
                          : "Ir" (ndx));

    return;
}

/* atomic set and test bit operation; returns the old value */
static __inline__ int
m_cmpsetbit32(volatile int *p, int ndx)
{
    int val = 0;

    __asm__ __volatile__ ("lock btsl %2, %0\n"
                          "jnc 1f\n"
                          "incl %1\n"
                          "1:\n"
                          : "=m" (*(p)), "=r" (val)
                          : "r" (ndx)
                          : "memory");

    return val;
}

/* atomic clear bit operation */
static __inline__ int
m_cmpclrbit32(volatile int *p, int ndx)
{
    int val = 0;

    __asm__ __volatile__ ("lock btrl %2, %0\n"
                          "jnc 1f\n"
                          "incl %1\n"
                          "1:\n"
                          : "=m" (*(p)), "=r" (val)
                          : "r" (ndx)
                          : "memory");

    return val;
}

#if defined(__GNUC__)
#define m_atomor(p, val)  __sync_or_and_fetch((p), (val))
#define m_atomand(p, val) __sync_and_and_fetch((p), (val))
#endif

/*
 * atomic compare and exchange byte
 * - if *p == want, let *p = val
 * - return original *p
 */
static __inline__ int
m_cmpxchg8(volatile int *p,
           int want,
           int val)
{
    int res;

    __asm__ __volatile__ ("lock cmpxchgb %b1, %2\n"
                          : "=a" (res)
                          : "q" (val), "m" (*(p)), "0" (want)
                          : "memory");

    return (res == want);
}

static __inline__ int
m_bsf32(unsigned int val)
{
    int ret = ~0;

    __asm__ __volatile__ ("bsfl %1, %0\n" : "=r" (ret) : "rm" (val));

    return ret;
}

static __inline__ int
m_bsr32(unsigned int val)
{
    int ret = ~0;

    __asm__ __volatile__ ("bsrl %1, %0\n" : "=r" (ret) : "rm" (val));

    return ret;
}

#endif /* __ZERO_X86_ASM_H__ */

