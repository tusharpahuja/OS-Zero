#ifndef __ARM_SETJMP_H__
#define __ARM_SETJMP_H__

#include <stddef.h>
#include <stdint.h>
#include <signal.h>

#include <zero/cdecl.h>

#if 0 /* ARMv6-M */

/* THANKS to Kazu Hirata for putting this code online :) */

struct _jmpbuf {
    int32_t r4;
    int32_t r5;
    int32_t r6;
    int32_t r7;
    int32_t r8;
    int32_t r9;
    int32_t r10;
    int32_t fp;
    int32_t sp;
    int32_t lr;
#if (_POSIX_SOURCE)
    sigset_t sigmask;
#elif (_BSD_SOURCE)
    int      sigmask;
#endif
} PACK();

#define __setjmp(env)                                                   \
    __asm__ __volatile__ ("mov r0, %0\n" : : "r" (env));                \
    __asm__ __volatile__ ("stmia r0!, { r4 - r7 }\n");                  \
    __asm__ __volatile__ ("mov r1, r8\n");                              \
    __asm__ __volatile__ ("mov r2, r9\n");                              \
    __asm__ __volatile__ ("mov r3, r10\n");                             \
    __asm__ __volatile__ ("mov r4, fp\n");                              \
    __asm__ __volatile__ ("mov r5, sp\n");                              \
    __asm__ __volatile__ ("mov r6, lr\n");                              \
    __asm__ __volatile__ ("stmia r0!, { r1 - r6 }\n");                  \
    __asm__ __volatile__ ("sub r0, r0, #40\n");                         \
    __asm__ __volatile__ ("ldmia r0!, { r4, r5, r6, r7 }\n");           \
    __asm__ __volatile__ ("mov r0, #0\n");                              \
    __asm__ __volatile__ ("bx lr\n")

#define __longjmp(env, val)                         			\
    __asm__ __volatile__ ("mov r0, %0\n" : : "r" (env));                \
    __asm__ __volatile__ ("mov r1, %0\n" : : "r" (val));                \
    __asm__ __volatile__ ("add r0, r0, #16\n");                         \
    __asm__ __volatile__ ("ldmia r0!, { r2 - r6 }\n");                  \
    __asm__ __volatile__ ("mov r8, r2\n");                              \
    __asm__ __volatile__ ("mov r9, r3\n");                              \
    __asm__ __volatile__ ("mov r10, r4\n");                             \
    __asm__ __volatile__ ("mov fp, r5\n");                              \
    __asm__ __volatile__ ("mov sp, r6\n");                              \
    __asm__ __volatile__ ("ldmia r0!, { r3 }\n");                       \
    __asm__ __volatile__ ("sub r0, r0, #40\n");                         \
    __asm__ __volatile__ ("ldmia r0!, { r4 - r7 }\n");                  \
    __asm__ __volatile__ ("mov r0, r1\n");                              \
    __asm__ __volatile__ ("moveq r0, #1\n");                            \
    __asm__ __volatile__ ("bx r3\n")

#endif /* 0 */

struct _jmpbuf {
    int32_t r4;
    int32_t r5;
    int32_t r6;
    int32_t r7;
    int32_t r8;
    int32_t r9;
    int32_t r10;
    int32_t fp;
    int32_t sp;
    int32_t lr;
    sigset_t sigmask;
} PACK();

#define __setjmp(env)                                                   \
    __asm__ __volatile__ ("movs r0, %0\n"                               \
                          "stmia r0!, { r4-r10, fp, sp, lr }\n"         \
                          "movs r0, #0\n"                               \
                          :                                             \
                          : "r" (env))

#define __longjmp(env, val)                                             \
    __asm__ __volatile__ ("movs r0, %0\n"                               \
                          "movs r1, %1\n"                               \
                          "ldmia r0!, { r4-r10, fp, sp, lr }\n"         \
                          "movs r0, r1\n"                               \
                          "moveq r0, #1\n"                              \
                          "bx lr\n"                                     \
                          :                                             \
                          : "r" (env), "r" (val))

    
#endif /* __ARM_SETJMP_H__ */

