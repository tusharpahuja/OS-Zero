/*
 * Zero Malloc Revision 2
 *
 * - an unstable alpha version of a new malloc for Zero.
 *
 * Copyright (C) Tuomo Petteri Ven�l�inen 2014-2015
 */

/*
 * TODO
 * ----
 * - fix mallinfo() to return proper information
 */

#define MALLOCNARN       8
#define MALLOCNEWSLABS   1
#define MALLOCEXPERIMENT 0

#define MALLOCDEBUGHOOKS 0
#define MALLOCSTEALMAG   0
#define MALLOCNEWHACKS   1

#define MALLOCNOSBRK     0
#define MALLOCDIAG       0
#define MALLOCFREEMDIR   0  // under construction
#define MALLOCSTKNDX     0
#define MALLOCFREEMAP    0  // use free block bitmaps
#define MALLOCHACKS      0  // enable experimental features
#define MALLOCBUFMAP     1  // buffer mapped slabs to global pool
#define MALLOCVARSIZEBUF 1  // use variable-size slabs; FIXME
#define MALLOCHASH       0
#define MALLOCNHASHBIT   24
#define MALLOCNHASH      (1UL << MALLOCNHASHBIT)

/*
 * THANKS
 * ------
 * - Matthew 'kinetik' Gregan for pointing out bugs, giving me cool routines to
 *   find more of them, and all the constructive criticism etc.
 * - Thomas 'Freaky' Hurst for patience with early crashes, 64-bit hints, and
 *   helping me find some bottlenecks.
 * - Henry 'froggey' Harrington for helping me fix issues on AMD64.
 * - Dale 'swishy' Anderson for the enthusiasm, encouragement, and everything
 *   else.
 * - Martin 'bluet' Stensg�rd for an account on an AMD64 system for testing
 *   earlier versions.
 */

/*
 *        malloc buffer layers
 *        --------------------
 *
 *                --------
 *                | mag  |----------------
 *                --------               |
 *                    |                  |
 *                --------               |
 *                | slab |               |
 *                --------               |
 *        --------  |  |   -------  -----------
 *        | heap |--|  |---| map |--| headers |
 *        --------         -------  -----------
 *
 *        mag
 *        ---
 *        - magazine cache with allocation stack of pointers into the slab
 *          - LIFO to reuse freed blocks of virtual memory
 *
 *        slab
 *        ----
 *        - slab allocator bottom layer
 *        - power-of-two size slab allocations
 *          - supports both heap (sbrk()) and mapped (mmap()) regions
 *
 *        heap
 *        ----
 *        - process heap segment
 *          - sbrk() interface; needs global lock
 *          - mostly for small size allocations
 *
 *        map
 *        ---
 *        - process map segment
 *          - mmap() interface; thread-safe
 *          - returns readily zeroed memory
 *
 *        headers
 *        -------
 *        - mapped internal book-keeping for magazines
 *          - pointer stacks
 *          - table to map allocation pointers to magazine pointers
 *            - may differ because of alignments etc.
 *          - optionally, a bitmap to denote allocated slices in magazines
 */

/*
 * TODO
 * ----
 * - free inactive subtables from mdir
 * - fix MALLOCVARSIZEBUF
 */

#define GNUMALLOCHOOKS 1

#if (MALLOCDEBUG)
#include <assert.h>
#endif
#include <features.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#if (MALLOCFREEMAP)
#include <limits.h>
#endif

#if 0
#if (_GNU_SOURCE)
#include <sys/sysinfo.h>
#elif (_POSIX_SOURCE)
#include <unistd.h>
#endif
#endif

//#include <sys/sysinfo.h>

#define ZEROMTX 1
#if defined(PTHREAD) && (PTHREAD)
#include <pthread.h>
#if (!ZEROMTX)
#define MUTEX pthread_mutex_t
#define mtxinit(mp) pthread_mutex_init(mp, NULL)
#define mtxlk(mp) pthread_mutex_lock(mp)
#define mtxunlk(mp) pthread_mutex_unlock(mp)
#endif /* !ZEROMTX */
#endif
#if defined(ZEROMTX) && (ZEROMTX)
#define MUTEX volatile long
#include <zero/mtx.h>
#elif (PTHREAD)
#define MUTEX pthread_mutex_t
#endif
#include <zero/cdecl.h>
#include <zero/param.h>
#include <zero/unix.h>
#include <zero/trix.h>
#if (MALLOCHASH)
#include <zero/hash.h>
#endif

//#include <sys/sysinfo.h>
//#define MALLOCNARN     (4 * get_nprocs_conf())
//#define MALLOCNARN     (2 * sysconf(_SC_NPROCESSORS_CONF))
//#define MALLOCNARN           16

#if 0
#define MALLOCSLABLOG2       18
#define MALLOCSMALLSLABLOG2  13
#define MALLOCMIDSLABLOG2    15
#define MALLOCSMALLMAPLOG2   20
#define MALLOCMIDMAPLOG2     22
#define MALLOCBIGMAPLOG2     24
#endif
#if (MALLOCNEWSLABS)
#define MALLOCSLABLOG2       22
#define MALLOCTINYSLABLOG2   10
#define MALLOCSMALLSLABLOG2  13
#define MALLOCMIDSLABLOG2    15
#define MALLOCBIGSLABLOG2    18
#define MALLOCSMALLMAPLOG2   23
#define MALLOCMIDMAPLOG2     25
#define MALLOCBIGMAPLOG2     27
#if 0
#define MALLOCSUPERSLABLOG2  20
#define MALLOCSLABLOG2       18
#define MALLOCSMALLSLABLOG2  10
#define MALLOCMIDSLABLOG2    13
#define MALLOCBIGSLABLOG2    16
#define MALLOCSMALLMAPLOG2   22
#define MALLOCMIDMAPLOG2     24
#define MALLOCBIGMAPLOG2     26
#endif
#elif (MALLOCEXPERIMENT)
#define MALLOCSUPERSLABLOG2  20
#define MALLOCSLABLOG2       17
#define MALLOCSMALLSLABLOG2  10
#define MALLOCMIDSLABLOG2    13
#define MALLOCBIGSLABLOG2    15
#define MALLOCSMALLMAPLOG2   23
#define MALLOCMIDMAPLOG2     24
#define MALLOCBIGMAPLOG2     25
#if 0
#define MALLOCSUPERSLABLOG2  22
#define MALLOCSLABLOG2       18
#define MALLOCSMALLSLABLOG2  12
#define MALLOCMIDSLABLOG2    14
#define MALLOCBIGSLABLOG2    16
#define MALLOCSMALLMAPLOG2   23
#define MALLOCMIDMAPLOG2     24
#define MALLOCBIGMAPLOG2     25
#endif
#elif (MALLOCNEWHACKS)
#define MALLOCSUPERSLABLOG2  20
#define MALLOCSLABLOG2       18
#define MALLOCSMALLSLABLOG2  12
#define MALLOCMIDSLABLOG2    14
#define MALLOCBIGSLABLOG2    16
#define MALLOCSMALLMAPLOG2   24
#define MALLOCMIDMAPLOG2     25
#define MALLOCBIGMAPLOG2     26
#if 0
#define MALLOCSUPERSLABLOG2  22
#define MALLOCSLABLOG2       20
#define MALLOCSMALLSLABLOG2  12
#define MALLOCMIDSLABLOG2    14
#define MALLOCBIGSLABLOG2    18
#define MALLOCSMALLMAPLOG2   23
#define MALLOCMIDMAPLOG2     24
#define MALLOCBIGMAPLOG2     26
#endif
#if 0
#define MALLOCSUPERSLABLOG2  20
#define MALLOCSLABLOG2       18
#define MALLOCMIDSLABLOG2    12
#define MALLOCBIGSLABLOG2    16
#define MALLOCSMALLMAPLOG2   22
#define MALLOCMIDMAPLOG2     24
#define MALLOCBIGMAPLOG2     26
#endif
#elif (MALLOCVARSIZEBUF)
#define MALLOCSUPERSLABLOG2  22
#define MALLOCSLABLOG2       18
#define MALLOCSMALLSLABLOG2  10
#define MALLOCMIDSLABLOG2    13
#define MALLOCBIGSLABLOG2    16
#define MALLOCSMALLMAPLOG2   20
#define MALLOCMIDMAPLOG2     22
#define MALLOCBIGMAPLOG2     24
#elif (MALLOCNOSBRK)
#define MALLOCSLABLOG2       16
#else
#define MALLOCSLABLOG2       18
#if 0
#define MALLOCMIDLOG2        18
#define MALLOCSMALLLOG2      15
#define MALLOCSMALLMAPLOG2   22
#define MALLOCMIDMAPLOG2     24
#define MALLOCBIGMAPLOG2     26
#endif
#endif
#if (MALLOCBUFMAP) && !(MALLOCNEWHACKS) && 0
#define MALLOCSMALLMAPLOG2   23
#define MALLOCMIDMAPLOG2     24
#define MALLOCBIGMAPLOG2     25
#endif
#define MALLOCMINSIZE        (1UL << MALLOCMINLOG2)
//#define MALLOCMINLOG2        CLSIZELOG2
#define MALLOCMINLOG2        CLSIZELOG2
#define MALLOCNBKT           PTRBITS
#if (MALLOCSTKNDX)
#if (MALLOCSLABLOG2 - MALLOCMINLOG2 <= 16)
#define MAGPTRNDX            uint16_t
#else
#define MAGPTRNDX            uint32_t
#endif
#endif

/*
 * magazines for bucket bktid have 1 << magnblklog2(bktid) blocks of
 * 1 << bktid bytes
 */
#if (MALLOCBUFMAP)
#define magnbufmaplog2(bktid)                                           \
    (((bktid) <= MALLOCSMALLMAPLOG2)                                    \
     ? 4                                                                \
     : (((bktid) <= MALLOCMIDMAPLOG2)                                   \
        ? 3                                                             \
        : 2))
