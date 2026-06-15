// fado_image_format.h — shared between converter tool and engine runtime

#ifndef FADO_IMAGE_FORMAT_H
#define FADO_IMAGE_FORMAT_H

typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int i32;


#pragma pack(push, 1)
struct FIMHeader
{
	u32 magic;		// 'FIM1' = 0x314D4946 (version-tagged for future changes)
	u32 width;
	u32 height;
	u32 channels;	// 4 = RGBA, 1 = grayscale, etc.
	u32 dataSize;     // total compressed bytes for all mips combined
	u32 format;       // FIM_FORMAT_RGBA or FIM_FORMAT_BC3
	u32 mipCount;     // number of mip levels stored
};
#pragma pack(pop)

#define FIM_FORMAT_RGBA  0   // raw uncompressed
#define FIM_FORMAT_BC3   1   // DXT5
#define FIM_MAGIC 0x314D4949 // "FIM1" little-endian

#endif // FADO_IMAGE_FORMAT_H