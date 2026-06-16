#ifndef FADO_ASSET_FORMAT_H
#define FADO_ASSET_FORMAT_H

// Copy pasting these typedefs to avoid including anything from fado engine code here.
// ─────────────────────────────────────────────
typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef float f32;
typedef double f64;

typedef i32 bool32;

// ─────────────────────────────────────────────

#define internal static

#pragma pack(push, 1)

// Every .fasset file starts with this.
struct FAssetHeader
{
    u32 magic;
    u32 assetType;
    u32 version;
    u32 reserved;
};

#define FASSET_MAGIC   0x31534146 // "FAS1"
#define FASSET_VERSION 1

#define FASSET_TYPE_IMAGE 0
#define FASSET_TYPE_MODEL 1

// ────────────────────────────────────────────────────────────────────────
// ---- Image payload (assetType == FASSET_TYPE_IMAGE) ----

struct FImageHeader
{
    u32 width;
    u32 height;
    u32 channels;
    u32 dataSize;
    u32 format;     // FIM_FORMAT_RGBA or FIM_FORMAT_BC3
    u32 mipCount;
};

// ────────────────────────────────────────────────────────────────────────

#define FASSET_MAX_NAME 64
#define FASSET_MAX_MIPS 16

#define FIMAGE_FORMAT_RGBA 0
#define FIMAGE_FORMAT_BC3  1

#pragma pack(pop)

#endif  // FADO_ASSET_FORMAT_H