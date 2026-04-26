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

struct vec2
{
    f32 x, y;
};

struct vec3
{
    f32 x, y, z;
};

struct color_rgba
{
    f32 r;
    f32 g;
    f32 b;
    f32 a;

    void Set(f32 Red, f32 Green, f32 Blue, f32 alpha = 1.0f)
    {
        r = Red;
        g = Green;
        b = Blue;
        a = alpha;
    }

};

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

#if FADO_DEBUG
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif  // FADO_DEBUG

#define Pi32 3.141459265359f

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#endif // FADO_TYPES_H