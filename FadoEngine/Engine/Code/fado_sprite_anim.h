// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_SPRITE_ANIM_H
#define FADO_SPRITE_ANIM_H

#include "fado_types.h"

// ────────────────────────────────────────────────────────────────────────

// A single animation clip (run, idle, attack..etc)
struct FAnimClip
{
	u32 startFrame;		// index into the atlas grid
	u32 frameCount;
	f32 fps;
	b32 loop;			// false = one shot, stays on last frame
};

#define FMAX_ANIM_CLIPS 32

// The atlas definition — one per character/spritesheet
struct FSpriteSheet
{
	HTexture hTex;
	u32	frameWidth;						// in pixels
	u32	frameHeight;					// in pixels
	u32	cols;							// atlas width / frameWidth
	u32	rows;							// atlas height / frameHeight

	FAnimClip clips[FMAX_ANIM_CLIPS];	// named by enum
	u32 clipsCount;
};

#define FMAX_SPRITESHEETS 64

struct FSpriteSheetTable
{
	FSpriteSheet sheets[FMAX_SPRITESHEETS];
	u32 count;
};

// ────────────────────────────────────────────────────────────────────────

inline void AddClip(FSpriteSheet* sheet, u32 startFrame, u32 frameCount, f32 fps, b8 loop)
{
	Assert(sheet->clipsCount < FMAX_ANIM_CLIPS);
	FAnimClip* clip = &sheet->clips[sheet->clipsCount++];
	clip->startFrame = startFrame;
	clip->frameCount = frameCount;
	clip->fps = fps;
	clip->loop = loop;
}

inline void SetClip(FAnimState* anim, u32 clip)
{
	if (anim->currentClip != clip)
	{
		anim->currentClip = clip;
		anim->currentFrame = 0;
		anim->timer = 0.0f;
	}
}

inline void UpdateAnimState(FEntity* entity, FSpriteSheet* sheet, f32 deltaTime)
{
	FAnimState* anim = &entity->animState;
	FAnimClip* clip = &sheet->clips[anim->currentClip];

	// Tick timer
	anim->timer += deltaTime;

	if (anim->timer >= (1.0f / clip->fps))
	{
		anim->timer = 0.0f;
		anim->currentFrame++;

		if (anim->currentFrame >= clip->frameCount)
		{
			if (clip->loop)
			{
				anim->currentFrame = 0;
			}
			else
			{
				anim->currentFrame = clip->frameCount - 1;	// stay on last
			}
		}
	}

	// Compute absolute frame index in atlas
	u32 absFrame = clip->startFrame + anim->currentFrame;

	// Convert to UV rect
	u32 col = absFrame % sheet->cols;
	u32 row = absFrame / sheet->cols;

	f32 frameWidth = 1.0f / (f32)sheet->cols;
	f32 frameHeight = 1.0f / (f32)sheet->rows;

	entity->spriteRect.u = col * frameWidth;
	entity->spriteRect.v = row * frameHeight;
	entity->spriteRect.width = frameWidth;
	entity->spriteRect.height = frameHeight;
}

// ────────────────────────────────────────────────────────────────────────

#endif // FADO_SPRITE_ANIM_H