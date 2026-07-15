// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_TYPES_H
#define FADO_TYPES_H

// ───────────────────────────────────────────── 
//>>Build options:
//  FADO_DEBUG
//   0 - Build for public release.
//   1 - Build for developer only.
#define FADO_DEBUG 1

#if FADO_DEBUG
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif  // FADO_DEBUG

/*
 * General Info:
   - Files are prefixed with fado_, e.g. "fado_types.h".
   
   - Custom typed are prefixed with F (Fado), e.g. "FCustomStruct".
   
   - All types that are typed like "HSomething" are just u32 handles.
   
   - This file must be included in all engine code, the only exceptions are tools and imported files.
   
   - The engine is divided into 2 parts, the engine (.exe) which currently includes the engine and renderer code,
     and the game (.dll) which includes the game code and collision implementation.
  	This structure allows for hot reloading and seperation between the game and engine such that different games can
  	just be imported into the game code and work out of the box.

   - Wrap all ImGui code with FADO_DEBUG.
*/

// ─────────────────────────────────────────────

// ─────────────────────────────────────────────
typedef signed char i8;
typedef short		i16;
typedef int			i32;
typedef long long	i64;

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long  u64;

typedef float f32;
typedef double f64;

typedef bool b8;
typedef i32 b32;

typedef char c8;
typedef const char cc8;
typedef wchar_t wchar;

// ─────────────────────────────────────────────

#define internal static
#define global_variable static
#define local_presist static

// ─────────────────────────────────────────────

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define ForceInline __forceinline

#define FMAX_PATH 128
#define FMAX_NAME 64

// ─────────────────────────────────────────────
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <string.h>

ForceInline void fmemset(void* dst, i32 value, u64 size)
{
	memset(dst, value, size);
}

ForceInline void fmemmove(void* dst, const void* src, u64 size)
{
	memmove(dst, src, size);
}

ForceInline void fmemcpy(void* dst, const void* src, u64 size)
{
	memcpy(dst, src, size);
}

#define FadoZeroStruct(Struct) fmemset((Struct), 0, sizeof(*(Struct)))
#define FadoZeroArray(Array) fmemset((Array), 0, sizeof((Array)))
#define FadoZeroMemory(Memory, Size) fmemset((Memory), 0, Size)

// ─────────────────────────────────────────────
/*
 * Vectors and matrices are defined here but all
 * operator overloads except '=' and functions are in "fado_math.h".
*/

// 2D Vector
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

// 3D Vector
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
};

