#include <stddef.h>
#include <stdint.h>
#include <zero/cdecl.h>
#include <zero/param.h>
#include <zero/trix.h>
#if defined(__i386__)
#include <kern/unit/ia32/link.h>
#include <kern/unit/ia32/vm.h>
#endif
#include <kern/mem/mem.h>
#include <kern/mem/mag.h>
#include <kern/mem/slab32.h>

#if (MEMTEST)
#include <stdio.h>
#define kprintf printf
#define panic() abort()
#endif

struct maghdr  maghdrtab[1UL << (PTRBITS - SLABMINLOG2)] ALIGNED(PAGESIZE);
struct maghdr *freemagtab[PTRBITS];
long           freelktab[PTRBITS];
uint8_t        membitmap[1UL << (PTRBITS - MAGMINLOG2 - 3)];

void *
memalloc(unsigned long nb, long flg)
{
    void          *ptr = NULL;
    unsigned long  sz = 0;
    unsigned long  bkt;
    struct maghdr *mag;
    uint8_t       *u8ptr;
    unsigned long  l;
    unsigned long  n;

    nb = max(MAGMIN, nb);
    bkt = membkt(nb);
    if (nb > (SLABMIN >> 1)) {
#if (MEMTEST)
        ptr = slaballoc(virtslabtab, virthdrtab, nb, flg);
#else
        ptr = vmmapvirt((uint32_t *)&_pagetab,
                        slaballoc(virtslabtab, virthdrtab, nb, flg), nb, flg);
#endif
        mag = &maghdrtab[maghdrnum(ptr)];
        mag->n = 1;
        mag->ndx = 0;
    } else {
        maglk(bkt);
        mag = freemagtab[bkt];
        if (mag) {
            ptr = magpop(mag);
            if (magfull(mag)) {
                if (mag->next) {
                    mag->next->prev = NULL;
                }
                freemagtab[bkt] = mag->next;
            }
        } else {
            sz = 1UL << bkt;
            ptr = u8ptr = slaballoc(virtslabtab, virthdrtab, sz, flg);
            n = 1UL << (SLABMINLOG2 - bkt);
            mag = &maghdrtab[maghdrnum(ptr)];
            mag->n = n;
            mag->ndx = 0;
            mag->bkt = bkt;
#if (MEMTEST)
//            printf("INIT: %ld items:", n);
#endif
            for (l = 0 ; l < n ; l++) {
                mag->ptab[l] = u8ptr;
#if (MEMTEST)
//                printf(" %p", u8ptr);
#endif
                u8ptr += sz;
            }
#if (MEMTEST)
//            printf("\n");
#endif
            ptr = magpop(mag);
            if (freemagtab[bkt]) {
                freemagtab[bkt]->prev = mag;
            }
            mag->next = freemagtab[bkt];
            freemagtab[bkt] = mag;
        }
        magunlk(bkt);
    }
    if (ptr) {
        if (bitset(membitmap, (uintptr_t)ptr >> MAGMINLOG2)) {
            kprintf("duplicate allocation %p\n", ptr);
            
            panic();
        }
        setbit(membitmap, (uintptr_t)ptr >> MAGMINLOG2);
        if (flg & MEMZERO) {
            bzero(ptr, 1UL << bkt);
        }
    }

    return ptr;
}

void
kfree(void *ptr)
{
    struct maghdr  *mag = &maghdrtab[maghdrnum(ptr)];
    unsigned long   bkt;

    if (!ptr) {

        return;
    }
    if (!bitset(membitmap, (uintptr_t)ptr >> MAGMINLOG2)) {
        kprintf("invalid free: %p\n", ptr);

        panic();
    }
    clrbit(membitmap, (uintptr_t)ptr >> MAGMINLOG2);
    if (mag->n == 1 && !mag->ndx) {
        slabfree(virtslabtab, virthdrtab, ptr);
    } else if (mag->n) {
        magpush(mag, ptr);
        if (magempty(mag)) {
            slabfree(virtslabtab, virthdrtab, ptr);
            bkt = mag->bkt;
            maglk(bkt);
            if (mag->prev) {
                mag->prev->next = mag->next;
            } else {
                freemagtab[bkt] = mag->next;
            }
            if (mag->next) {
                mag->next->prev = mag->prev;
            }
            magunlk(bkt);
        } else if (mag->ndx == mag->n - 1) {
//            slab = &virthdrtab[slabnum(ptr)];
//            bkt = slabgetbkt(slab);
            bkt = mag->bkt;
            mag->prev = NULL;
            maglk(bkt);
            if (freemagtab[bkt]) {
                freemagtab[bkt]->prev = mag;
            }
            mag->next = freemagtab[bkt];
            freemagtab[bkt] = mag;
            magunlk(bkt);
        }
    }
#if (MEMTEST)
    magdiag();
#endif

    return;
}

