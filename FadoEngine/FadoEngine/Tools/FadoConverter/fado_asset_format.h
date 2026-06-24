// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

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

typedef i32 b32;

#define internal static
// ─────────────────────────────────────────────

#pragma pack(push, 1)

/*
 * Fado Asset Type
   - This file includes structs for all file format that are compressed and
     converted to ".fasset".
   - TODO: Check if we can have all files be ".fasset".
*/

// Every .fasset file starts with this.
struct FAssetHeader
{
    u32 magic;      // FASSET_MAGIC
    u32 assetType;  // FASSET_TYPE_...
    u32 version;    // FASSET_VERSION
    u32 reserved;   // Reserved for future use. Must be zero now.
};

#define FASSET_MAGIC   0x31534146 // "FAS1"
#define FASSET_VERSION 1

// Types:
#define FASSET_TYPE_IMAGE 0
#define FASSET_TYPE_FONT  1

//
#define FASSET_MAX_MIPS 16

#define FASSET_FLAG_LZ4 (1 << 0)

// ────────────────────────────────────────────────────────────────────────
// Image payload (assetType == FASSET_TYPE_IMAGE)
struct FImageHeader
{
    u32 width;
    u32 height;
    u32 channels;
    u32 dataSize;           // compressed (lz4) size
    u32 uncompressedSize;
    u32 format;             // FIM_FORMAT_RGBA or FIM_FORMAT_BC3
    u32 mipCount;
    u32 flags;
};

#define FIMAGE_FORMAT_BC1  0
#define FIMAGE_FORMAT_BC3  1

// ────────────────────────────────────────────────────────────────────────
// Font payload (assetType == FASSET_TYPE_FONT)
struct FFontAssetHeader
{
    u32 dataSize;           // compressed (lz4) size
    u32 uncompressedSize;
    u32 flags;
};

// ────────────────────────────────────────────────────────────────────────
#pragma pack(pop)

#endif  // FADO_ASSET_FORMAT_H