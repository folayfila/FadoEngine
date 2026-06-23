// (C) Copyright 2026 by Abdallah Maaliki / folayfila.
#define _CRT_SECURE_NO_WARNINGS

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"
#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"

#include "fado_asset_format.h"
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <math.h>

// ──────────────────────────────────────────────────────────────────────────────────────────
/*
* Fado Converter
* - A standalone tool that goes over assets in "FadoEngine\AssetsSource", compresses and converts them
    to the engine's custom type ".fasset".
*
* How to use:
* - Run "compile_fado_converter.bat" in "FadoEngine\Tools\FadoConverter", it should compile the cpp file and 
*   generate an exe and an obj.
* - Run the other bat, "compile_fado_converter.bat" also in "FadoEngine\Tools\FadoConverter".
*   The bat will go over all files in the subfolders of "FadoEngine\AssetsSource", check if they have an
*   implemented importer/baker, runs the code which converts them into .fasset placed in the same subfolders but in "FadoEngine\Assets".
*   Skips over undefined types and files assets that weren't updated since the last bake.
*   For more on the bakers, check the section "Asset Pipeline Dispatcher" below.
* 
*/
// ──────────────────────────────────────────────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────────────────────────
// Compression functions - Uses stb to compress.

internal u32 ComputeMipCount(u32 width, u32 height)
{
    u32 count = 1;
    while (width > 1 || height > 1)
    {
        if (width > 1)  { width >>= 1; }
        if (height > 1) { height >>= 1; }
        ++count;
    }
    return count;
}

internal u32 CompressBC3(u8* rgba, u32 width, u32 height, u8* outBuffer)
{
    if (width < 4)  { width = 4; }
    if (height < 4) { height = 4; }

    u32 blocksX = (width + 3) / 4;
    u32 blocksY = (height + 3) / 4;

    u8* dst = outBuffer;
    for (u32 by = 0; by < blocksY; ++by)
        for (u32 bx = 0; bx < blocksX; ++bx)
        {
            u8 block[64] = {};
            for (u32 py = 0; py < 4; ++py)
                for (u32 px = 0; px < 4; ++px)
                {
                    u32 srcX = bx * 4 + px;
                    if (srcX >= width)
                    {
                        srcX = width - 1;
                    }

                    u32 srcY = by * 4 + py;
                    if (srcY >= height)
                    {
                        srcY = height - 1;
                    }
                    memcpy(&block[(py * 4 + px) * 4], &rgba[(srcY * width + srcX) * 4], 4);
                }
            stb_compress_dxt_block(dst, block, 1, STB_DXT_NORMAL);
            dst += 16;
        }
    return (u32)(dst - outBuffer);
}

internal u32 BakeMippedBC3_UpperBound(u32 width, u32 height)
{
    u32 mip0 = ((width + 3) / 4) * ((height + 3) / 4) * 16;
    return mip0 * 2 + ComputeMipCount(width, height) * 64;
}

// Generates the full mip chain from rgba (width x height, RGBA8) and BC3-compresses each level back to back into dstBuffer.
// Size dstBuffer with BakeMippedBC3_UpperBound().
internal u32 BakeMippedBC3(u8* rgba, u32 width, u32 height, u8* dstBuffer,
    u32 mipOffsets[FASSET_MAX_MIPS], u32 mipSizes[FASSET_MAX_MIPS],
    u32* outMipCount)
{
    u32 mipCount = ComputeMipCount(width, height);
    if (mipCount > FASSET_MAX_MIPS)
    {
        mipCount = FASSET_MAX_MIPS;
    }

    u32 total = 0;
    u32 mipWidth = width, mipHeight = height;
    u8* prevMip = rgba;
    u8* prevAlloc = nullptr;

    for (u32 mip = 0; mip < mipCount; ++mip)
    {
        u8* mipPixels;
        if (mip == 0)
        {
            mipPixels = rgba;
        }
        else
        {
            mipPixels = (u8*)malloc(mipWidth * mipHeight * 4);
            u32 srcW = (mip == 1) ? width : (mipWidth << 1);
            u32 srcH = (mip == 1) ? height : (mipHeight << 1);
            stbir_resize_uint8_srgb(prevMip, (i32)srcW, (i32)srcH, 0,
                mipPixels, (i32)mipWidth, (i32)mipHeight, 0, STBIR_RGBA);
            if (prevAlloc)
            {
                free(prevAlloc);
            }
            prevAlloc = mipPixels;
        }

        u32 compSize = CompressBC3(mipPixels, mipWidth, mipHeight, dstBuffer + total);
        mipOffsets[mip] = total;
        mipSizes[mip] = compSize;
        total += compSize;

        prevMip = mipPixels;
        if (mipWidth > 1)
        {
            mipWidth >>= 1;
        }
        if (mipHeight > 1)
        {
            mipHeight >>= 1;
        }
    }

    if (prevAlloc)
    {
        free(prevAlloc);
    }
    *outMipCount = mipCount;
    return total;
}