#define magnbufmap(bktid)                                               \
    (1UL << magnbufmaplog2(bktid))
#endif /* MALLOCBUFMAP */
#if (MALLOCNEWSLABS)
#define magnbytelog2(bktid)                                             \
    (((bktid) <= MALLOCTINYSLABLOG2)                                    \
     ? MALLOCSMALLSLABLOG2                                              \
     : (((bktid) <= MALLOCSMALLSLABLOG2)                                \
        ? MALLOCMIDSLABLOG2                                             \
        : (((bktid) <= MALLOCMIDSLABLOG2)                               \
           ? MALLOCBIGSLABLOG2                                          \
           : (((bktid) <= MALLOCSLABLOG2)                               \
              ? (MALLOCSLABLOG2 + max(2, MALLOCSLABLOG2 - (bktid)))     \
              : (((bktid) <= MALLOCMIDMAPLOG2)                          \
                 ? MALLOCBIGMAPLOG2                                     \
                 : (bktid))))))
#if 0
#define magnbytelog2(bktid)                                             \
    (((bktid) <= MALLOCSUPERSLABLOG2)                                   \
     ? (((bktid) <= MALLOCSMALLSLABLOG2)                                \
        ? MALLOCMIDSLABLOG2                                             \
        : (((bktid) <= MALLOCBIGSLABLOG2)                               \
           ? MALLOCSLABLOG2                                             \
           : MALLOCSUPERSLABLOG2))                                      \
     : (((bktid) <= MALLOCSMALLMAPLOG2)                                 \
        ? MALLOCMIDMAPLOG2                                              \
        : (((bktid) <= MALLOCMIDMAPLOG2)                                \
           ? MALLOCBIGMAPLOG2                                           \
           : (bktid))))
#endif
#define magnblklog2(bktid)                                              \
    (magnbytelog2(bktid) - (bktid))
#elif (MALLOCEXPERIMENT)
#define magnbytelog2(bktid)                                             \
    (((bktid) <= MALLOCSUPERSLABLOG2)                                   \
     ? (((bktid) <= MALLOCSMALLSLABLOG2)                                \
        ? MALLOCMIDSLABLOG2                                             \
        : (((bktid) <= MALLOCMIDSLABLOG2)                               \
           ? MALLOCSLABLOG2                                             \
           : ((bktid) + max(2, MALLOCSUPERSLABLOG2 - (bktid)))))        \
     : (((bktid) <= MALLOCSMALLMAPLOG2)                                 \
        ? MALLOCMIDMAPLOG2                                              \
        : (((bktid) <= MALLOCMIDMAPLOG2)                                \
           ? MALLOCBIGMAPLOG2                                           \
           : (bktid))))
#define magnblklog2(bktid)                                              \
    (magnbytelog2(bktid) - (bktid))
#elif (MALLOCNEWHACKS)
#define magnbytelog2(bktid)                                             \
    (((bktid) < MALLOCSMALLSLABLOG2 - 1)                                \
     ? MALLOCBIGSLABLOG2                                                \
     : (((bktid) < MALLOCMIDSLABLOG2 - 1)                               \
        ? MALLOCMIDSLABLOG2                                             \
        : (((bktid) < MALLOCBIGSLABLOG2 - 1)                            \
           ? MALLOCBIGSLABLOG2                                          \
           : (((bktid) < MALLOCSLABLOG2 - 1)                            \
              ? MALLOCSLABLOG2                                          \
              : (((bktid) < MALLOCSMALLMAPLOG2 - 1)                     \
                 ? MALLOCSMALLMAPLOG2                                   \
                 : (((bktid) < MALLOCMIDMAPLOG2 - 1)                    \
                     ? MALLOCMIDMAPLOG2                                 \
                     : (((bktid) <= MALLOCBIGMAPLOG2)                   \
                        ? MALLOCBIGMAPLOG2                              \
                        : (bktid))))))))
#if 0
#define magnbytelog2(bktid) max(MALLOCSLABLOG2, (bktid))
#endif
#define magnblklog2(bktid)                                              \
    (magnbytelog2(bktid) - (bktid))
#elif (MALLOCSTKNDX)
#define magnbytelog2(bktid)                                             \
    (1UL << (bktid))
#define magnblklog2(bktid)                                              \
    (((bktid) <= MALLOCSLABLOG2)                                        \
     ? (MALLOCSLABLOG2 - (bktid))                                       \
     : 0)
#elif (MALLOCVARSIZEBUF)
#define magnbytelog2(bktid)                                             \
    (((bktid) <= MALLOCSLABLOG2)                                        \
     ? (((bktid) <= MALLOCSMALLSLABLOG2)                                \
        ? MALLOCMIDSLABLOG2                                             \
        : (((bktid) <= MALLOCMIDSLABLOG2)                               \
           ? MALLOCBIGSLABLOG2                                          \
           : min(2, MALLOCSUPERSLABLOG2- (bktid))))                     \
     : (((bktid) <= MALLOCSMALLMAPLOG2)                                 \
        ? MALLOCMIDMAPLOG2                                              \
        : (((bktid) <= MALLOCMIDMAPLOG2)                                \
           ? MALLOCBIGMAPLOG2                                           \
           : (bktid))))
#define magnblklog2(bktid)                                              \
    (magnbytelog2(bktid) - (bktid))
#else
#define magnbytelog2(bktid) max((bktid), MALLOCSLABLOG2)
#define magnblklog2(bktid)                                              \
    (((bktid) <= MALLOCSLABLOG2)                                        \
     ? (MALLOCSLABLOG2 - (bktid))                                       \
     : 0)
#endif

#define MAGMAP         0x01
#define MAGGLOB        0x02
#define MAGFLGMASK     (MAGMAP | MAGGLOB)
#define MALLOCHDRSIZE  (8 * PAGESIZE)
/* magazines for larger/fewer allocations embed the tables in the structure */
#define magembedstk(bktid)                                              \
    (magnbytetab(bktid) <= MALLOCHDRSIZE - offsetof(struct mag, data))
#define magembedtab(bktid)                                              \
    (magnbytetab(bktid) <= MALLOCHDRSIZE - offsetof(struct mag, data))
/* magazine header structure */
struct mag {
    void        *adr;
    long         cur;
    long         lim;
    long         arnid;
    long         bktid;
    struct mag  *prev;
    struct mag  *next;
#if (MALLOCFREEMAP)
    uint8_t     *freemap;
#endif
#if (MALLOCSTKNDX)
    MAGPTRNDX   *stk;
    MAGPTRNDX   *ptrtab;
#elif (MALLOCHACKS)
    uintptr_t   *stk;
    uintptr_t   *ptrtab;
#else
    void       **stk;
    void       **ptrtab;
#endif
    uint8_t      data[EMPTY];
};

#if (MALLOCDEBUG) || (MALLOCDIAG)
void
magprint(struct mag *mag)
{
    fprintf(stderr, "MAG %p\n", mag);
    fprintf(stderr, "\tadr\t%p\n", mag->adr);
    fprintf(stderr, "\tcur\t%ld\n", mag->cur);
    fprintf(stderr, "\tlim\t%ld\n", mag->lim);
    fprintf(stderr, "\tbktid\t%ld\n", mag->bktid);
#if (MALLOCFREEMAP)
    fprintf(stderr, "\tfreemap\t%p\n", mag->freemap);
#endif
    fprintf(stderr, "\tstk\t%p\n", mag->stk);
    fprintf(stderr, "\tptrtab\t%p\n", mag->ptrtab);

    return;
}
#endif

/* magazine list header structure */
struct maglist {
    MUTEX       lk;
    long        n;
    struct mag *head;
    struct mag *tail;
};

#if (MALLOCHASH)
struct hashitem {
    void            *ptr;
    struct mag      *mag;
    struct hashitem *next;
};

struct hashlist {
    MUTEX            lk;
    long             n;
    struct hashitem *head;
    struct hashitem *tail;
};
#endif /* MALLOCHASH */

#if (MALLOCFREEMDIR)
struct magitem {
    struct mag *mag;
    long        nref;
};
#endif

#define MALLOCARNSIZE      rounduppow2(sizeof(struct arn), PAGESIZE)
/* arena structure */
struct arn {
    struct maglist magtab[MALLOCNBKT];  // partially allocated magazines
    struct maglist hdrtab[MALLOCNBKT];  // header cache
    long           nref;                // number of threads using the arena
    MUTEX          nreflk;              // lock for updating nref
};

#define MALLOPT_PERTURB_BIT 0x00000001
struct mallopt {
    int action;
    int flg;
    int perturb;
    int mmapmax;
    int mmaplog2;
};

/* malloc global structure */
#define MALLOCINIT 0x00000001L
struct malloc {
    struct maglist    magtab[MALLOCNBKT]; // partially allocated magazines
    struct maglist    hdrtab[MALLOCNBKT];
    struct maglist    freetab[MALLOCNBKT]; // totally unallocated magazines
    struct maglist    hdrbuf[MALLOCNBKT];
    struct maglist    stkbuf[MALLOCNBKT];
#if (MALLOCHASH)
    struct hashlist  *hashtab;
    struct hashitem  *hashbuf;
#endif
    struct arn      **arntab;           // arena structures
    MUTEX            *mlktab;
    void            **mdir;             // allocation header lookup structure
    MUTEX             initlk;           // initialization lock
    MUTEX             heaplk;           // lock for sbrk()
    /* FIXME: should this key be per-thread? */
    pthread_key_t     arnkey;           // for reclaiming arenas to global pool
    long              narn;             // number of arenas in action
    long              flags;            // allocator flags
    int               zerofd;           // file descriptor for mmap()
    struct mallopt    mallopt;          // mallopt() interface
    struct mallinfo   mallinfo;         // mallinfo() interface
};

