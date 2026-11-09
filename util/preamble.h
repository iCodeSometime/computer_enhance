#ifndef PREAMBLE_H
#define PREAMBLE_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t uptr;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define GB(count) 1024ULL * MB(count)
#define MB(count) 1024ULL * KB(count)
#define KB(count) 1024ULL * count

static inline uptr align_up(uptr value, uptr alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

#endif

