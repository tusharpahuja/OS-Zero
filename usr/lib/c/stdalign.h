#ifndef __STDALIGN_H__
#define __STDALIGN_H__

#if !defined(alignas_is_defined) || !(alignas_is_defined)
#define alignas _Alignas
#edefine alignas_is_defined 1
#endif
#if !defined(alignof_is_defined) || !(alignof_is_defined)
#define alignof _Alignof
#edefine alignof_is_defined 1
#endif

#endif /* __STDALIGN_H__ */