static struct malloc  g_malloc ALIGNED(PAGESIZE);
__thread long        _arnid = -1;
MUTEX                _arnlk;
long                 curarn;
#if (MALLOCSTAT)
long long            nheapbyte;
long long            nmapbyte;
long long            ntabbyte;
#endif

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS) && !defined(__GLIBC__)
void *(*__malloc_hook)(size_t size, const void *caller);
void *(*__realloc_hook)(void *ptr, size_t size, const void *caller);
void *(*__memalign_hook)(size_t align, size_t size, const void *caller);
void  (*__free_hook)(void *ptr, const void *caller);
void *(*__malloc_initialize_hook)(void);
void  (*__after_morecore_hook)(void);
#endif

#if (MALLOCSTAT)
void
mallocstat(void)
{
    fprintf(stderr, "HEAP: %lld\n", nheapbyte);
    fprintf(stderr, "MAP: %lld\n", nmapbyte);
    fprintf(stderr, "TAB: %lld\n", ntabbyte);
}
#endif

/* allocation pointer flag bits */
#define BLKDIRTY    0x01
#define BLKFLGMASK  (MALLOCMINSIZE - 1)
/* clear flag bits at allocation time */
#define clrptr(ptr) ((void *)((uintptr_t)ptr & ~BLKFLGMASK))

#if (MALLOCFREEMAP)
#define magptrndx(mag, ptr)                                             \
    (((uintptr_t)ptr - ((uintptr_t)mag->adr & ~BLKFLGMASK)) >> mag->bktid)
#endif
#if (MALLOCSTKNDX)
#if (MALLOCFREEMAP)
#define magnbytetab(bktid)                                              \
    (((1UL << (magnblklog2(bktid) + 2)) * sizeof(MAGPTRNDX))            \
     + rounduppow2((1UL << magnblklog2(bktid)) / CHAR_BIT, PAGESIZE))
#else /* !MALLOCFREEMAP */
#define magnbytetab(bktid)                                              \
    ((1UL << (magnblklog2(bktid) + 1)) * sizeof(MAGPTRNDX))
#endif /* MALLOCFREEMAP */
#elif (MALLOCFREEMAP) /* !MALLOCSTKNDX */
#define magnbytetab(bktid)                                              \
    ((1UL << (magnblklog2((bktid) + 1))) * sizeof(MAGPTRNDX)            \
     + rounduppow2((1UL << magnblklog2(bktid)) / CHAR_BIT, PAGESIZE))
#else
#define magnbytetab(bktid) ((1UL << (magnblklog2(bktid) + 1)) * sizeof(void *))
#endif
#if (MALLOCNEWHACKS)
#define magnbyte(bktid) (1UL << magnbytelog2(bktid))
#elif (MALLOCVARSIZEBUF)
#define magnbyte(bktid) (1UL << magnbytelog2(bktid))
#else
#define magnbyte(bktid) (1UL << ((bktid) + magnblklog2(bktid)))
#endif
#define ptralign(ptr, pow2)                                             \
    (!((uintptr_t)ptr & (align - 1))                                    \
     ? ptr                                                              \
     : ((void *)rounduppow2((uintptr_t)ptr, align)))
#define blkalignsz(sz, aln)                                             \
    (((aln) <= MALLOCMINSIZE)                                           \
     ? max(sz, aln)                                                     \
     : (sz) + (aln))

#if (MALLOCSTKNDX)
#define magputptr(mag, ptr1, ptr2)                                      \
    ((mag)->ptrtab[magptr2ndx(mag, ptr1)] = magptr2ndx(mag, ptr2))
#define magptr2ndx(mag, ptr)                                            \
    ((MAGPTRNDX)(((uintptr_t)ptr                                        \
                  - ((uintptr_t)(mag)->adr & ~(MAGFLGMASK)))            \
                 >> (bktid)))
#define magndx2ptr(mag, ndx)                                            \
    ((void *)((uintptr_t)(mag)->adr &~MAGFLGMASK) + ((ndx) << (mag)->bktid))
#define maggetptr(mag, ptr)                                             \
    (magndx2ptr(mag, magptr2ndx(mag, ptr)))
#else /* !MALLOCSTKNDX */
#define magptrid(mag, ptr)                                              \
    (((uintptr_t)(ptr) - ((uintptr_t)(mag)->adr & ~MAGFLGMASK)) >> (mag)->bktid)
#define magputptr(mag, ptr1, ptr2)                                      \
    (((void **)(mag)->ptrtab)[magptrid(mag, ptr1)] = (ptr2))
#define maggetptr(mag, ptr)                                             \
    (((void **)(mag)->ptrtab)[magptrid(mag, ptr)])
#endif /* MALLOCSTKNDX */

#define mdirl1ndx(ptr) (((uintptr_t)ptr >> MDIRL1NDX) & ((1UL << MDIRNL1BIT) - 1))
#define mdirl2ndx(ptr) (((uintptr_t)ptr >> MDIRL2NDX) & ((1UL << MDIRNL2BIT) - 1))
#define mdirl3ndx(ptr) (((uintptr_t)ptr >> MDIRL3NDX) & ((1UL << MDIRNL3BIT) - 1))
#define mdirl4ndx(ptr) (((uintptr_t)ptr >> MDIRL4NDX) & ((1UL << MDIRNL4BIT) - 1))

#if (PTRBITS == 32)
#elif (MALLOC4LEVELTAB)
#define MDIRNL1BIT     12
#define MDIRNL2BIT     12
#define MDIRNL3BIT     12
//#define MDIRNL4BIT     (ADRBITS - MDIRNL1BIT - MDIRNL2BIT - MDIRNL3BIT - MALLOCMINLOG2)
#define MDIRNL4BIT     (PTRBITS - MDIRNL1BIT - MDIRNL2BIT - MDIRNL3BIT - MALLOCMINLOG2)
#define MDIRNL1KEY     (1UL << MDIRNL1BIT)
#define MDIRNL2KEY     (1UL << MDIRNL2BIT)
#define MDIRNL3KEY     (1UL << MDIRNL3BIT)
#define MDIRNL4KEY     (1UL << MDIRNL4BIT)
#define MDIRL1NDX      (MDIRL2NDX + MDIRNL2BIT)
#define MDIRL2NDX      (MDIRL3NDX + MDIRNL3BIT)
#define MDIRL3NDX      (MDIRL4NDX + MDIRNL4BIT)
#define MDIRL4NDX      MALLOCMINLOG2
#else /* PTRBITS != 32 */
#define MDIRNL1BIT     12
#define MDIRNL2BIT     16
#define MDIRNL3BIT     (PTRBITS - MDIRNL1BIT - MDIRNL2BIT - MALLOCMINLOG2)
#define MDIRNL1KEY     (1UL << MDIRNL1BIT)
#define MDIRNL2KEY     (1UL << MDIRNL2BIT)
#define MDIRNL3KEY     (1UL << MDIRNL3BIT)
#define MDIRL1NDX      (MDIRL2NDX + MDIRNL2BIT)
#define MDIRL2NDX      (MDIRL3NDX + MDIRNL3BIT)
#define MDIRL3NDX      MALLOCMINLOG2
#endif

#if (MALLOCSIG)
    void
    mallquit(int sig)
    {
        fprintf(stderr, "QUIT (%d)\n", sig);
#if (MALLOCSTAT)
        mallocstat();
#endif

        exit(sig);
    }
#endif

#if (MALLOCDEBUGHOOKS)

void
mallocinithook(void)
{
    fprintf(stderr, "MALLOC_INIT\n");

    return;
}

void *
mallochook(size_t size, const void *caller)
{
    fprintf(stderr, "MALLOC: %p (%ld)\n", caller, (long)size);

    return NULL;
}

void *
reallochook(void *ptr, size_t size, const void *caller)
{
    fprintf(stderr, "REALLOC: %p: %p (%ld)\n", caller, size, ptr);

    return NULL;
}

void
freehook(void *ptr, const void *caller)
{
    fprintf(stderr, "FREE: %p\n", ptr);

    return;
}

void
morecorehook(void *ptr, const void *caller)
{
    fprintf(stderr, "HEAP: %p\n", sbrk(0));

    return;
}

void *
memalignhook(size_t align, size_t size, const void *caller)
{
    fprintf(stderr, "MEMALIGN: %p: %p (%ld)\n", caller, align, size);

    return;
}

#endif

#if (MALLOCDIAG)
void
mallocdiag(void)
{
    long        arnid;
    long        bktid;
    long        cnt;
    struct arn *arn;
    struct mag *mag;

    for (bktid = 0 ; bktid < MALLOCNBKT ; bktid++) {
        mtxlk(&g_malloc.freetab[bktid].lk);
        mag = g_malloc.freetab[bktid].head;
        cnt = 0;
        while (mag) {
            if (mag->prev) {
                if (mag->prev->next != mag) {
                    fprintf(stderr, "mag->next INVALID on free list\n");
                    magprint(mag);

                    exit(1);
                }
#if 0
            } else if (cnt) {
                fprintf(stderr, "%ld: mag->prev INCORRECT on free list\n", cnt);
                magprint(mag);

                exit(1);
#endif
            }
            if (mag->next) {
                if (mag->next->prev != mag) {
                    fprintf(stderr, "mag->prev INVALID on free list\n");
                    magprint(mag);

                    exit(1);
                }
            }
            if (mag->cur) {
                fprintf(stderr, "mag->cur NON-ZERO on free list\n");
                magprint(mag);

                exit(1);
            }
            cnt++;
        }
        mtxunlk(&g_malloc.freetab[bktid].lk);
    }
    for (arnid = 0 ; arnid < g_malloc.narn ; arnid++) {
        arn = g_malloc.arntab[arnid];
        if (arn) {
            for (bktid = 0 ; bktid < MALLOCNBKT ; bktid++) {
                mtxlk(&arn->magtab[bktid].lk);
                mag = arn->magtab[bktid].head;
                cnt = 0;
                while (mag) {
                    if (mag->prev) {
                        if (mag->prev->next != mag) {
                            fprintf(stderr, "mag->next INVALID on partial list\n");
                            magprint(mag);
                            
                            exit(1);
                        }
                    } else if (cnt) {
                        fprintf(stderr,
                                "mag->prev INCORRECT on partial list\n");
                        magprint(mag);
                        
                        exit(1);
                    }
                    if (mag->next) {
                        if (mag->next->prev != mag) {
                            fprintf(stderr, "mag->prev INVALID on partial list\n");
                            magprint(mag);
                            
                            exit(1);
                        }
                    }
                    if (!mag->cur) {
                        fprintf(stderr, "mag->cur == 0 on partial list\n");
                        magprint(mag);
                        
                        exit(1);
                    }
                    if (mag->cur >= mag->lim) {
                        fprintf(stderr, "mag->cur >= mag->lim on partial list\n");
                        magprint(mag);
                        
                        exit(1);
                    }
                    if (mag->next) {
                        if (mag->next->prev != mag) {
                            fprintf(stderr, "mag->prev != mag on partial list\n");
                            magprint(mag);
                            
                            exit(1);
                        }
#if 0
                    } else if (cnt) {
                        fprintf(stderr, "%ld items on partial list\n", cnt);
#endif
                    }
                    if (mag->cur >= mag->lim) {
                        fprintf(stderr, "mag->cur >= mag->lim on partial list\n");
                        magprint(mag);

                        exit(1);
                    }
                    mag = mag->next;
                    cnt++;
                }
                mtxunlk(&arn->magtab[bktid].lk);
            }
            
        }
    }   

    return;
}
#endif /* MALLOCDIAG */

