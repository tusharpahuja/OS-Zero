#ifndef __ZERO_IA32_TYPES_H__
#define __ZERO_IA32_TYPES_H__

#include <stdint.h>
#include <stdint.h>
//#include <signal.h>
#include <zero/cdecl.h>
#include <zero/param.h>
#include <zero/x86/types.h>

/* C call frame - 8 bytes */
struct m_stkframe {
    /* automatic variables go here */
    int32_t fp;         // caller frame pointer
    int32_t pc;         // return address
    /* call parameters on stack go here in 'reverse order' */
};

/* general purpose registers - 32 bytes */
struct m_genregs {
    int32_t edi;
    int32_t esi;
    int32_t esp;
    int32_t ebp;
    int32_t ebx;
    int32_t edx;
    int32_t ecx;
    int32_t eax;
};

/* segment registers */
struct m_segregs {
    int32_t ds;         // data segment
    int32_t es;         // data segment
    int32_t fs;         // thread-local storage
    int32_t gs;         // kernel per-CPU segment
};

/* return stack for IRET - 20 bytes */
struct m_jmpframe {
    int32_t eip;	// old instruction pointer
    int16_t cs;		// code segment selector
    int16_t pad1;	// pad to 32-bit boundary
    int32_t eflags;	// machine status word
    /* present in case of privilege transition */
    int32_t uesp;	// user stack pointer
    int16_t uss;	// user stack segment selector
    int16_t pad2;	// pad to 32-bit boundary
};

/* task state segment */
struct m_tss {
    uint16_t link, _linkhi;
    uint32_t esp0;
    uint16_t ss0, _ss0hi;
    uint32_t esp1;
    uint16_t ss1, _ss1hi;
    uint32_t esp2;
    uint16_t ss2, _ss2hi;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint16_t es, _eshi;
    uint16_t cs, _cshi;
    uint16_t ss, _sshi;
    uint16_t ds, _dshi;
    uint16_t fs, _fshi;
    uint16_t gs, _gshi;
    uint16_t ldt, _ldthi;
    uint16_t trace;
    uint16_t iomapofs;
//    long     hasiomap;          // indicates presence of an 8K iomap field
//    uint8_t  iomap[EMPTY] ALIGNED(PAGESIZE);
//    uint8_t  iomap[8192] ALIGNED(CLSIZE);
};

/* data segment registers - 16 bytes */
/* thread control block */
#define M_TCBFCTXSIZE 512
struct m_tcb {
    uint8_t           fctx[M_TCBFCTXSIZE];      // 512 bytes @ 0
    int32_t           fxsave;                   // 4 bytes @ 512
    int32_t           pdbr;                     // 4 bytes @ 516
    struct m_segregs  segregs;                  // 16 bytes @ 520
    struct m_genregs  genregs;                  // 32 bytes @ 536
    struct m_jmpframe iret;                     // 20 bytes @ 568
};

#endif /* __ZERO_IA32_TYPES_H__ */