// ────────────────────────────────────────────────────────────────────────
/*
 * Asset Pipeline Dispatcher.
   - Calls the bake function based on the type "extenstion".
   - Each type is manually added to importers with the dispatched function.
*/

typedef bool AssetBakeFn(const char* src, const char* dst);

struct AssetImporter
{
    const char* extension;
    AssetBakeFn* bake;
};

bool BakeImage(const char* src, const char* dst);

// All types here
// >> Important: Increase the count if you add more/new types.
#define ASSET_IMPOSTERS_COUNT 4
AssetImporter importers[ASSET_IMPOSTERS_COUNT] =
{
    { ".png",  BakeImage },
    { ".jpg",  BakeImage },
    { ".jpeg", BakeImage },
    { ".tga",  BakeImage },
    //{ ".glb",  BakeGLB   },
};

AssetImporter* FindImporter(const char* path)
{
    const char* ext = strrchr(path, '.');
    if (!ext)
    {
        return nullptr;
    }

    for (u32 i = 0; i < ASSET_IMPOSTERS_COUNT; ++i)
    {
        if (_stricmp(ext, importers[i].extension) == 0)
        {
            return &importers[i];
        }
    }

    return nullptr;
}

// ────────────────────────────────────────────────────────────────────────
// Imposters functions implementations

// -- Images --
bool BakeImage(const char* src, const char* dst)
{
    i32 width, height, channels;
    u8* pixels = stbi_load(src, &width, &height, &channels, 4);
    if (!pixels)
    {
        printf("failed to load %s\n", src);
        return false;
    }

    // Compress and generate mip chain.
    u32 compressedCapacity = BakeMippedBC3_UpperBound((u32)width, (u32)height);
    u8* compressed = (u8*)malloc(compressedCapacity);

    u32 mipOffsets[FASSET_MAX_MIPS] = {};
    u32 mipSizes[FASSET_MAX_MIPS] = {};
    u32 mipCount = 0;
    u32 compressedSize = BakeMippedBC3(pixels, (u32)width, (u32)height, compressed,
        mipOffsets, mipSizes, &mipCount);

    stbi_image_free(pixels);

    // --- Write .fasset file ---
    // Header + mip offset table + mip size table + compressed data.
    FAssetHeader header = {};
    header.magic = FASSET_MAGIC;
    header.assetType = FASSET_TYPE_IMAGE;
    header.version = FASSET_VERSION;
    header.reserved = 0;

    FImageHeader imageHeader = {};
    imageHeader.width = (u32)width;
    imageHeader.height = (u32)height;
    imageHeader.channels = 4;
    imageHeader.dataSize = compressedSize;
    imageHeader.format = FIMAGE_FORMAT_BC3;
    imageHeader.mipCount = mipCount;

    FILE* out = fopen(dst, "wb");
    fwrite(&header, sizeof(header), 1, out);
    fwrite(&imageHeader, sizeof(imageHeader), 1, out);
    fwrite(mipOffsets, sizeof(u32), mipCount, out);
    fwrite(mipSizes, sizeof(u32), mipCount, out);
    fwrite(compressed, compressedSize, 1, out);
    fclose(out);

    free(compressed);

    printf("wrote %s (%dx%d, %u mips) %.2fMB -> %.2fMB BC3\n",
        dst, width, height, mipCount,
        (width * height * 4) / (1024.0f * 1024.0f),
        compressedSize / (1024.0f * 1024.0f));

    return true;
}

// ────────────────────────────────────────────────────────────────────────

// Checks if the file was updated.
internal bool SourceIsNewer(const char* srcPath, const char* outPath)
{
    struct _stat srcStat, outStat;
    if (_stat(outPath, &outStat) != 0) { return true; }
    if (_stat(srcPath, &srcStat) != 0) { return true; }
    return srcStat.st_mtime > outStat.st_mtime;
}

// ────────────────────────────────────────────────────────────────────────
// ---- fado_converter main ----
int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("usage: fado_converter input.* output.fasset\n");
        return 1;
    }

    if (!SourceIsNewer(argv[1], argv[2]))
    {
        printf("skipped %s (up to date)\n", argv[1]);
        return 0;
    }

    AssetImporter* importer = FindImporter(argv[1]);
    if (!importer)
    {
        printf("unsupported asset type: %s\n", argv[1]);
        return 1;
    }

    if (!importer->bake(argv[1], argv[2]))
    {
        return 1;
    }

    return 0;
}