long
thrarnid(void)
{
    struct arn *arn;

    if (_arnid >= 0) {

        return _arnid;
    }
    mtxlk(&_arnlk);
    _arnid = curarn++;
    arn = g_malloc.arntab[_arnid];
    mtxlk(&arn->nreflk);
    arn->nref++;
    curarn &= (g_malloc.narn - 1);
    pthread_setspecific(g_malloc.arnkey, g_malloc.arntab[_arnid]);
    mtxunlk(&arn->nreflk);
    mtxunlk(&_arnlk);

    return _arnid;
}

static __inline__ long
blkbktid(size_t size)
{
    unsigned long bktid = PTRBITS;
    unsigned long nlz;
    
    nlz = lzerol(size);
    bktid -= nlz;
    if (powerof2(size)) {
        bktid--;
    }
    
    return bktid;
}

static __inline__ void
magsetstk(struct mag *mag)
{
    long bktid = mag->bktid;
#if (MALLOCSTKNDX)
    MAGPTRNDX   *stk = NULL;
    MAGPTRNDX   *tab;
#elif (MALLOCHACKS)
    uintptr_t   *stk = NULL;
    uintptr_t   *tab = NULL;
#else
    void       **stk = NULL;
//    void       **tab = NULL;
#endif
    
    if (magembedtab(bktid)) {
        /* use magazine header's data-field for allocation stack */
#if (MALLOCSTKNDX)
        mag->stk = (MAGPTRNDX *)mag->data;
        mag->ptrtab = &mag->stk[1UL << magnblklog2(bktid)];
#elif (MALLOCHACKS)
        mag->stk = (uintptr_t *)mag->data;
        mag->ptrtab = (uintptr_t *)&mag->stk[1UL << magnblklog2(bktid)];
#else
        mag->stk = (void **)mag->data;
        mag->ptrtab = &mag->stk[1UL << magnblklog2(bktid)];
#endif
#if (MALLOCFREEMAP)
        mag->freemap = (uint8_t *)&mag->stk[(1UL << (magnblklog2(bktid) + 1))];
#endif
//                        stk = mag->stk;
    } else {
        /* map new allocation stack */
        stk = mapanon(g_malloc.zerofd, magnbytetab(bktid));
        if (stk == MAP_FAILED) {
            unmapanon(mag, MALLOCHDRSIZE);
#if (MALLOCSTAT)
            mallocstat();
#endif
#if defined(ENOMEM)
            errno = ENOMEM;
#endif
            
            exit(1);
        }
#if (MALLOCSTKNDX)
        mag->stk = (MAGPTRNDX *)stk;
        mag->ptrtab = &stk[1UL << magnblklog2(bktid)];
#else
        mag->stk = stk;
        mag->ptrtab = &stk[1UL << magnblklog2(bktid)];
#endif
#if (MALLOCFREEMAP)
        mag->freemap = (uint8_t *)&mag->stk[(1UL << (magnblklog2(bktid) + 1))];
#endif
    }
    
    return;
}

static __inline__ struct mag *
maggethdr(long bktid)
{
    struct mag *ret = MAP_FAILED;
    struct mag *mag;
    long        ndx;
    uint8_t    *ptr;

    mtxlk(&g_malloc.hdrtab[bktid].lk);
    mag = g_malloc.hdrbuf[bktid].head;
    if (!mag) {
        ret = mapanon(g_malloc.zerofd, 16 * MALLOCHDRSIZE);
        if (ret == MAP_FAILED) {

            return ret;
        }
        ptr = (uint8_t *)ret;
        magsetstk(ret);
        ndx = 16;
        while (--ndx) {
            ptr += MALLOCHDRSIZE;
            mag = (struct mag *)ptr;
            magsetstk(mag);
            mag->next = g_malloc.hdrtab[bktid].head;
            if (mag->next) {
                mag->next->prev = mag;
            }
            g_malloc.hdrtab[bktid].head = mag;
        }
    } else {
        if (mag->next) {
            mag->next->prev = NULL;
        }
        g_malloc.hdrtab[bktid].head = mag->next;
        ret = mag;
    }
    mtxunlk(&g_malloc.hdrtab[bktid].lk);

    return ret;
}

#if (MALLOCHASH)

static struct hashitem *
hashgetitem(void)
{
    struct hashitem *item = g_malloc.hashbuf;

    if (item) {
        g_malloc.hashbuf = item->next;
    } else {
        struct hashitem *tmp;
        unsigned long    n = PAGESIZE / sizeof(struct hashitem);
        
        item = mapanon(g_malloc.zerofd, PAGESIZE / sizeof(struct hashitem));
        if (item) {
            tmp = item;
            while (--n) {
                tmp->next = ++item;
                tmp = item;
            }
            tmp->next = NULL;
        }
    }

    return item;
}

static struct mag *
findmag(void *ptr)
{
    unsigned long    key = hashq128(&ptr, sizeof(void *), MALLOCNHASHBIT);
    struct hashitem *item;

    item = g_malloc.hashtab[key].head;
    while (item) {
        if (item->ptr == ptr) {
            
            return item->mag;
        }
        item = item->next;
    }

    return NULL;
}

static void
setmag(void *ptr,
       struct mag *mag)
{
    unsigned long    key = hashq128(ptr, sizeof(void *), MALLOCNHASHBIT);
    struct hashitem *prev = NULL;
    struct hashitem *item;

    if (!mag) {
        item = g_malloc.hashtab[key].head;
        while (item) {
            if (item->ptr == ptr) {
                if (prev) {
                    prev->next = item->next;
                }
                if (!item->next && !prev) {
                    g_malloc.hashtab[key].head = NULL;
                    g_malloc.hashtab[key].tail = NULL;
                }

                return;
            }
            prev = item;
            item = item->next;
        }
    } else {
        item = hashgetitem();
        item->ptr = ptr;
        item->mag = mag;
        item->next = g_malloc.hashtab[key].head;
        if (!item->next) {
            g_malloc.hashtab[key].tail = item;
        }
        g_malloc.hashtab[key].head = item;
    }
}

#elif (PTRBITS > 32)

#if (MALLOC4LEVELTAB)

static struct mag *
findmag(void *ptr)
{
    uintptr_t   l1 = mdirl1ndx(ptr);
    uintptr_t   l2 = mdirl2ndx(ptr);
    uintptr_t   l3 = mdirl3ndx(ptr);
    uintptr_t   l4 = mdirl4ndx(ptr);
    void       *ptr1;
    void       *ptr2;
    struct mag *mag = NULL;

    ptr1 = g_malloc.mdir[l1];
    if (ptr1) {
        ptr2 = ((void **)ptr1)[l2];
        if (ptr2) {
            ptr1 = ((void **)ptr2)[l3];
            if (ptr1) {
                mag = ((struct mag **)ptr1)[l4];
            }
        }
    }

    return mag;
}

static void
setmag(void *ptr,
       struct mag *mag)
{
    uintptr_t        l1 = mdirl1ndx(ptr);
    uintptr_t        l2 = mdirl2ndx(ptr);
    uintptr_t        l3 = mdirl3ndx(ptr);
    uintptr_t        l4 = mdirl4ndx(ptr);
    struct mag     **ptr1;
    struct mag     **ptr2;
    void           **pptr;

    mtxlk(&g_malloc.mlktab[l1]);
    ptr1 = g_malloc.mdir[l1];
    if (!ptr1) {
        g_malloc.mdir[l1] = ptr1 = mapanon(g_malloc.zerofd,
                                           MDIRNL2KEY * sizeof(void *));
        if (ptr1 == MAP_FAILED) {
#ifdef ENOMEM
            errno = ENOMEM;
#endif
            
            exit(1);
        }
#if (MALLOCSTAT)
        ntabbyte += MDIRNL2KEY * sizeof(void *);
#endif
    }
    pptr = (void **)ptr1;
    ptr2 = pptr[l2];
    if (!ptr2) {
        pptr[l2] = ptr2 = mapanon(g_malloc.zerofd,
                                  MDIRNL3KEY * sizeof(void *));
        if (ptr2 == MAP_FAILED) {
#ifdef ENOMEM
            errno = ENOMEM;
#endif
            
            exit(1);
        }
#if (MALLOCSTAT)
        ntabbyte += MDIRNL3KEY * sizeof(void *);
#endif
    }
    pptr = (void **)ptr2;
    ptr1 = pptr[l3];
    if (!ptr1) {
        pptr[l3] = ptr1 = mapanon(g_malloc.zerofd,
                                  MDIRNL4KEY * sizeof(void *));
        if (ptr2 == MAP_FAILED) {
#ifdef ENOMEM
            errno = ENOMEM;
#endif
            
            exit(1);
        }
#if (MALLOCSTAT)
        ntabbyte += MDIRNL3KEY * sizeof(void *);
#endif
    }
    ptr2 = &((struct mag **)ptr1)[l4];
    *ptr2 = mag;
    mtxunlk(&g_malloc.mlktab[l1]);
    
