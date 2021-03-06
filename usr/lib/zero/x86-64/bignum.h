#ifndef __ZERO_X86_64_BIGNUM_H__
#define __ZERO_X86_64_BIGNUM_H__

#if defined(__GNUC__)
#include <quadmath.h>
typedef __float128        f128;
#else
/* let's try */
typedef Quad              f128;
#endif
#if defined(__GNUC__)
typedef __int128          i128;
typedef unsigned __int128 u128;
#else
typedef  __int128_t       i128
typedef  __uint128_t      u128
#endif

u128 addu128(u128 a, u128 b);
u128 mulu128(u128 a, u128 b);

static __inline__ i128
add128(i128 a, i128 b) {

    return a + b;
}

static __inline__ i128
sub128(i128 a, i128 b) {

    return a - b;
}

static __inline__ i128
mul128(i128 a, i128 b) {

    return a * b;
}

static __inline__ i128
div128(i128 a, i128 b) {

    return a / b;
}

#define mod128(a, b) rem128(a, b)

static __inline__ i128
rem128(i128 a, i128 b) {

    return a % b;
}

#endif /* __ZERO_X86_64_BIGNUM_H__ */

