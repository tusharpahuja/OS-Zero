#ifndef __BITS_MATH_H__
#define __BITS_MATH_H__

#include <endian.h>

#define HUGE_VAL    0x7ff0000000000000.0
#define HUGE_VALF   0x7f800000F
#define HUGE_VALL   HUGE_VAL

#define NAN         0x7fc00000f

#define INFINITY    HUGE_VALF
#define FP_ILOGB0   (-2147483647)
#define FP_ILOGBNAN 2147483647

#endif /* __BITS_MATH_H__ */
