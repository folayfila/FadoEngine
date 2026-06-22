// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_TYPES_H
#define FADO_TYPES_H

#include <stdint.h>
// ─────────────────────────────────────────────
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

typedef bool b8;
typedef i32 b32;

typedef char c8;
typedef const char cc8;

// ─────────────────────────────────────────────

#define internal static
#define global_variable static
#define local_presist static

// ─────────────────────────────────────────────
// Compilers
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
// ─────────────────────────────────────────────


/*
* Build Options:
** FADO_DEBUG
*   0 - Build for public release.
*   1 - Build for developer only.
*/
#define FADO_DEBUG 1

#if FADO_DEBUG
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif  // FADO_DEBUG

#define Pi32 3.141459265359f

#define MAX_FLOAT 3.402823466e+38F

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define ZeroStruct(Struct) memset((Struct), 0, sizeof(*(Struct)))

// ─────────────────────────────────────────────


struct v2
{
	union
	{
		struct { f32 x, y; };
		f32 e[2];
	};

	inline v2& operator=(const v2& rhs)
	{
		x = rhs.x;
		y = rhs.y;
		return *this;
	}
};

struct v3
{
	union
	{
		struct { f32 x, y, z; };
		struct { f32 r, g, b; };
		f32 e[3];
	};

	inline v3& operator=(const v3& rhs)
	{
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;
		return *this;
	}

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

	inline v3& operator+=(f32 a)
	{
		x += a;
		y += a;
		z += a;
		return *this;
	}
};

struct v4
{
	union
	{
		struct { f32 x, y, z, w; };
		struct { f32 r, g, b, a; };
		struct { f32 x, y, width, height; };
		struct { f32 u0, v0, u1, v1; };
		f32 e[4];
	};

	inline v4& operator=(const v4& rhs)
	{
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;
		w = rhs.w;
		return *this;
	}
};
typedef v4 quat;

struct mat3
{
	f32 m[9];
};

struct mat4
{
	union
	{
		f32 m[16];
		f32 e[4][4]; // e[row][col], row-major
	};
};

// ─────────────────────────────────────────────
// ──────────────── Transform ──────────────────

typedef u32 HTransform; // Transform handle.

#define MAX_TRANSFORMS 1024
#define INVALID_HANDLE 0xFFFFFFFF

struct FTransformTable
{
    v3 positions[MAX_TRANSFORMS];
    v3 scales[MAX_TRANSFORMS];
    quat rotations[MAX_TRANSFORMS];
    u32 count;
};

// ─────────────────────────────────────────────
// ──────────────── Entities ──────────────────

typedef u32 HEntity;
typedef u32 HMesh;
typedef u32 HTexture;

#define MAX_ENTITIES 1024
#define MAX_MESHES 265
#define MAX_TEXTURES 265

enum EShaderTypes
{
    Shader_None,
    Color,
    UnlitTexture,
    LitTexture
};

struct FEntity
{
    HTransform hTransform;
    HMesh hMesh;
    HTexture hTexture;
    v4 color;
    EShaderTypes shaderType;
};

struct FEntityTable
{
    FEntity entities[MAX_ENTITIES];
    u32 count;
};

// Helpers
inline FEntity* GetEntity(FEntityTable* entityTable, HEntity hEntity)
{
	return &entityTable->entities[hEntity];
}

// ─────────────────────────────────────────────
// ──────────────── Arena ──────────────────────

struct FMemoryArena
{
    u32 used;
    u32 size;
    u8* base;
};

struct FEngineMemory
{
    // Permanent — lives for the entire session.
    // Scratch — resets every asset load.
    FMemoryArena permanent;
    FMemoryArena scratch;   // ! MUST reset manually after using with ArenaReset()
};

#define ArenaPushSize(Arena, type, size) (type *)AreaPushSize_(Arena, size)
#define ArenaPushType(Arena, type) (type *)AreaPushSize_(Arena, sizeof(type))
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
// ─────────────────────────────────────────────

// ─────────────────────────────────────────────
// Strings

inline void CopyCString(char* dst, const char* src)
{
    if (!dst || !src)
    {
        return;
    }

    i32 i = 0;
    for (; src[i] != '\0'; ++i)
    {
        dst[i] = src[i];
    }

    dst[i] = '\0';
}

// ───────────
// Fonts
struct FFontGlyph
{
	v4 coords;
	i32 width, height;
	v2 offset;
	f32 xadvance;
};

#define GLYPHS_COUNT 96
struct FFont
{
	HTexture atlasTexture;
	FFontGlyph glyphs[GLYPHS_COUNT];	// ASCII 32-127
	f32 size;							// font size
};

// ─────────────────────────────────────────────
// ──────────────── Shared Stuff ───────────────
/// FCamera
struct FCamera
{
	HEntity handle;
	v3 forward;
	v3 up;
	v3 right;
	f32 fovY;       // vertical FOV in radians
	f32 aspect;
	f32 nearZ;
	f32 farZ;
};

struct FViewPort
{
	f32 topLeftX;
	f32 topLeftY;
	f32 width;
	f32 height;
	f32 minDepth;
	f32 maxDepth;
};

// Struct containing pointers to stuff that both the renderer and the game could use/access.
struct FSharedStuff
{
	FCamera camera;
	FViewPort viewport;

	FEntityTable* entityTable;
	FTransformTable* transforms;
	struct FCollisionWorld* collisionWorld;
	struct FUICommandBucket* uiCommands;

	FMemoryArena* scratchArena;

	HEntity selectedEntity;
	b32 canSelect;
};



#endif // FADO_TYPES_H