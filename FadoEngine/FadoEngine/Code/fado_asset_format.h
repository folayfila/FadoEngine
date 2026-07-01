// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_ASSET_FORMAT_H
#define FADO_ASSET_FORMAT_H

#include "fado_types.h"

#pragma pack(push, 1)

/*
 * Fado Asset Type
   - This file includes structs for all file format that are compressed and
     converted to ".f*asset*".
     Exampless: 
        .wav -> .fsound
        .png -> .fimage
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
#define FASSET_TYPE_SOUND 2
#define FASSET_TYPE_LEVEL 3
#define FASSET_TYPE_SAVE  4

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
struct FFontHeader
{
    u32 dataSize;           // compressed (lz4) size
    u32 uncompressedSize;
    u32 flags;
};

// ────────────────────────────────────────────────────────────────────────
// Sound payload (assetType == FASSET_TYPE_SOUND)
struct FSoundHeader
{
    u32 dataSize;          // compressed (lz4) size
    u32 uncompressedSize;  // raw PCM bytes
    u32 sampleCount;       // per channel
    u32 channels;
    u32 sampleRate;
    u32 flags;
};

// ────────────────────────────────────────────────────────────────────────
// Level payload (assetType == FASSET_TYPE_LEVEL)
struct FLevelHeader
{
    u32 index;              // 0, 1, 2...
    u32 entityCount;
    u32 flags;
};

// What actually gets saved/loaded per entity
struct FEntityDesc
{
    EEntityType  type;
    v3           pos;
    quat         rot;
    v3           scale;
    HMesh        hMesh;
    HTexture     hTexture;
    v4           color;
    EShaderTypes shaderType;
    u32          reserved;
};

// ────────────────────────────────────────────────────────────────────────
// Save(game) payload (assetType == FASSET_TYPE_SAVE)
// Any future games should follow this structrue to save game files/progress.
// We use chunks and just add them insteead of manually increasing one SaveAsset header.

/* Load chunk would look something like this:
* FSaveChunkHeader chunk;
    while (fread(&chunk, sizeof(chunk), 1, f) == 1)
    {
        switch (chunk.chunkId)
        {
            case SAVE_CHUNK_PLAYER:
            {
                FPlayerSave player;
                fread(&player, sizeof(player), 1, f);
                ApplyPlayerSave(gs, &player);
            } break;
        // .. etc

* Writeing would look something like this:
*     #define WRITE_CHUNK(id, ver, data) \
    { \
        FSaveChunkHeader ch = {id, ver, sizeof(data)}; \
        fwrite(&ch,   sizeof(ch),   1, f); \
        fwrite(&data, sizeof(data), 1, f); \
    }

    FPlayerSave playerSave = BuildPlayerSave(gs);
    WRITE_CHUNK(SAVE_CHUNK_PLAYER, 1, playerSave);
*/

// Chunks
#define SAVE_CHUNK_HEADER    0x00000001  // FSaveHeader, always first

struct FSaveChunkHeader
{
    u32 chunkId;   // e.g. SAVE_CHUNK_PLAYER, SAVE_CHUNK_ENTITIES, SAVE_CHUNK_PROGRESS
    u32 version;   // per-chunk version
    u32 byteSize;  // so unknown/old chunks can be skipped
};
// ────────────────────────────────────────────────────────────────────────

#pragma pack(pop)

#endif  // FADO_ASSET_FORMAT_H