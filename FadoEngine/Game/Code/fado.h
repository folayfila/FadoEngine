// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_H
#define FADO_H

#include "fado_shared.h"
#include "fado_sound.h"

// ────────────────────────────────────────────────────────────────────────

/*
* Game Layer
  - Works as a connection layer between the game and engine.
    For any game/level, we just call a specific game's update inside the update loop here.
    This wasy, technically we can just drop game files and have a game run out of the box.
*/

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Forwards --
struct FGameInput;
struct FLevel;

struct FGameState
{   
    FGameInput* input;
    FSharedStuff* shared;

    FSoundManager* soundManager;

    FLevel* currentLevel;

    FFont* font;
    FUINavState uiNavState;

    f32 cameraYaw;   // degrees, accumulates freely
    f32 cameraPitch; // degrees, clamped to [-89, 89]

    b8 running;
    b8 paused;
    b8 initialized;
};

// ──────────────────────────────────────────────────────────────────────────────────────────

void SetGamePaused(FGameState* gameState, b8 pause);

// ──────────────────────────────────────────────────────────────────────────────────────────
// The only function that's exposed to the engine/platform layer.
#define GAME_UPDATE(name) void name(FGameState* gameState, struct FGameInput* input)
typedef GAME_UPDATE(FGameUpdate);

// ────────────────────────────────────────────────────────────────────────

#endif // FADO_H