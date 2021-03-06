#ifndef __KERN_MEM_MEM_H__
#define __KERN_MEM_MEM_H__

//#define MEMPARANOIA 1
#undef MEMPARANOIA

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <zero/param.h>
#include <zero/trix.h>
#include <kern/mem/pool.h>
#include <kern/mem/slab.h>

/* memory caching type */
#define MEMWRBIT     0x01
#define MEMCACHEBIT  0x02
#define MEMWRBUFBIT  0x04
#define MEMWRPROT    0x00 // write-protectected
/* uncacheable */
#define MEMNOCACHE   (MEMWRBIT)
/* write-combining, uncached */
#define MEMWRCOMB    (MEMWRBIT | MEMIOBUFBIT)
/* write-through */
#define MEMWRTHRU    (MEMWRBIT | MEMCACHEBIT)
/* write-back */
#define MEMWRBACK    (MEMWRBIT | MEMCACHEBIT | MEMWRBUFBIT)

/* allocator parameters */
#define MEMNHDRHASH    (512 * 1024)     // # of entries in header hash table
//#define MEMNHDRBUF     (roundup(__STRUCT_MEMBLK_SIZE, CLSIZE))
#define MEMMINSIZE     (1U << MEMMINSHIFT)
#define MEMSLABSIZE    (1U << MEMSLABSHIFT)
#define MEMMINSHIFT    8 // minimum allocation of 256 bytes
#define MEMSLABSHIFT   16
#define MEMNBKTBIT     8 // 8 low bits of info used for bucket ID (max 255)
#define MEMNTYPEBIT    8 // up to 256 object types
#define MEMBKTMASK     ((1UL << (MEMNBKTBIT - 1)) - 1)
/* allocation flags */
#define MEMFREE        0x00000001UL
#define MEMZERO        0x00000002UL
#define MEMWIRE        0x00000004UL
#define MEMDIRTY       0x00000008UL
#define MEMFLGBITS     (MEMFREE | MEMZERO | MEMWIRE | MEMDIRTY)
#define MEMNFLGBIT     4
#define memslabsize(bkt)                                                \
    (1UL << (bkt))
#define memtrylkhdr(hdr)                                                \
    (!m_cmpsetbit(&hdr->info, MEMLOCKBIT))
#define memlkhdr(hdr)                                                   \
    do {                                                                \
        volatile long res;                                              \
                                                                        \
        do {                                                            \
            res =  !m_cmpsetbit(&hdr->info, MEMLOCKBIT);                \
        } while (!res);                                                 \
    } while (0)
#define memunlkhdr(hdr)                                                 \
    (m_unlkbit(&(hdr)->info, MEMLOCKBIT))
#define memgetbkt(hdr)                                                  \
    (&(hdr)->info & ((1 << MEMNBKTBIT) - 1))

void meminit(size_t nbphys, size_t nbvirt);
void meminitphys(struct mempool *pool, uintptr_t base, size_t nbyte);
void meminitvirt(struct mempool *pool, size_t nbvirt);
void memfree(struct mempool *pool, void *ptr);

#endif /* __KERN_MEM_MEM_H__ */

