// (C) Copyright 2026 by Abdallah Maaliki / folayfila.
#define _CRT_SECURE_NO_WARNINGS

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"
#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"

#include "lz4/lz4.h"

#include "../../Code/fado_asset_format.h"
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
* LZ4 Compression:
* - All assets go through a typical size compression, and get converted to .fasset.
*   That means even files who are not GPU compressed like images, get just a regular compression to save space.
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
// ─────────────────────────────────────────
// -- BC1 --
internal u32 CompressBC1(u8* rgba, u32 width, u32 height, u8* outBuffer)
{
    if (width < 4) { width = 4; }
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
                    if (srcX >= width) { srcX = width - 1; }
                    u32 srcY = by * 4 + py;
                    if (srcY >= height) { srcY = height - 1; }
                    memcpy(&block[(py * 4 + px) * 4], &rgba[(srcY * width + srcX) * 4], 4);
                }
            stb_compress_dxt_block(dst, block, 0, STB_DXT_NORMAL); // 0 = no alpha = BC1
            dst += 8; // BC1 is 8 bytes per block, BC3 is 16
        }
    return (u32)(dst - outBuffer);
}

internal u32 BakeMippedBC1_UpperBound(u32 width, u32 height)
{
    u32 mip0 = ((width + 3) / 4) * ((height + 3) / 4) * 8; // BC1 = 8 bytes per block
    return mip0 * 2 + ComputeMipCount(width, height) * 32;
}