    return;
}

#else

static struct mag *
findmag(void *ptr)
{
    uintptr_t   l1 = mdirl1ndx(ptr);
    uintptr_t   l2 = mdirl2ndx(ptr);
    uintptr_t   l3 = mdirl3ndx(ptr);
    void       *ptr1;
    void       *ptr2;
    struct mag *mag = NULL;

    ptr1 = g_malloc.mdir[l1];
    if (ptr1) {
        ptr2 = ((void **)ptr1)[l2];
        if (ptr2) {
#if (MALLOCFREEMDIR)
            mag = ((struct magitem **)ptr2)[l3]->mag;
#endif
            mag = ((struct mag **)ptr2)[l3];
        }
    }

    return mag;
}

static void
setmag(void *ptr,
       struct mag *mag)
{
    uintptr_t        l1 = mdirl1ndx(ptr);
    uintptr_t        l2 = mdirl2ndx(ptr);
    uintptr_t        l3 = mdirl3ndx(ptr);
#if (MALLOCFREEMDIR)
    struct magitem  *ptr1;
    struct magitem  *ptr2;
#else
    struct mag     **ptr1;
    struct mag     **ptr2;
#endif
    void           **pptr;
#if (MALLOCFREEMDIR)
    void            *tab[3] = { NULL, NULL, NULL };
    long             nref;
#endif

    mtxlk(&g_malloc.mlktab[l1]);
    ptr1 = g_malloc.mdir[l1];
    if (!ptr1) {
#if (MALLOCFREEMDIR)
        if (!ptr) {
            
            return;
        }
        g_malloc.mdir[l1] = ptr1 = mapanon(g_malloc.zerofd,
                                           MDIRNL2KEY * sizeof(struct magitem));
#else
        g_malloc.mdir[l1] = ptr1 = mapanon(g_malloc.zerofd,
                                           MDIRNL2KEY * sizeof(void *));
#endif
        if (ptr1 == MAP_FAILED) {
#ifdef ENOMEM
            errno = ENOMEM;
#endif
            
            exit(1);
#if (MALLOCFREEMDIR)
        } else {
            ptr1->nref++;
#endif
        }
#if (MALLOCSTAT)
        ntabbyte += MDIRNL2KEY * sizeof(void *);
#endif
#if (MALLOCFREEMDIR)
    } else if (!mag) {
        nref = --ptr1->nref;
        if (!nref) {
            tab[0] = ptr1;
        }
    } else {
        ptr1->nref++;
#endif /* MALLOCFREEMDIR */
    }
    pptr = (void **)ptr1;
    ptr2 = pptr[l2];
    if (!ptr2) {
#if (MALLOCFREEMDIR)
        if (!mag) {
            nref = --ptr1->nref;
            if (!nref) {
                tab[1] = ptr1;
            }
            ptr1 = tab[0];
            if (ptr1) {
                unmapanon(ptr1,
                          MDIRNL2KEY * sizeof(struct magitem));
                
                return;
            }
        } else {
            pptr[l2] = ptr2 = mapanon(g_malloc.zerofd,
                                      MDIRNL3KEY * sizeof(struct magitem));
        }
#else /* !MALLOCFREEMDIR */
        pptr[l2] = ptr2 = mapanon(g_malloc.zerofd,
                                  MDIRNL3KEY * sizeof(void *));
#endif /* MALLOCFREEMDIR */
        if (ptr2 == MAP_FAILED) {
#ifdef ENOMEM
            errno = ENOMEM;
#endif
            
            exit(1);
        }
#if (MALLOCSTAT)
        ntabbyte += MDIRNL3KEY * sizeof(void *);
#endif
#if (MALLOCFREEMDIR)
    } else if (!mag) {
        nref = --ptr1->nref;
        if (!nref) {
            tab[1] = ptr;
        }
        if (ptr2) {
            ptr2->nref++;
        }
#endif
    }
#if (MALLOCFREEMDIR)
    ptr1 = &((struct magitem *)ptr2)[l3];
    ptr1->mag = mag;
    ptr1->nref++;
#else
    ptr1 = &((struct mag **)ptr2)[l3];
    *ptr1 = mag;
#endif
    mtxunlk(&g_malloc.mlktab[l1]);
    
    return;
}

#endif /* MALLOC4LEVELTAB */

#endif /* MALLOCHASH */

static void
prefork(void)
{
    struct arn *arn;
    long        arnid;
    long        bktid;

    mtxlk(&g_malloc.initlk);
    mtxlk(&_arnlk);
    mtxlk(&g_malloc.heaplk);
    for (arnid = 0 ; arnid < g_malloc.narn ; arnid++) {
        arn = g_malloc.arntab[arnid];
        mtxlk(&arn->nreflk);
        for (bktid = 0 ; bktid < MALLOCNBKT ; bktid++) {
            mtxlk(&arn->hdrtab[bktid].lk);
            mtxlk(&arn->magtab[bktid].lk);
        }
    }
    for (bktid = 0 ; bktid < MALLOCNBKT ; bktid++) {
        mtxlk(&g_malloc.magtab[bktid].lk);
        mtxlk(&g_malloc.freetab[bktid].lk);
        mtxlk(&g_malloc.hdrtab[bktid].lk);
    }
    
    return;
}

static void
postfork(void)
{
    struct arn *arn;
    long        arnid;
    long        bktid;

    for (bktid = 0 ; bktid < MALLOCNBKT ; bktid++) {
        mtxunlk(&g_malloc.hdrtab[bktid].lk);
        mtxunlk(&g_malloc.freetab[bktid].lk);
        mtxunlk(&g_malloc.magtab[bktid].lk);
    }
    for (arnid = 0 ; arnid < g_malloc.narn ; arnid++) {
        arn = g_malloc.arntab[arnid];
        for (bktid = 0 ; bktid < MALLOCNBKT ; bktid++) {
            mtxunlk(&arn->magtab[bktid].lk);
            mtxunlk(&arn->hdrtab[bktid].lk);
        }
        mtxunlk(&arn->nreflk);
    }
    mtxunlk(&g_malloc.heaplk);
    mtxunlk(&_arnlk);
    mtxunlk(&g_malloc.initlk);
    
    return;
}

static void
freearn(void *arg)
{
    struct arn *arn = arg;
    struct mag *mag;
    struct mag *head;
    long        bktid;
#if (MALLOCBUFMAP)
    long        n = 0;
#endif
    
    mtxlk(&arn->nreflk);
    arn->nref--;
    if (!arn->nref) {
        for (bktid = 0 ; bktid < MALLOCNBKT ; bktid++) {
            mtxlk(&arn->magtab[bktid].lk);
            head = arn->magtab[bktid].head;
            if (head) {
#if (MALLOCBUFMAP)
                n = 1;
#endif
                mag = head;
                mag->adr = (void *)((uintptr_t)mag->adr | MAGGLOB);
                while (mag->next) {
#if (MALLOCBUFMAP)
                    n++;
#endif
                    mag = mag->next;
                    mag->adr = (void *)((uintptr_t)mag->adr | MAGGLOB);
                }
                mtxlk(&g_malloc.magtab[bktid].lk);
#if (MALLOCBUFMAP)
                g_malloc.magtab[bktid].n += n;
#endif
                mag->next = g_malloc.magtab[bktid].head;
                if (mag->next) {
                    mag->next->prev = mag;
                }
                g_malloc.magtab[bktid].head = head;
                mtxunlk(&g_malloc.magtab[bktid].lk);
            }
            arn->magtab[bktid].head = NULL;
            mtxunlk(&arn->magtab[bktid].lk);
            mtxlk(&arn->hdrtab[bktid].lk);
            head = arn->hdrtab[bktid].head;
            if (head) {
                mag = head;
                while (mag->next) {
                    mag = mag->next;
                }
                mtxlk(&g_malloc.hdrtab[bktid].lk);
                mag->next = g_malloc.hdrtab[bktid].head;
                if (mag->next) {
                    mag->next->prev = mag;
                }
                g_malloc.hdrtab[bktid].head = head;
                mtxunlk(&g_malloc.hdrtab[bktid].lk);
            }
            mtxunlk(&arn->hdrtab[bktid].lk);
        }
    }
    mtxunlk(&arn->nreflk);
    
    return;
}

