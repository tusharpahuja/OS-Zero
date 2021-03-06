#ifndef __ZAS_ZAS_H__
#define __ZAS_ZAS_H__

#include <zas/conf.h>

#if (ZASZEROHASH)
#define NHASHBIT 16
#define ZASNHASH (1UL << NHASHBIT)
#else
#define ZASNHASH 1024
#endif

#include <stddef.h>
#include <stdint.h>

#if (ZAS32BIT)
typedef uint32_t  zasmemadr_t;
typedef int32_t   zasword_t;
typedef uint32_t  zasuword_t;
typedef uint32_t  zassize_t;
#else /* 64-bit */
typedef uint64_t  zasmemadr_t;
typedef int64_t   zasword_t;
typedef uint64_t  zasuword_t;
typedef uint64_t  zassize_t;
#endif

#if (ZVM)
#define zasgetmemt(adr, t)      zvmgetmemt(adr, t)
#define zasputmemt(adr, t, val) zvmputmemt(adr, t, val)
#endif

#define ZASRESOLVE     (~((zasword_t)0))

#define ZASTOKENVALUE  0x01
#define ZASTOKENLABEL  0x02
#define ZASTOKENINST   0x03
#define ZASTOKENREG    0x04
#if (ZASVEC)
#define ZASTOKENVAREG  0x05
#define ZASTOKENVLREG  0x06
#endif
#define ZASTOKENSYM    0x07
#define ZASTOKENCHAR   0x08
#define ZASTOKENIMMED  0x09
#define ZASTOKENINDIR  0x0a
#define ZASTOKENADR    0x0b
#define ZASTOKENINDEX  0x0c
#define ZASTOKENDATA   0x0d
#define ZASTOKENGLOBL  0x0e
#define ZASTOKENSPACE  0x0f
#define ZASTOKENORG    0x10
#define ZASTOKENALIGN  0x11
#define ZASTOKENASCIZ  0x12
#define ZASTOKENSTRING 0x13
#define ZASTOKENPAREN  0x14
#if (ZASPREPROC)
#define ZASTOKENOP     0x15
#endif
#define ZASNTOKEN      0x16

struct zasopinfo {
    const char *name;
    uint8_t     narg;
};

struct zasop {
    const char *name;
    uint8_t     code;
    uint8_t     narg;
    uint8_t     len;
#if (WPMVEC)
    uint32_t    flg;
#endif
    struct zasop *next;
#if (ZVM) && (ZVMMULTITAB)
    struct zasop *tab;
#endif
};

struct zaslabel {
    char            *name;
    uintptr_t        adr;
    struct zaslabel *next;
};

struct zasvalue {
    zasword_t val;
    uint8_t   size;
};

struct zasinst {
    const char *name;
#if (ZASDB)
    uint8_t    *data;
#endif
    uint8_t     op;
    uint8_t     narg;
#if (WPMVEC)
    uint8_t     flg;
#endif
};

struct zassymrec {
    struct zassymrec *next;
    char             *name;
    uintptr_t         adr;
};

struct zassym {
    char       *name;
    zasuword_t  adr;
};

struct zasadr {
    char      *name;
    uintptr_t  val;
};

struct zasndx {
    zasword_t reg;
    zasword_t val;
};

struct zasval {
    char          *name;
    zasword_t      val;
    struct zasval *next;
};

struct zastoken {
    struct zastoken     *prev;
    struct zastoken     *next;
    unsigned long        type;
    zasword_t            val;
#if (ZASNEWHACKS)
    zasword_t            size;
#endif
    zasword_t            unit;
#if (WPMVEC)
    uint32_t             opflg;
#endif
#if (ZASDB)
    uint8_t             *file;
    unsigned long        line;
#endif
    union {
        struct zaslabel  label;
        struct zasvalue  value;
        struct zasinst   inst;
        struct zassym    sym;
        struct zasadr    adr;
        struct zasndx    ndx;
        char            *str;
        uint8_t          ch;
        uint8_t          size;
        zasuword_t       reg;
    } data;
};

#if (ZASDB) || (WPMDB)
struct zasline {
    struct zasline *next;
    uintptr_t       adr;
    uint8_t        *file;
    unsigned long   num;
    uint8_t        *data;
};

struct zasline * zasfindline(zasmemadr_t adr);
#endif

extern void        zasinit(void);
#if (ZASALIGN)
extern void        zasinitalign(void);
#endif
extern zasmemadr_t zastranslate(zasmemadr_t base);
extern void        zasresolve(zasmemadr_t base);
extern void        zasremovesyms(void);
#if (ZASBUF)
extern void        zasreadfile(char *name, zasmemadr_t adr, int bufid);
#else
extern void        zasreadfile(char *name, zasmemadr_t adr);
#endif

extern void        zasfreetoken(struct zastoken *token);
extern void        zasqueuesym(struct zassymrec *sym);

extern void        zasprinttoken(struct zastoken *token);

typedef struct zastoken * zastokfunc_t(struct zastoken *, zasmemadr_t,
                                       zasmemadr_t *);

#endif /* __ZAS_ZAS_H__ */

