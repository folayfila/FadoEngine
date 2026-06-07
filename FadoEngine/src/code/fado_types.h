// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_TYPES_H
#define FADO_TYPES_H

#include <stdint.h>
/************** Types ***************/
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef wchar_t wchar;

typedef i32 bool32;

/**************************************/

#define internal static
#define global_variable static
#define local_presist static

/************** Compilers ***************/
#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif	// COMPILER_MSVC

#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif	// COMPILER_LLVM

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif	// _MSC_VER
#endif	// COMPILER_MSVC && !COMPILER_LLVM

#if COMPILER_MSVC
#include <intrin.h>
#endif // COMPILER_MSVC
/**************************************/


/*
* Build Options:
** FADO_DEBUG
*   0 - Build for public release.
*   1 - Build for developer only.

* ** FADO_RELEASE
*   1 - Build for public release.
*/

#if 1
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif  // FADO_DEBUG

#define Pi32 3.141459265359f

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define ZeroStruct(Struct) memset((Struct), 0, sizeof(*(Struct)))

//////////////////////////////////////////

struct v2
{
    f32 x, y;
};

struct v3
{
    union
    {
        struct
        {
            f32 x, y, z;
        };

        struct
        {
            f32 r, g, b;
        };
    };

    inline v3& operator+=(v3 a)
    {
        x += a.x;
        y += a.y;
        z += a.z;
        return *this;
    }
    inline v3& operator-=(v3 a)
    {
        x -= a.x;
        y -= a.y;
        z -= a.z;
        return *this;
    }
};

struct v4
{
    union
    {
        struct
        {
            f32 x, y, z, w;
        };
        struct
        {
            f32 r, g, b, a;
        };
        f32 e[4];
    };
};
typedef v4 quat;

struct matrix
{
    f32 m[16];
};

/////////////////// Transform ///////////////////////
#define MAX_TRANSFORMS 1024
#define INVALID_TRANSFORM 0xFFFFFFFF

typedef u32 HTransform; // Transform handle.

struct FTransformTable
{
    v3 positions[MAX_TRANSFORMS];
    v3 scales[MAX_TRANSFORMS];
    quat rotations[MAX_TRANSFORMS];
    u32 count;
};

/////////////////// Arena ///////////////////////

struct FMemoryArena
{
    u32 used;
    u32 size;
    u8* base;
};

struct FEngineMemory
{
    // Permanent — lives for the entire session.
    // Scratch — reset every asset load (or every frame for temp work).
    FMemoryArena permanent;
    FMemoryArena scratch;
};

#define ArenaPushSize(Arena, type) (type *)AreaPushSize_(Arena, sizeof(type))
#define ArenaPushArray(Arena, Count, type) (type *)AreaPushSize_(Arena, (Count)*sizeof(type))
internal void* AreaPushSize_(FMemoryArena* arena, u32 size)
{
    Assert((arena->used + size) <= arena->size);
    void* result = arena->base + arena->used;
    arena->used += size;

    return result;
}

internal FMemoryArena ArenaMake(u8* backing, u32 size)
{
    FMemoryArena arena = {};
    arena.base = backing;
    arena.size = size;
    return arena;
}

inline void ArenaReset(FMemoryArena* arena)
{
    arena->used = 0;
}
////////////////////////////////////////////////

#endif // FADO_TYPES_H