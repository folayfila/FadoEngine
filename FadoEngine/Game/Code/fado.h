// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_H
#define FADO_H

#include "fado_types.h"
#include "fado_ui.h"
#include "fado_sound.h"

/*
* Game Layer
  - The plan is to have this as the connection layer between the game and engine and
    for any game, we just call a specific game's update inside the update loop here.
    This wasy, technically we can just drop game files and have a game run out of the box.
*/

// -- Forwards --
struct FGameInput;

// ──────────────────────────────────────────────────────────────────────────────────────────
struct FGameState
{
    b32 running;
    b32 paused;
    b32 initialized;
    
    FSharedStuff* shared;
    FGameInput* input;

    f32 cameraYaw;   // degrees, accumulates freely
    f32 cameraPitch; // degrees, clamped to [-89, 89]

    FFont* font;
    FUINavState uiNavState;

    // Entites
    HEntity infinitePlane;
    HEntity skyBox;
    HEntity cube1;
    HEntity cube2;
    HEntity sphere1;
    HEntity sphere2;
    HEntity fire;

    // folayfila
    HEntity folayfila;
    HTexture hFolayfilaTex;
    HSpriteSheet hFolayfilaSheet;

    // Mesh handles
    HMesh hQuadMesh;        // Created manually once when we load the assets and used across all sprites.
    HEntity hPlaneMesh;
    HMesh hSkyBoxMesh;
    HMesh hCubeMesh;
    HMesh hSphereMesh;

    // Texture handles
    HTexture hWhiteTexture;
    HTexture hGridTexture;
    HTexture hSkyBoxTexture;
    HTexture hMosaicTexture;
    HTexture hGraniteTexture;

    // Sound
    FSoundManager* soundManager;
    HSound hMusic;
    HSound hCollideSFX;
    HSound hUIClickSFX;

    HSound hFireSFX;
    HSound hFireSFXInstance;

    // Levels
    enum ELevel currentLevel;
};

// ──────────────────────────────────────────────────────────────────────────────────────────

void SetGamePaused(FGameState* gameState, b8 pause);

// ──────────────────────────────────────────────────────────────────────────────────────────
#define GAME_UPDATE(name) void name(FGameState* gameState, struct FGameInput* input)
typedef GAME_UPDATE(FGameUpdate);

#endif FADO_H