// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_UI_H
#define FADO_UI_H

#include "fado_types.h"
#include "fado_math.h"

enum EUICommandType
{
	Rect,
	Text
};

struct FUIRectCommand
{
	v4 rect;
	v4 coords;
	v4 color;
	HTexture hTexture;
};

#define MAX_UI_TEXT 256
struct FUITextCommand
{
	v2 pos;
	v4 color;
	char text[MAX_UI_TEXT];
};

struct FUICommand
{
	EUICommandType type;
	union
	{
		FUIRectCommand rect;
		FUITextCommand text;
	};
};

#define MAX_UI_COMMANDS 256

struct FUICommandBucket
{
	FUICommand commands[MAX_UI_COMMANDS];
	u32 count;
};

struct FUIButtonStyle
{
	v4 idleColor;
	v4 hoverColor;
	v4 pressedColor;
	v4 textColor;
	HTexture texture;
};

struct FUINavState
{
	i32 focusedIndex;       // which button is currently selected
	i32 buttonCount;        // how many buttons were submitted this frame
};

// ─────────────────────────────────────────────
//  Public API
// ────────────────────────────────────────────

inline void UIPushRect(FUICommandBucket* bucket, v4 rect, v4 coords, v4 color, HTexture hTexture)
{
	Assert(bucket->count < MAX_UI_COMMANDS);
	FUICommand* cmd = &bucket->commands[bucket->count++];
	cmd->type = EUICommandType::Rect;
	cmd->rect = { rect, coords, color, hTexture };
}

inline void UIPushText(FUICommandBucket* bucket, FFont* font, cc8* text, v2 pos, v4 color)
{
	v2 cursor = pos;

	for (cc8* p = text; *p; ++p)
	{
		if (*p < 32 || *p > 127)
		{
			continue;
		}
		FFontGlyph* glyph = &font->glyphs[*p - 32];

		v4 rect = {
			cursor.x + glyph->offset.x,
			cursor.y + glyph->offset.y,
			(f32)glyph->width,
			(f32)glyph->height
		};
		v4 coords = glyph->coords;

		UIPushRect(bucket, rect, coords, color, font->atlasTexture);
		cursor.x += glyph->xadvance;
	}
}

inline b8 UIPointInRect(v2 mPos, v4 rect)
{
	b8 result = (mPos.x >= rect.x) && (mPos.x <= rect.x + rect.width) &&
					(mPos.y >= rect.y) && (mPos.y <= rect.y + rect.height);
	return result;
}

inline void UINavigateNext(FUINavState* nav, b8 wrap = true)
{
	i32 newIndex = nav->focusedIndex + 1;
	if (newIndex >= nav->buttonCount)
	{
		if (wrap)
		{
			nav->focusedIndex = 0;
		}
	}
	else
	{
		nav->focusedIndex = newIndex;
	}
}

inline void UINavigateBack(FUINavState* nav, b8 wrap = true)
{
	i32 newIndex = nav->focusedIndex - 1;
	if (newIndex < 0)
	{
		if (wrap)
		{
			nav->focusedIndex = nav->buttonCount - 1;
		}
	}
	else
	{
		nav->focusedIndex = newIndex;
	}
}

inline void UIGuiPushText(FUICommandBucket* bucket, v2 pos, v4 color, cc8* text)
{
	Assert(bucket->count < MAX_UI_COMMANDS);
	FUICommand* cmd = &bucket->commands[bucket->count++];
	cmd->type = EUICommandType::Text;
	cmd->text.pos = pos;
	cmd->text.color = color;
	CopyCString(cmd->text.text, text);
}

#endif FADO_UI_H