static void
mallinit(void)
{
    long        narn;
    long        arnid;
    long        bktid;
#if (!MALLOCNOSBRK)
    long        ofs;
#endif
    uint8_t    *ptr;
#if (MALLOCNEWHACKS) && 0
    long        bkt;
#endif

    mtxlk(&g_malloc.initlk);
    if (g_malloc.flags & MALLOCINIT) {
        mtxunlk(&g_malloc.initlk);
        
        return;
    }
#if (MALLOCNEWHACKS) && 0
    for (bkt = 0 ; bkt < MALLOCNBKT ; bkt++) {
        setnbytelog2(bkt, _nbytelog2tab[bkt]);
    }
#endif
#if (MALLOCSIG)
    signal(SIGQUIT, mallquit);
    signal(SIGINT, mallquit);
    signal(SIGSEGV, mallquit);
    signal(SIGABRT, mallquit);
#endif
#if (MALLOCSTAT)
    atexit(mallocstat);
#endif
#if (MMAP_DEV_ZERO)
    g_malloc.zerofd = open("/dev/zero", O_RDWR);
#endif
#if (MALLOCHASH)
    g_malloc.hashtab = mapanon(g_malloc.zerofd,
                               MALLOCNHASH * sizeof(struct hashlist));
#endif
//    narn = MALLOCNARN;
//    narn = MALLOCNARN;
    narn = MALLOCNARN;
#if 0
#if (_GNU_SOURCE)
    narn = 4 * get_nprocs_conf();
#elif (_POSIX_SOURCE)
    narn = 4 * sysconf(_SC_NPROCESSORS_CONF);
#endif
#endif
    g_malloc.arntab = mapanon(g_malloc.zerofd,
                              narn * sizeof(struct arn **));
    g_malloc.narn = narn;
    ptr = mapanon(g_malloc.zerofd, narn * MALLOCARNSIZE);
    if (ptr == MAP_FAILED) {
        errno = ENOMEM;

        exit(1);
    }
    arnid = narn;
    while (arnid--) {
        g_malloc.arntab[arnid] = (struct arn *)ptr;
        ptr += MALLOCARNSIZE;
    }
#if (ARNREFCNT)
    g_malloc.arnreftab = mapanon(_mapfd, NARN * sizeof(unsigned long));
    g_malloc.arnreflktab = mapanon(_mapfd, NARN * sizeof(MUTEX));
#endif
    arnid = narn;
    while (arnid--) {
        for (bktid = 0 ; bktid < MALLOCNBKT ; bktid++) {
            mtxinit(&g_malloc.arntab[arnid]->magtab[bktid].lk);
            mtxinit(&g_malloc.arntab[arnid]->hdrtab[bktid].lk);
        }
        mtxinit(&g_malloc.hdrtab[bktid].lk);
    }
    bktid = MALLOCNBKT;
    while (bktid--) {
        mtxinit(&g_malloc.freetab[bktid].lk);
        mtxinit(&g_malloc.magtab[bktid].lk);
    }
#if (!MALLOCNOSBRK)
    mtxlk(&g_malloc.heaplk);
    ofs = (1UL << MALLOCSLABLOG2) - ((long)growheap(0) & (PAGESIZE - 1));
    if (ofs != PAGESIZE) {
        growheap(ofs);
    }
    mtxunlk(&g_malloc.heaplk);
#endif /* !MALLOCNOSBRK */
    g_malloc.mlktab = mapanon(g_malloc.zerofd, MDIRNL1KEY * sizeof(long));
    g_malloc.mdir = mapanon(g_malloc.zerofd, MDIRNL1KEY * sizeof(void *));
#if (GNUMALLOCHOOKS) && (MALLOCDEBUGHOOKS)
    __malloc_initialize_hook = mallocinithook;
    __realloc_hook = reallochook;
    __free_hook = freehook;
    __after_morecore_hook = morecorehook;
    __memalign_hook = memalignhook;
#endif
#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__malloc_initialize_hook) {
        __malloc_initialize_hook();
    }
#endif
    pthread_key_create(&g_malloc.arnkey, freearn);
    pthread_atfork(prefork, postfork, postfork);
    g_malloc.flags |= MALLOCINIT;
    mtxunlk(&g_malloc.initlk);
    
    return;
}

/*
 * M_MMAP_MAX       - maximum # of allocation requests serviced simultaneously
 * M_MMAP_THRESHOLD - minimum size for mmap()
 */
int
mallopt(int parm, int val)
{
    int  ret = 0;
    long num;
    
    switch (parm) {
        case M_MXFAST:
            fprintf(stderr, "mallopt: M_MXFAST not supported\n");

            break;
        case M_NLBLKS:
            fprintf(stderr, "mallopt: M_NLBLKS not supported\n");

            break;
        case M_GRAIN:
            fprintf(stderr, "mallopt: M_GRAIN not supported\n");

            break;
        case M_KEEP:
            fprintf(stderr, "mallopt: M_KEEP not supported\n");

            break;
        case M_TRIM_THRESHOLD:
            fprintf(stderr, "mallopt: M_TRIM_THRESHOLD not supported\n");

            break;
        case M_TOP_PAD:
            fprintf(stderr, "mallopt: M_TOP_PAD not supported\n");

            break;
        case M_MMAP_THRESHOLD:
            num = sizeof(long) - tzerol(val);
            if (powerof2(val)) {
                num++;
            }
            ret = 1;
            g_malloc.mallopt.mmaplog2 = num;

            break;
        case M_MMAP_MAX:
            g_malloc.mallopt.mmapmax = val;
            ret = 1;

            break;
        case M_CHECK_ACTION:
            g_malloc.mallopt.action |= val & 0x07;
            ret = 1;
            
            break;
        case M_PERTURB:
            g_malloc.mallopt.flg |= MALLOPT_PERTURB_BIT;
            g_malloc.mallopt.perturb = val;

            break;
        default:
            fprintf(stderr, "MALLOPT: invalid parm %d\n", parm);

            break;
    }

    return ret;
}

int
malloc_info(int opt, FILE *fp)
{
    int retval = -1;

    fprintf(stderr, "malloc_info not implemented\n");

    return retval;
}

struct mallinfo
mallinfo(void)
{
    return g_malloc.mallinfo;
}