// 4D Vector
struct v4
{
	union
	{
		struct { f32 x, y, z, w; };				// normal v4
		struct { f32 r, g, b, a; };				// rgba color
		struct { f32 x, y, width, height; };	// rect
		struct { f32 u, v, width, height; };	// sprite rect
		struct { f32 u0, v0, u1, v1; };			// uv coords
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

// Just a good old v4 :)
typedef v4 quat;

// ─────────────────────────────────────────────
// 3x3 Matrix
struct mat3
{
	union
	{
		f32 m[9];
		f32 e[3][3]; // e[row][col], row-major
	};
};

// 4x4 Matrix
struct mat4
{
	union
	{
		f32 m[16];
		f32 e[4][4]; // e[row][col], row-major
	};
};

// ─────────────────────────────────────────────

// ──────────────── Arena ──────────────────────

#define PERMANENT_ARENA_SIZE Megabytes(64)
#define SCRATCH_ARENA_SIZE Megabytes(15)
#define LEVEL_ARENA_SIZE Megabytes(1)

// Preallocated memory block. Usually allocated on startup and used across the code.
struct FMemoryArena
{
	u32 used;
	u32 size;
	u8* base;
};

// Permanent — lives for the entire session.
// Scratch   — resets every asset load.
// Level     - levels only. permanaent through out the game. Allocated on startup, reused in levels.
struct FEngineMemory
{
	FMemoryArena permanent;
	FMemoryArena scratch;   // MUST reset manually after using with ArenaReset()
	FMemoryArena level;
};

// Saves a type with a size (can be different) into the arena.
// e.g. ArenaPushSize(arena, u8, size).
#define ArenaPushSize(Arena, type, size) (type *)AreaPushSize_(Arena, size)

// Saves a certain type with its size into the arena.
// e.g. ArenaPushSize(arena, FTransformTable).
#define ArenaPushType(Arena, type) (type *)AreaPushSize_(Arena, sizeof(type))

// Saves an array into the arena by passing its cound and type.
// e.g. ArenaPushArray(arena, maxEntities, FEntity) 
#define ArenaPushArray(Arena, type, Count) (type *)AreaPushSize_(Arena, (Count)*sizeof(type))

internal void* AreaPushSize_(FMemoryArena* arena, u32 size)
{
	Assert((arena->used + size) <= arena->size);
	void* result = arena->base + arena->used;
	arena->used += size;

	return result;
}

// Create an arena using a preallocated memory block.
// - backing: the allocated memory.
// - size:	size of the arena.
internal FMemoryArena ArenaMake(u8* backing, u32 size)
{
	FMemoryArena arena = {};
	arena.base = backing;
	arena.size = size;
	return arena;
}

internal void ArenaReset(FMemoryArena* arena)
{
	arena->used = 0;
}
// ─────────────────────────────────────────────

#define INVALID_HANDLE 0xFFFFFFFF

#define FMAX_ENTITIES 512
#define FMAX_MESHES 265
#define FMAX_TEXTURES 265

// Handles
typedef u32 HEntity;
typedef u32 HMesh;
typedef u32 HTexture;
typedef u32 HSound;
typedef u32 HParticle;
typedef u32 HSpriteSheet;

// ────────────────
// Material
// ────────────────

enum EMaterialFlags : u8
{
	Material_None		 = 0,
	Material_Lit		 = 1 << 0,	// Whether the material calculates light or not.
	Material_Transparent = 1 << 1,	// While we use the alpha channel in the color for transparency, this allows us to transparent blend alpha channels in texture for sprites.
	Material_CastShadow  = 1 << 2,	// Whether the material casts a shadow blob.
};

// Used to draw entities.
// A material can be based on a loaded texture, or can be an rgb color.
// If the texture is valid, the color is applied as tint to it.
struct FMaterial
{
	v4 color;			// Used as the main texture or applied as tint to a texture.
	HTexture texture;   // hWhiteTexture = no texture (color)
	u8 flags;			// Material flags
};

// Global directional light.
struct FDirectionalLight
{
	v4 ambientColor;
	v4 diffuseColor;
	v3 lightDirection;
};


// ────────────────
// Animation State
// Runtime animation state — per entity
// ────────────────
struct FAnimState
{
	HSpriteSheet hSheet;
	u32 currentClip;
	u32 currentFrame;
	f32 timer;          // counts up to 1/fps
};

// ────────────────
// Entities
// ────────────────
struct FEntity
{
    HMesh hMesh;				// hQuad for 2D
	FMaterial material;			// texture, color, alpha

	v4 spriteRect;				// UV region, {0,0,1,1} for non-atlas
	FAnimState animState;		// only used for sprites.
};

struct FEntityTable
{
	FEntity entities[FMAX_ENTITIES];
	u32 count;
};

// ────────────────
// Transform 
// Transfomrs use the same handle as the entity. An entity has a transform anyway.
// ────────────────
struct FTransforms
{
	v3 positions[FMAX_ENTITIES];
	v3 scales[FMAX_ENTITIES];
	quat rotations[FMAX_ENTITIES];
};

// ─────────────────────────────────────────────


// ──────────────── Camera and Viewport ───────────────
enum ECameraType
{
	Camera_Perspective,
	Camera_Orthographic
};

// ────────────────
// FCamera
// Check the struct for details.
// ────────────────
struct FCamera
{
	HEntity handle;	// camera entity handle
	ECameraType type;

	v3 forward, up, right;

	mat4 view;
	mat4 projection;

	// Perspective attributes
	f32 fovY;       // vertical FOV in radians
	f32 aspect;		// aspect ratio

	// Orthographic attributes
	f32 orthoWidth;
	f32 orthoHeight;

	// Shared
	f32 nearZ;		// screen near
	f32 farZ;		// screen depth
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

// ─────────────────────────────────────────────

// ──────────────── Fonts ───────────────

// ────────────────
// Font glyphs are presented as textures on the screen,
// each glyph is merely a part of the font file captured by the
// uv coords.
// ────────────────
struct FFontGlyph
{
	v4 coords;
	i32 width, height;
	v2 offset;
	f32 xadvance;
};

#define GLYPHS_COUNT 96

// ────────────────
// Font struct:
// Currently ASCII 32-127 only
// - size: Font size
// ────────────────
struct FFont
{
	HTexture atlas;
	FFontGlyph glyphs[GLYPHS_COUNT];
	f32 size;
};

// ─────────────────────────────────────────────

#endif // FADO_TYPES_H