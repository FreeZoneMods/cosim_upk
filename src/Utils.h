#include "PlatformSpecific.h"
template <typename T>
T ALIGN_DOWN( T x, uint32 align )
{
    return (x & ~(align - 1));
}

template <typename T>
T ALIGN_UP( T x, uint32 align )
{
    return ((x & (align - 1)) ? ALIGN_DOWN(x, align) + align : x);
}