void *
_malloc(size_t size,
        size_t align,
        long zero)
{
    struct arn  *arn;
    struct mag  *mag;
    uint8_t     *ptr;
    uint8_t     *ptrval = NULL;
#if (MALLOCSTKNDX)
    MAGPTRNDX   *stk = NULL;
    MAGPTRNDX   *tab;
#elif (MALLOCHACKS)
    uintptr_t   *stk = NULL;
    uintptr_t   *tab = NULL;
#else
    void       **stk = NULL;
    void       **tab = NULL;
#endif
    long         arnid;
    long         sz = max(blkalignsz(size, align), MALLOCMINSIZE);
    long         bktid = blkbktid(sz);
//    long         mapped = 0;
    long         lim;
    long         n;
    size_t       incr;
#if (MALLOCSTEALMAG)
    long         id;
#endif
    long         stolen = 0;
    
    if (!(g_malloc.flags & MALLOCINIT)) {
        mallinit();
    }
    arnid = thrarnid();
    arn = g_malloc.arntab[arnid];
    /* try to allocate from a partially used magazine */
    mtxlk(&arn->magtab[bktid].lk);
    mag = arn->magtab[bktid].head;
    if (mag) {
#if (MALLOCDEBUG) && 0
        fprintf(stderr, "1\n");
#endif
#if (MALLOCSTKNDX)
        ptrval = magndx2ptr(mag, mag->stk[mag->cur]);
        mag->cur++;
#elif (MALLOCHACKS)
        ptrval = (void *)mag->stk[mag->cur++];
#else
        ptrval = mag->stk[mag->cur++];
#endif
#if (MALLOCDEBUG) && 0
        if (mag->cur > mag->lim) {
            fprintf(stderr, "mag->cur: %ld\n", mag->cur);
            magprint(mag);
        }
        assert(mag->cur <= mag->lim);
#endif
        if (mag->cur == mag->lim) {
            /* remove fully allocated magazine from partially allocated list */
            if (mag->next) {
                mag->next->prev = NULL;
            }
            arn->magtab[bktid].head = mag->next;
            mag->prev = NULL;
            mag->next = NULL;
        }
        mtxunlk(&arn->magtab[bktid].lk);
    } else {
        /* try to allocate from list of free magazines with active allocations */
#if (MALLOCDEBUG) && 0
        fprintf(stderr, "2\n");
#endif
        mtxunlk(&arn->magtab[bktid].lk);
        mtxlk(&g_malloc.magtab[bktid].lk);
        mag = g_malloc.magtab[bktid].head;
        if (mag) {
#if (MALLOCSTKNDX)
            ptrval = magndx2ptr(mag, mag->stk[mag->cur]);
            mag->cur++;
#elif (MALLOCHACKS)
            ptrval = (void *)mag->stk[mag->cur++];
#else
            ptrval = mag->stk[mag->cur++];
#endif
#if (MALLOCDEBUG)
            if (mag->cur > mag->lim) {
                fprintf(stderr, "mag->cur: %ld\n", mag->cur);
                magprint(mag);
            }
            assert(mag->cur <= mag->lim);
#endif
            if (mag->cur == mag->lim) {
                if (mag->next) {
                    mag->next->prev = NULL;
                }
                g_malloc.magtab[bktid].head = mag->next;
                mag->adr = (void *)((uintptr_t)mag->adr & ~MAGGLOB);
                mag->prev = NULL;
                mag->next = NULL;
            }
            mtxunlk(&g_malloc.magtab[bktid].lk);
        } else {
#if (MALLOCDEBUG) && 0
            fprintf(stderr, "3\n");
#endif
            mtxunlk(&g_malloc.magtab[bktid].lk);
            mtxlk(&g_malloc.freetab[bktid].lk);
            mag = g_malloc.freetab[bktid].head;
            if (mag) {
#if (MALLOCDEBUG)
                if (mag->cur) {
                    fprintf(stderr, "mag->cur is %ld/%ld (not 0)\n", mag->cur, mag->lim);
                    magprint(mag);
                }
                assert(mag->cur == 0);
#endif
                if (mag->next) {
                    mag->next->prev = NULL;
                }
                g_malloc.freetab[bktid].head = mag->next;
#if (MALLOCBUFMAP)
                g_malloc.freetab[bktid].n--;
#endif
#if (MALLOCSTKNDX)
                ptrval = magndx2ptr(mag, mag->stk[mag->cur]);
                mag->cur++;
#elif (MALLOCHACKS)
                ptrval = (void *)mag->stk[mag->cur++];
#else
                ptrval = mag->stk[mag->cur++];
#endif
                mtxunlk(&g_malloc.freetab[bktid].lk);
                mag->prev = NULL;
                mag->next = NULL;
                if (gtpow2(mag->lim, 1)) {
                    /* queue magazine to partially allocated list */
                    mag->adr = (void *)((uintptr_t)mag->adr | MAGGLOB);
                    mtxlk(&g_malloc.magtab[bktid].lk);
                    mag->next = g_malloc.magtab[bktid].head;
                    if (mag->next) {
                        mag->next->prev = mag;
                    }
                    g_malloc.magtab[bktid].head = mag;
                    mtxunlk(&g_malloc.magtab[bktid].lk);
                }
//                mtxunlk(&g_malloc.freetab[bktid].lk);
            } else {
                /* create new magazine */
#if (MALLOCDEBUG) && 0
                fprintf(stderr, "4\n");
#endif
                mtxunlk(&g_malloc.freetab[bktid].lk);
                mtxlk(&arn->hdrtab[bktid].lk);
                /* try to use a cached magazine header */
                mag = arn->hdrtab[bktid].head;
                if (mag) {
                    if (mag->next) {
                        mag->next->prev = NULL;
                    }
                    arn->hdrtab[bktid].head = mag->next;
                    mtxunlk(&arn->hdrtab[bktid].lk);
                    mag->prev = NULL;
                    mag->next = NULL;
                } else {
                    mtxunlk(&arn->hdrtab[bktid].lk);
                    mtxlk(&g_malloc.hdrtab[bktid].lk);
                    mag = g_malloc.hdrtab[bktid].head;
                    if (mag) {
                        if (mag->next) {
                            mag->next->prev = NULL;
                        }
                        g_malloc.hdrtab[bktid].head = mag->next;
                        mag->prev = NULL;
                        mag->next = NULL;
                    }
                    mtxunlk(&g_malloc.hdrtab[bktid].lk);
                }
#if (MALLOCSTEALMAG)
                if (!mag) {
                    for (id = 0 ; id < g_malloc.narn ; id++) {
                        struct arn *curarn;
                            
                        if (id != arnid) {
                            curarn = g_malloc.arntab[arnid];
                            if (curarn) {
                                mtxlk(&curarn->magtab[bktid].lk);
                                mag = curarn->magtab[bktid].head;
                                if (mag) {
                                    if (mag->next) {
                                        mag->next->prev = NULL;
                                    }
                                    curarn->magtab[bktid].head = mag->next;
                                    mtxunlk(&curarn->magtab[bktid].lk);
                                    mag->arnid = id;
                                    stolen = 1;
                                    
                                    break;
                                }
                                mtxunlk(&curarn->magtab[bktid].lk);
                            }
                        }
                    }
                }
                if ((mag) && (stolen)) {
#if (MALLOCSTKNDX)
                    ptrval = magndx2ptr(mag, mag->stk[mag->cur]);
                    mag->cur++;
#elif (MALLOCHACKS)
                    ptrval = (void *)mag->stk[mag->cur++];
#else
                    ptrval = mag->stk[mag->cur++];
#endif
                    mtxlk(&arn->magtab[bktid].lk);
                    if (mag->cur < mag->lim) {
                        mag->prev = NULL;
                        mag->next = arn->magtab[bktid].head;
                        if (mag->next) {
                            mag->next->prev = mag;
                        }
                        arn->magtab[bktid].head = mag;
                    } else {
                        mag->prev = NULL;
                        mag->next = NULL;
                    }
                    mtxunlk(&arn->magtab[bktid].lk);
//                    stk = mag->stk;
                }
#endif /* MALLOCSTEALMAG */
                if (
#if (MALLOCSTEALMAG)
                    !stolen
#else
                    !mag
#endif
                    ) {
                    mtxunlk(&arn->hdrtab[bktid].lk);
                    /* map new magazine header */
//                    mag = mapanon(g_malloc.zerofd, MALLOCHDRSIZE);
                    mag = maggethdr(bktid);
                    if (mag == MAP_FAILED) {
#if (MALLOCSTAT)
                        mallocstat();
#endif
#if defined(ENOMEM)
                        errno = ENOMEM;
#endif
                        
                        return NULL;
                    }
                    magsetstk(mag);
                }
                if (!stolen) {
                    ptr = SBRK_FAILED;
#if (!MALLOCNOSBRK)
                    if (bktid <= MALLOCSLABLOG2) {
                        /* try to allocate slab from heap */
                        mtxlk(&g_malloc.heaplk);
                        ptr = growheap(magnbyte(bktid));
                        mtxunlk(&g_malloc.heaplk);
                    }
#endif
                    if (ptr == SBRK_FAILED) {
                        /* try to map slab */
                        ptr = mapanon(g_malloc.zerofd, magnbyte(bktid));
                        if (ptr == MAP_FAILED) {
                            unmapanon(mag, MALLOCHDRSIZE);
                            if (!magembedstk(bktid)) {
                                unmapanon(stk, magnbytetab(bktid));
                            }
#if (MALLOCSTAT)
                            mallocstat();
#endif
#if defined(ENOMEM)
                            errno = ENOMEM;
#endif
                            
                            return NULL;
                        }
#if (MALLOCSTAT)
                        nmapbyte += magnbyte(bktid);
#endif
                        mag->adr = (void *)((uintptr_t)ptr | MAGMAP);
//                        mapped = 1;
                    } else {
#if (MALLOCSTAT)
                        nheapbyte += magnbyte(bktid);
#endif
                        mag->adr = ptr;
                    }
#if (MALLOCDEBUG)
                    assert(mag->adr != NULL);
#endif
                    ptrval = ptr;
                    /* initialise magazine header */
                    lim = 1UL << magnblklog2(bktid);
                    mag->cur = 1;
                    mag->lim = lim;
                    mag->arnid = arnid;
                    mag->bktid = bktid;
                    mag->prev = NULL;
                    mag->next = NULL;
                    stk = mag->stk;
                    tab = mag->ptrtab;
                    /* initialise allocation stack */
                    incr = 1UL << bktid;
                    for (n = 0 ; n < lim ; n++) {
#if (MALLOCSTKNDX)
                        stk[n] = (MAGPTRNDX)n;
                        tab[n] = (MAGPTRNDX)0;
#elif (MALLOCHACKS)
                        stk[n] = (uintptr_t)ptr;
                        tab[n] = (uintptr_t)0;
#else
                        stk[n] = ptr;
                        tab[n] = NULL;
#endif
                        ptr += incr;
                    }
                    if (gtpow2(lim, 1)) {
                        /* queue slab with an active allocation */
                        mtxlk(&arn->magtab[bktid].lk);
                        mag->next = arn->magtab[bktid].head;
                        if (mag->next) {
                            mag->next->prev = mag;
                        }
                        arn->magtab[bktid].head = mag;
                        mtxunlk(&arn->magtab[bktid].lk);
                    }
                }
            }
        }
    }
    ptr = clrptr(ptrval);
    if (ptr) {
        if ((zero) && (((uintptr_t)ptrval & BLKDIRTY))) {
            memset(ptr, 0, 1UL << bktid);
        } else if (g_malloc.mallopt.flg & MALLOPT_PERTURB_BIT) {
            int perturb = g_malloc.mallopt.perturb;

            perturb = (~perturb) & 0xff;
            memset(ptr, perturb, 1UL << bktid);
        }
        if ((align) && ((uintptr_t)ptr & (align - 1))) {
            ptr = ptralign(ptr, align);
        }
        /* store unaligned source pointer */
        magputptr(mag, ptr, clrptr(ptrval));
#if (MALLOCFREEMAP)
        if (bitset(mag->freemap, magptrndx(mag, ptr))) {
            fprintf(stderr, "trying to reallocate block");
            
            abort();
        }
        setbit(mag->freemap, magptrndx(mag, ptr));
#endif
        /* add magazine to lookup structure using ptr as key */
        setmag(ptr, mag);
#if defined(ENOMEM)
    } else {
        errno = ENOMEM;
#endif
    }
#if (MALLOCDEBUG)
    if (!ptr) {
#if (MALLOCSTAT)
        mallocstat();
#endif
#if defined(ENOMEM)
        errno = ENOMEM;
#endif
        magprint(mag);

        abort();
    }
    assert(mag->adr != NULL);
    assert(mag != NULL);
    assert(ptr != NULL);
#endif
#if (MALLOCDIAG)
//    fprintf(stderr, "DIAG in _malloc()\n");
    mallocdiag();
#endif
    
    return ptr;
}

