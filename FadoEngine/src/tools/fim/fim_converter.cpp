// fim_converter.cpp — standalone tool, not part of the engine build

#define _CRT_SECURE_NO_WARNINGS

#define internal static
// fim_converter.cpp
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"
#include "fado_image_format.h"
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <math.h>

internal bool SourceIsNewer(const char* srcPath, const char* outPath)
{
    struct _stat srcStat, outStat;
    if (_stat(outPath, &outStat) != 0) { return true; }
    if (_stat(srcPath, &srcStat) != 0) { return true; }
    return srcStat.st_mtime > outStat.st_mtime;
}

internal u32 ComputeMipCount(u32 width, u32 height)
{
    u32 count = 1;
    while (width > 1 || height > 1)
    {
        if (width > 1) { width >>= 1; }
        if (height > 1) { height >>= 1; }
        ++count;
    }
    return count;
}

internal u32 CompressBC3(u8* rgba, u32 width, u32 height, u8* outBuffer)
{
    // Clamp to minimum BC3 block size.
    if (width < 4) { width = 4; }
    if (height < 4) { height = 4; }

    u32 blocksX = (width + 3) / 4;
    u32 blocksY = (height + 3) / 4;

    u8* dst = outBuffer;
    for (u32 by = 0; by < blocksY; ++by)
    {
        for (u32 bx = 0; bx < blocksX; ++bx)
        {
            u8 block[64] = {};
            for (u32 py = 0; py < 4; ++py)
            {
                for (u32 px = 0; px < 4; ++px)
                {
                    u32 srcX = bx * 4 + px;
                    u32 srcY = by * 4 + py;
                    if (srcX >= width) { srcX = width - 1; }
                    if (srcY >= height) { srcY = height - 1; }
                    u32 srcIdx = (srcY * width + srcX) * 4;
                    u32 dstIdx = (py * 4 + px) * 4;
                    block[dstIdx + 0] = rgba[srcIdx + 0];
                    block[dstIdx + 1] = rgba[srcIdx + 1];
                    block[dstIdx + 2] = rgba[srcIdx + 2];
                    block[dstIdx + 3] = rgba[srcIdx + 3];
                }
            }
            stb_compress_dxt_block(dst, block, 1, STB_DXT_NORMAL);
            dst += 16;
        }
    }
    return (u32)(dst - outBuffer);
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("usage: fim_converter input.png output.fim\n");
        return 1;
    }

    if (!SourceIsNewer(argv[1], argv[2]))
    {
        printf("skipped %s (up to date)\n", argv[1]);
        return 0;
    }

    i32 width, height, channels;
    u8* pixels = stbi_load(argv[1], &width, &height, &channels, 4);
    if (!pixels)
    {
        printf("failed to load %s\n", argv[1]);
        return 1;
    }

    u32 mipCount = ComputeMipCount((u32)width, (u32)height);

    // --- Build all mip levels and compress each to BC3 ---
    // Allocate worst-case output: sum of all mip levels compressed.
    // Each mip is at most (w/4 * h/4 * 16 bytes). Total is < 2x mip0 size.
    u32 mip0CompressedSize = ((width + 3) / 4) * ((height + 3) / 4) * 16;
    u32 maxTotalSize = mip0CompressedSize * 2 + (mipCount * 64); // generous upper bound
    u8* allMipData = (u8*)malloc(maxTotalSize);

    // Per-mip offset table so the runtime knows where each mip starts.
    u32* mipOffsets = (u32*)malloc(mipCount * sizeof(u32));
    u32* mipSizes = (u32*)malloc(mipCount * sizeof(u32));

    u32 totalCompressedSize = 0;
    u32 mipWidth = (u32)width;
    u32 mipHeight = (u32)height;
    u8* prevMip = pixels;
    u8* prevAlloc = nullptr;

    for (u32 mip = 0; mip < mipCount; ++mip)
    {
        u8* mipPixels = nullptr;

        if (mip == 0)
        {
            mipPixels = pixels; // mip 0 = original
        }
        else
        {
            // Downsample from previous mip using high-quality Lanczos filter.
            mipPixels = (u8*)malloc(mipWidth * mipHeight * 4);
            stbir_resize_uint8_srgb(
                prevMip, (i32)(mip == 1 ? (u32)width : mipWidth << 1),
                (i32)(mip == 1 ? (u32)height : mipHeight << 1),
                0,
                mipPixels, (i32)mipWidth, (i32)mipHeight, 0,
                STBIR_RGBA
            );

            if (prevAlloc) { free(prevAlloc); }
            prevAlloc = mipPixels;
        }

        // Compress this mip level to BC3.
        u32 compMipWidth = mipWidth < 4 ? 4 : mipWidth;
        u32 compMipHeight = mipHeight < 4 ? 4 : mipHeight;
        u32 blockBytes = ((compMipWidth + 3) / 4) * ((compMipHeight + 3) / 4) * 16;
        u8* compBuf = (u8*)malloc(blockBytes);

        u32 compSize = CompressBC3(mipPixels, mipWidth, mipHeight, compBuf);

        mipOffsets[mip] = totalCompressedSize;
        mipSizes[mip] = compSize;

        memcpy(allMipData + totalCompressedSize, compBuf, compSize);
        totalCompressedSize += compSize;

        free(compBuf);
        prevMip = mipPixels;

        // Step down for next mip.
        if (mipWidth > 1) { mipWidth >>= 1; }
        if (mipHeight > 1) { mipHeight >>= 1; }
    }

    if (prevAlloc) { free(prevAlloc); }
    stbi_image_free(pixels);

    // --- Write .fim file ---
    // Header + mip offset table + mip size table + compressed data.
    FIMHeader header = {};
    header.magic = FIM_MAGIC;
    header.width = (u32)width;
    header.height = (u32)height;
    header.channels = 4;
    header.format = FIM_FORMAT_BC3;
    header.mipCount = mipCount;
    header.dataSize = totalCompressedSize;

    FILE* out = fopen(argv[2], "wb");
    fwrite(&header, sizeof(header), 1, out);
    fwrite(mipOffsets, sizeof(u32), mipCount, out);
    fwrite(mipSizes, sizeof(u32), mipCount, out);
    fwrite(allMipData, totalCompressedSize, 1, out);
    fclose(out);

    free(allMipData);
    free(mipOffsets);
    free(mipSizes);

    printf("wrote %s (%dx%d, %d mips) %.2fMB -> %.2fMB BC3\n",
        argv[2], width, height, mipCount,
        (float)(width * height * 4) / (1024.0f * 1024.0f),
        (float)totalCompressedSize / (1024.0f * 1024.0f));

    return 0;
}