internal u32 BakeMippedBC1(u8* rgba, u32 width, u32 height, u8* dstBuffer,
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

        u32 compSize = CompressBC1(mipPixels, mipWidth, mipHeight, dstBuffer + total);
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

// ─────────────────────────────────────────
// -- BC3 --
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

// Bake functions forwards
bool BakeImage(const char* src, const char* dst);
bool BakeFont(const char* src, const char* dst);
bool BakeSound(const char* src, const char* dst);

// All types here
// >> Important: Increase the count if you add more/new types.
#define ASSET_IMPOSTERS_COUNT 6
AssetImporter importers[ASSET_IMPOSTERS_COUNT] =
{
    { ".png",  BakeImage },
    { ".jpg",  BakeImage },
    { ".jpeg", BakeImage },
    { ".tga",  BakeImage },
    { ".ttf",  BakeFont  },
    { ".wav",  BakeSound }
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

    bool hasAlpha = (channels == 4);

    // BC compression first
    u32 bcCapacity = hasAlpha
        ? BakeMippedBC3_UpperBound((u32)width, (u32)height)
        : BakeMippedBC1_UpperBound((u32)width, (u32)height);
    u8* bcData = (u8*)malloc(bcCapacity);

    u32 mipOffsets[FASSET_MAX_MIPS] = {};
    u32 mipSizes[FASSET_MAX_MIPS] = {};
    u32 mipCount = 0;
    u32 bcSize = 0;

    bcSize = hasAlpha
        ? BakeMippedBC3(pixels, (u32)width, (u32)height, bcData, mipOffsets, mipSizes, &mipCount)
        : BakeMippedBC1(pixels, (u32)width, (u32)height, bcData, mipOffsets, mipSizes, &mipCount);

    stbi_image_free(pixels);

    // LZ4 compress the BC data
    i32 lz4Capacity = LZ4_compressBound((i32)bcSize);
    u8* lz4Data = (u8*)malloc(lz4Capacity);
    i32 lz4Size = LZ4_compress_default((const char*)bcData, (char*)lz4Data, (i32)bcSize, lz4Capacity);

    free(bcData);

    if (lz4Size <= 0)
    {
        printf("lz4 compression failed for %s\n", src);
        free(lz4Data);
        return false;
    }

    FAssetHeader header = {};
    header.magic = FASSET_MAGIC;
    header.assetType = FASSET_TYPE_IMAGE;
    header.version = FASSET_VERSION;
    header.reserved = 0;

    FImageHeader imageHeader = {};
    imageHeader.width = (u32)width;
    imageHeader.height = (u32)height;
    imageHeader.channels = (u32)channels;
    imageHeader.dataSize = (u32)lz4Size;
    imageHeader.uncompressedSize = bcSize;
    imageHeader.format = hasAlpha ? FIMAGE_FORMAT_BC3 : FIMAGE_FORMAT_BC1;
    imageHeader.mipCount = mipCount;
    imageHeader.flags = FASSET_FLAG_LZ4;

    FILE* out = fopen(dst, "wb");
    fwrite(&header, sizeof(header), 1, out);
    fwrite(&imageHeader, sizeof(imageHeader), 1, out);
    fwrite(mipOffsets, sizeof(u32), mipCount, out);
    fwrite(mipSizes, sizeof(u32), mipCount, out);
    fwrite(lz4Data, lz4Size, 1, out);
    fclose(out);

    free(lz4Data);

    printf("wrote %s (%dx%d, %u mips, %s) %.2fMB -> BC %.2fMB -> LZ4 %.2fMB\n",
        dst, width, height, mipCount,
        hasAlpha ? "BC3" : "BC1",
        (width * height * 4) / (1024.0f * 1024.0f),
        bcSize / (1024.0f * 1024.0f),
        lz4Size / (1024.0f * 1024.0f));

    return true;
}

// -- Fonts --
bool BakeFont(const char* src, const char* dst)
{
    FILE* in = fopen(src, "rb");
    if (!in)
    {
        printf("failed to load %s\n", src);
        return false;
    }

    fseek(in, 0, SEEK_END);
    u32 dataSize = (u32)ftell(in);
    fseek(in, 0, SEEK_SET);

    u8* fontData = (u8*)malloc(dataSize);
    fread(fontData, 1, dataSize, in);
    fclose(in);

    // LZ4 compress
    i32 lz4Capacity = LZ4_compressBound((i32)dataSize);
    u8* lz4Data = (u8*)malloc(lz4Capacity);
    i32 lz4Size = LZ4_compress_default((const char*)fontData, (char*)lz4Data, (i32)dataSize, lz4Capacity);

    free(fontData);

    if (lz4Size <= 0)
    {
        printf("lz4 compression failed for %s\n", src);
        free(lz4Data);
        return false;
    }

    FAssetHeader header = {};
    header.magic = FASSET_MAGIC;
    header.assetType = FASSET_TYPE_FONT;
    header.version = FASSET_VERSION;
    header.reserved = 0;

    FFontAssetHeader fontHeader = {};
    fontHeader.dataSize = (u32)lz4Size;
    fontHeader.uncompressedSize = dataSize;
    fontHeader.flags = FASSET_FLAG_LZ4;

    FILE* out = fopen(dst, "wb");
    fwrite(&header, sizeof(header), 1, out);
    fwrite(&fontHeader, sizeof(fontHeader), 1, out);
    fwrite(lz4Data, lz4Size, 1, out);
    fclose(out);

    free(lz4Data);

    printf("wrote %s (font) %.2fKB -> LZ4 %.2fKB\n",
        dst,
        dataSize / 1024.0f,
        lz4Size / 1024.0f);

    return true;
}

// -- Sound --
bool BakeSound(const char* src, const char* dst)
{
    FILE* file = fopen(src, "rb");
    if (!file)
    {
        printf("failed to load %s\n", src);
        return false;
    }

    // -- parse WAV header minimally --
    // WAV: "RIFF" -> chunk size -> "WAVE" -> "format " -> "data"
    u32 riff, fileSize, wave;
    fread(&riff, 4, 1, file); // "RIFF"
    fread(&fileSize, 4, 1, file);
    fread(&wave, 4, 1, file); // "WAVE"

    // format chunk
    u32 formatId, formatSize;
    u16 audioformat, channels;
    u32 sampleRate, byteRate;
    u16 blockAlign, bitsPerSample;
    fread(&formatId, 4, 1, file);
    fread(&formatSize, 4, 1, file);
    fread(&audioformat, 2, 1, file); // 1 = PCM
    fread(&channels, 2, 1, file);
    fread(&sampleRate, 4, 1, file);
    fread(&byteRate, 4, 1, file);
    fread(&blockAlign, 2, 1, file);
    fread(&bitsPerSample, 2, 1, file);

    Assert(audioformat == 1);       // must be PCM
    Assert(bitsPerSample == 16);    // we only handle i16

    // skip any extra format bytes
    if (formatSize > 16)
    {
        fseek(file, formatSize - 16, SEEK_CUR);
    }

    // find data chunk (skip non-data chunks)
    u32 chunkId, chunkSize;
    while (true)
    {
        fread(&chunkId, 4, 1, file);
        fread(&chunkSize, 4, 1, file);
        if (chunkId == 0x61746164/*"data"*/)
        {
            break;
        }
        fseek(file, chunkSize, SEEK_CUR);
    }

    u8* pcmData = (u8*)malloc(chunkSize);
    fread(pcmData, 1, chunkSize, file);
    fclose(file);

    u32 sampleCount = chunkSize / (channels * sizeof(i16));

    // LZ4 compress
    i32 lz4Capacity = LZ4_compressBound((i32)chunkSize);
    u8* lz4Data = (u8*)malloc(lz4Capacity);
    i32 lz4Size = LZ4_compress_default((const char*)pcmData, (char*)lz4Data, (i32)chunkSize, lz4Capacity);
    free(pcmData);

    if (lz4Size <= 0)
    {
        printf("lz4 failed for %s\n", src);
        free(lz4Data);
        return false;
    }

    FAssetHeader header = {};
    header.magic = FASSET_MAGIC;
    header.assetType = FASSET_TYPE_SOUND;
    header.version = FASSET_VERSION;

    FSoundAssetHeader sndHeader = {};
    sndHeader.dataSize = (u32)lz4Size;
    sndHeader.uncompressedSize = chunkSize;
    sndHeader.sampleCount = sampleCount;
    sndHeader.channels = channels;
    sndHeader.sampleRate = sampleRate;
    sndHeader.flags = FASSET_FLAG_LZ4;

    FILE* out = fopen(dst, "wb");
    fwrite(&header, sizeof(header), 1, out);
    fwrite(&sndHeader, sizeof(sndHeader), 1, out);
    fwrite(lz4Data, lz4Size, 1, out);
    fclose(out);
    free(lz4Data);

    printf("wrote %s (sound) %.2fKB -> LZ4 %.2fKB | %dch %dHz\n",
        dst, chunkSize / 1024.0f, lz4Size / 1024.0f, channels, sampleRate);

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
        //printf("skipped %s (up to date)\n", argv[1]);
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