void
_free(void *ptr)
{
    struct arn *arn;
    struct mag *mag;
    void       *adr = NULL;
    long        arnid;
    long        lim;
    long        bktid;
    long        freemap = 0;
    
    mag = findmag(ptr);
    if (mag) {
        setmag(ptr, NULL);
        arnid = mag->arnid;
        bktid = mag->bktid;
        arn = g_malloc.arntab[arnid];
        mtxlk(&arn->magtab[bktid].lk);
        mtxlk(&g_malloc.magtab[bktid].lk);
        /* remove pointer from allocation lookup structure */
        setmag(ptr, NULL);
#if (MALLOCFREEMAP)
        if (!bitset(mag->freemap, magptrndx(mag, ptr))) {
            fprintf(stderr, "trying free an unused block\n");

            abort();
        }
        clrbit(mag->freemap, magptrndx(mag, ptr));
#endif
        ptr = maggetptr(mag, ptr);
        if (g_malloc.mallopt.flg & MALLOPT_PERTURB_BIT) {
            int perturb = g_malloc.mallopt.perturb;

            perturb &= 0xff;
            memset(ptr, perturb, 1UL << bktid);
        }
//        arn = g_malloc.arntab[arnid];
        lim = mag->lim;
        bktid = mag->bktid;
#if (MALLOCSTKNDX)
        mag->stk[--mag->cur] = magptr2ndx(mag, ptr);
#elif (MALLOCHACKS)
        mag->stk[--mag->cur] = (uintptr_t)ptr | BLKDIRTY;
#else
        mag->stk[--mag->cur] = (void *)((uintptr_t)ptr | BLKDIRTY);
#endif
        if (!mag->cur) {
            if (gtpow2(lim, 1)) {
                /* remove magazine from partially allocated list */
                if (mag->prev) {
                    mag->prev->next = mag->next;
                } else if ((uintptr_t)mag->adr & MAGGLOB) {
                    g_malloc.magtab[bktid].head = mag->next;
                } else {
                    arn->magtab[bktid].head = mag->next;
                }
                if (mag->next) {
                    mag->next->prev = mag->prev;
                }
                mag->prev = NULL;
                mag->next = NULL;
            }
            if ((uintptr_t)mag->adr & MAGMAP) {
                /* indicate magazine was mapped */
                adr = (void *)((uintptr_t)mag->adr & ~MAGFLGMASK);
                freemap = 1;
            } else {
                mag->prev = NULL;
                /* queue map to list of totally unallocated ones */
                mtxlk(&g_malloc.freetab[bktid].lk);
                mag->next = g_malloc.freetab[bktid].head;
                if (mag->next) {
                    mag->next->prev = mag;
                }
                g_malloc.freetab[bktid].head = mag;
                mtxunlk(&g_malloc.freetab[bktid].lk);
            }
            /* allocate from list of partially allocated magazines */
        } else if (gtpow2(lim, 1) && mag->cur == lim - 1) {
            /* queue an unqueued earlier fully allocated magazine */
            mag->prev = NULL;
            mag->next = arn->magtab[bktid].head;
            if (mag->next) {
                mag->next->prev = mag;
            }
            arn->magtab[bktid].head = mag;
        }
        mtxunlk(&g_malloc.magtab[bktid].lk);
        mtxunlk(&arn->magtab[bktid].lk);
        if (freemap) {
#if (MALLOCBUFMAP)
            if ((uintptr_t)mag->adr & MAGMAP) {
                mtxlk(&g_malloc.freetab[bktid].lk);
                if (g_malloc.freetab[bktid].n < magnbufmap(bktid)) {
                    mag->prev = NULL;
                    mag->next = g_malloc.freetab[bktid].head;
                    if (mag->next) {
                        mag->next->prev = mag;
                    }
                    g_malloc.freetab[bktid].head = mag;
                    g_malloc.freetab[bktid].n++;
                    freemap = 0;
                }
                mtxunlk(&g_malloc.freetab[bktid].lk);
            }
            if (freemap) {
                mtxunlk(&g_malloc.freetab[bktid].lk);
                /* unmap slab */
                unmapanon(adr, magnbyte(bktid));
                mag->adr = NULL;
                mag->prev = NULL;
                /* add magazine header to header cache */
                mtxlk(&arn->hdrtab[bktid].lk);
                mag->next = arn->hdrtab[bktid].head;
                if (mag->next) {
                    mag->next->prev = mag;
                }
                arn->hdrtab[bktid].head = mag;
                mtxunlk(&arn->hdrtab[bktid].lk);
            }
#else
            /* unmap slab */
            unmapanon(adr, magnbyte(bktid));
            mag->adr = NULL;
            mag->prev = NULL;
            /* add magazine header to header cache */
            mtxlk(&arn->hdrtab[bktid].lk);
            mag->next = arn->hdrtab[bktid].head;
            if (mag->next) {
                mag->next->prev = mag;
            }
            arn->hdrtab[bktid].head = mag;
            mtxunlk(&arn->hdrtab[bktid].lk);
#endif
        }
    }
#if (MALLOCDIAG)
//    fprintf(stderr, "DIAG in _free()\n");
    mallocdiag();
#endif
    
    return;
}

void *
malloc(size_t size)
{
    void   *ptr;

    if (!size) {
        
        return NULL;
    }
#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__malloc_hook) {
        void *caller = NULL;

//        m_getretadr(caller);
        __malloc_hook(size, (const void *)caller);;
    }
#endif
    ptr = _malloc(size, 0, 0);
#if (MALLOCDEBUG)
    assert(ptr != NULL);
#endif

    return ptr;
}

void *
calloc(size_t n, size_t size)
{
    size_t sz = max(n * size, MALLOCMINSIZE);
    void *ptr = _malloc(sz, 0, 1);

    if (!sz) {
        
        return NULL;
    }
#if (MALLOCDEBUG)
    assert(ptr != NULL);
#endif

    return ptr;
}

void *
_realloc(void *ptr,
         size_t size,
         long rel)
{
    void       *retptr = ptr;
    long        sz = max(size, MALLOCMINSIZE);
    struct mag *mag = (ptr) ? findmag(ptr) : NULL;
    long        bktid = blkbktid(sz);
    long        csz = (mag) ? 1UL << mag->bktid : 0;

    if (!ptr) {
        retptr = _malloc(sz, 0, 0);
    } else if ((mag) && mag->bktid < bktid) {
        csz = min(csz, size);
        retptr = _malloc(sz, 0, 0);
        if (retptr) {
            memcpy(retptr, ptr, csz);
            _free(ptr);
            ptr = NULL;
        }
    }
    if ((rel) && (ptr)) {
        _free(ptr);
    }
#if (MALLOCDEBUG)
    assert(retptr != NULL);
#endif
    if (!retptr) {
#if defined(ENOMEM)
        errno = ENOMEM;
#endif
#if (MALLOCSTAT)
        mallocstat();
#endif
    }

    return retptr;
}

void *
realloc(void *ptr,
        size_t size)
{
    void *retptr = NULL;

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__realloc_hook) {
        void *caller = NULL;

//        m_getretadr(caller);
        __realloc_hook(ptr, size, (const void *)caller);
    }
#endif
    if (!size && (ptr)) {
        _free(ptr);
    } else {
        retptr = _realloc(ptr, size, 0);
    }
#if (MALLOCDEBUG)
    assert(retptr != NULL);
#endif

    return retptr;
}

void
free(void *ptr)
{
#if (_BNU_SOURCE)
    if (__free_hook) {
        void *caller = NULL;

//        m_getretadr(caller);
        __free_hook(ptr, (const void *)caller);
    }
#endif
    if (ptr) {
        _free(ptr);
    }

    return;
}

#if (_ISOC11_SOURCE)
void *
aligned_alloc(size_t align,
              size_t size)
{
    void   *ptr = NULL;
    size_t  aln = max(align, MALLOCMINSIZE);

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__memalign_hook) {
        void *caller = NULL;
        
//        m_getretadr(caller);
        __memalign_hook(align, size, (const void *)caller);
    }
#endif
    if (!powerof2(aln) || (size & (aln - 1))) {
        errno = EINVAL;
    } else {
        ptr = _malloc(size, aln, 0);
    }
#if (MALLOCDEBUG)
    assert(ptr != NULL);
#endif

    return ptr;
}

#endif

#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
int
posix_memalign(void **ret,
               size_t align,
               size_t size)
{
    void   *ptr = NULL;
    size_t  aln = max(align, MALLOCMINSIZE);
    int     retval = -1;

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__memalign_hook) {
        void *caller = NULL;
        
//        m_getretadr(caller);
        __memalign_hook(align, size, (const void *)caller);
    }
#endif
    if (!powerof2(align) || (align & (sizeof(void *) - 1))) {
        errno = EINVAL;
    } else {
        ptr = _malloc(size, aln, 0);
        if (ptr) {
            retval = 0;
        }
    }
#if (MALLOCDEBUG)
    assert(ptr != NULL);
#endif
    *ret = ptr;

    return retval;
}
#endif

/* STD: UNIX */

#if ((_BSD_SOURCE)                                                      \
     || (_XOPEN_SOURCE >= 500 || ((_XOPEN_SOURCE) && (_XOPEN_SOURCE_EXTENDED))) \
     && !(_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600))
void *
valloc(size_t size)
{
    void *ptr;

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__memalign_hook) {
        void *caller = NULL;
        
//        m_getretadr(caller);
        __memalign_hook(PAGESIZE, size, (const void *)caller);
    }
#endif
    ptr = _malloc(size, PAGESIZE, 0);
#if (MALLOCDEBUG)
    assert(ptr != NULL);
#endif
    
    return ptr;
}
#endif

void *
memalign(size_t align,
         size_t size)
{
    void   *ptr = NULL;
    size_t  aln = max(align, MALLOCMINSIZE);

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__memalign_hook) {
        void *caller = NULL;
        
//        m_getretadr(caller);
        __memalign_hook(align, size, (const void *)caller);
    }
#endif
    if (!powerof2(align)) {
        errno = EINVAL;
    } else {
        ptr = _malloc(size, aln, 0);
    }
#if (MALLOCDEBUG)
    assert(ptr != NULL);
#endif

    return ptr;
}

#if (_BSD_SOURCE)
void *
reallocf(void *ptr,
         size_t size)
{
    void *retptr = _realloc(ptr, size, 1);

#if (MALLOCDEBUG)
    assert(retptr != NULL);
#endif

    return retptr;
}
#endif

#if defined(_GNU_SOURCE)
void *
pvalloc(size_t size)
{
    size_t  sz = rounduppow2(size, PAGESIZE);
    void   *ptr = _malloc(sz, PAGESIZE, 0);

#if (MALLOCDEBUG)
    assert(ptr != NULL);
#endif

    return ptr;
}
#endif

void
cfree(void *ptr)
{
    if (ptr) {
        _free(ptr);
    }

    return;
}

size_t
malloc_usable_size(void *ptr)
{
    struct mag *mag = findmag(ptr);
    size_t      sz = (mag) ? 1UL << mag->bktid : 0;

    return sz;
}

size_t
malloc_good_size(size_t size)
{
    size_t sz = 1UL << blkbktid(size);

    return sz;
}

size_t
malloc_size(void *ptr)
{
    struct mag *mag = findmag(ptr);
    size_t      sz = (mag) ? 1UL << mag->bktid : 0;
    
    return sz;
}

