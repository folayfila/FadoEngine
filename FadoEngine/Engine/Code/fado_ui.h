// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_UI_H
#define FADO_UI_H

#include "fado_types.h"
#include "fado_math.h"

// ─────────────────────────────────────────────

// ────────────────
// UICommand to add a rect.
// Rects use the texture they have as a base and apply a color on it,
// this allows us to use plain white textures to display colored rects and any texture
// for stylized rects.
// ────────────────
struct FUICommand
{
	v4 rect;
	v4 coords;
	v4 color;
	HTexture hTexture;
};

#define FMAX_UI_COMMANDS 256

struct FUICommandsBucket
{
	FUICommand commands[FMAX_UI_COMMANDS];
	u32 count;
};

// A stylized button with idle, hover and click colors.
struct FUIButtonStyle
{
	v4 idleColor;
	v4 hoverColor;
	v4 pressedColor;
	v4 textColor;
	HTexture texture;
};

// Controller UI navigation state.
struct FUINavState
{
	i32 focusedIndex;       // which button is currently selected
	i32 buttonCount;        // how many buttons were submitted this frame
};

// ─────────────────────────────────────────────
//  Public API
// ────────────────────────────────────────────

// Pushes a rect into the ui bucket to draw on the screen.
inline void UIPushRect(FUICommandsBucket* bucket, v4 rect, v4 coords, v4 color, HTexture hTexture)
{
	Assert(bucket->count < FMAX_UI_COMMANDS);
	FUICommand* cmd = &bucket->commands[bucket->count++];
	*cmd = { rect, coords, color, hTexture };
}

// Pushes text into the ui bucket to draw on the screen using a font.
// Font glyphs are just rects being drawn on the screen.
inline void UIPushText(FUICommandsBucket* bucket, FFont* font, cc8* text, v2 pos, v4 color)
{
	// Current pen position while laying out glyphs.
	v2 cursor = pos;

	for (cc8* p = text; *p; ++p)
	{
		// Skip unsupported characters (currently we only support ASCII).
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

		UIPushRect(bucket, rect, coords, color, font->atlas);
		cursor.x += glyph->xadvance;
	}
}

// Calculates the total width of a given text.
// Used to centralize the text in a rect.
inline v2 UIMeasureTextWidth(FFont* font, cc8* text)
{
	f32 width = 0.0f;

	for (cc8* p = text; *p; ++p)
	{
		if (*p < 32 || *p > 127)
		{
			continue;
		}

		FFontGlyph* glyph = &font->glyphs[*p - 32];
		width += glyph->xadvance;
	}

	v2 result = { width, (f32)font->size };
	return result;
}

// Pushes a stylized button into the ui commads bucket.
inline void UIPushButton(v4 rect, cc8* text, FUICommandsBucket* bucket, FUIButtonStyle* style, FFont* font, b8 clicked, b8 hovered)
{
	v4 color = style->idleColor;
	if (clicked)
	{
		color = style->pressedColor;
	}
	else if (hovered)
	{
		color = style->hoverColor;
	}

	UIPushRect(bucket, rect, { 0, 0, 1, 1 }, color, style->texture);

	// Plcae the text in the center of the rect.
	v2 textSize = UIMeasureTextWidth(font, text);
	f32 textX = rect.x + (rect.width - textSize.x) * 0.5f;
	f32 textY = rect.y + (rect.height * 0.5f) + /*yoffset*/4.0f;
	UIPushText(bucket, font, text, { textX, textY }, style->textColor);
}

// Returns true if the mouse position is withing the rect bounds.
inline b8 UIPointInRect(v2 mPos, v4 rect)
{
	b8 result = (mPos.x >= rect.x) && (mPos.x <= rect.x + rect.width) &&
				(mPos.y >= rect.y) && (mPos.y <= rect.y + rect.height);
	return result;
}

// Moves focus to the next index.
// - wrap: true->Index will wrap to the first index if it reaches the last, otherwise stays on the last.
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

// Moves focus to the previous index.
// - wrap: true->Index will wrap to the last index if it reaches the last, otherwise stays on the last.
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

// ─────────────────────────────────────────────

#endif // FADO_UI_H