#if defined(ZEROFUTEX)

/*
 * TODO
 * ----
 * - implement linux-like futexes (fast userspace mutexes) for zero
 * - test and fix the functions in this file
 */

#include <features.h>
#include <pthread.h>
#include <zero/asm.h>
#include <zero/futex.h>

#if defined(__linux__)
int
futex(void *adr1, long op, long val1, struct timespec *timeout,
      void *adr2, int val2)
{
    long retval = syscall(SYS_futex, adr1, op, val1, timeout, adr2, val2);

    return (int)retval;
}
#endif /* defined(__linux__) */

long
mutex_init(mutex_t *mutex, const pthread_mutexattr_t *atr)
{
    (void)atr;
    m_membar();
    *mutex = MUTEXUNLOCKED;

    return 0;
}

long
mutex_destroy(mutex_t *mutex)
{
    (void)mutex;
    
    return 0;
}

long
mutex_lock(mutex_t *mutex)
{
    volatile long mtx = MUTEXLOCKED;
    long          l;

    /* spin and try to lock mutex */
    for (l = 0 ; l < 100; l++) {
        mtx = m_cmpswap(mutex, MUTEXUNLOCKED, MUTEXLOCKED);
        if (mtx != MUTEXUNLOCKED) {

            return 0;
        }
        m_waitint();
    }
    /* mutex is now contended */
    if (mtx == MUTEXUNLOCKED) {
        mtx = m_xchg(mutex, MUTEXCONTD);
    }
    while (mtx) {
        futex(mutex, FUTEX_WAIT_PRIVATE, MUTEXCONTD, NULL, NULL, 0);
        mtx = m_xchg(mutex, MUTEXCONTD);
    }
    
    return mtx;
}

long
mutex_unlock(mutex_t *mutex)
{
    volatile long mtx = m_cmpswap(mutex, MUTEXCONTD, MUTEXUNLOCKED);
    long          l;

    if (mtx != MUTEXCONTD
        || m_xchg(mutex, MUTEXUNLOCKED) == MUTEXLOCKED) {
        
        return 0;
    }
    for (l = 0 ; l < 200; l++) {
        if (*mutex != MUTEXUNLOCKED) {
            if (m_cmpswap(mutex, MUTEXLOCKED, MUTEXUNLOCKED)) {

                return 0;
            }
        }
        m_waitint();
    }
    futex(mutex, FUTEX_WAKE_PRIVATE, MUTEXLOCKED, NULL, NULL, 0);

    return mtx;
}

#endif /* ZEROFUTEX */

