#include <stddef.h>
#include <zero/cdefs.h>
#include <zero/param.h>
#include <zero/asm.h>

NOINLINE void
m_getpc(void **pp)
{
    void *_ptr = NULL;
    
    m_getretadr(&_ptr);
    *pp = _ptr;

    return;
}

INLINE void
m_getpc2(void **pp)
{
    void *_ptr;

    __asm__ __volatile__ ("leaq (%rip), %0\n"
                          : "=r" (_ptr));
    *pp = _ptr;

    return;
}

INLINE void
m_getpc3(void **pp)
{
    void *_ptr;

    __asm__ __volatile__ ("movq $., %0\n"
                          : "=r" (_ptr));
    *pp = _ptr;

    return;
}
