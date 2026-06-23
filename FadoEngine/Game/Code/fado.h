// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_H
#define FADO_H

#include "fado_types.h"
#include "fado_ui.h"

/*
* Game Layer
  - The plan is to have this as the connection layer between the game and engine and
    for any game, we just call a specific game's update inside the update loop here.
    This wasy, technically we can just drop game files and have a game run out of the box.
*/

// ──────────────────────────────────────────────────────────────────────────────────────────

// State of a button
// - !wasDown && isDown -> was just clicked
// - wasDown && !isDown -> was just released
struct FGameButtonState
{
	b8 wasDown;
	b8 isDown;
	f32 heldLength;	// Time since the button has been pressed and held.
};

struct FMouseInput
{
    FGameButtonState buttons[5];    // Mouse Buttons: 0 left, 1 middle, 2 right
    i32 x, y, z;                    // Position 
    i32 deltaX, deltaY;             // Difference in mouse position between the last and current frame.
    b32 isRotating;                 // Used to track mouse rotation so we prevent snappy rotatiots when delta is huge.
};

enum EStickDirection
{
    StickDirection_None,
    Up,
    Down,
    Left,
    Right
};

// Game Controller - used for both keyboard and joysticks
struct FGameControllerInput
{
    v2 leftStickAverage;
    v2 rightStickAverage;
    b32 isConnected;
    b32 isAnalog;

    // L2 & R2 buttons are handled as triggers with push values.
    f32 leftTrigger;
    f32 rightTrigger;

    union
    {
        FGameButtonState buttons[14];
        struct
        {
            FGameButtonState dpadUp;
            FGameButtonState dpadDown;
            FGameButtonState dpadLeft;
            FGameButtonState dpadRight;

            FGameButtonState actionUp;
            FGameButtonState actionDown;
            FGameButtonState actionLeft;
            FGameButtonState actionRight;

            FGameButtonState leftShoulder;          // L1
            FGameButtonState rightShoulder;         // R1

            FGameButtonState leftTriggerButton;     // L2
            FGameButtonState rightTriggerButton;    // R2

            FGameButtonState start;
            FGameButtonState back;

            //? All buttons must be added above this one.
            FGameButtonState terminator;
        };
    };
};

struct FGameInput
{
    FMouseInput mouse;
    FGameControllerInput controllers[5];    // 0->Keyboard, 1-4>Controller
    f32 deltaTime;
};

// ──────────────────────────────────────────────────────────────────────────────────────────
struct FGameState
{
    b32 running;
    b32 initialized;
    
    FSharedStuff* shared;

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

    // Mesh handles
    HEntity hPlaneMesh;
    HMesh hCubeMesh;
    HMesh hSphereMesh;

    // Texture handles
    HTexture hGridTexture;
    HTexture hSkyBoxTexture;
    HTexture hMosaicTexture;
    HTexture hGraniteTexture;
    HTexture hWhiteTexture;
};

// ──────────────────────────────────────────────────────────────────────────────────────────
#define GAME_UPDATE(name) void name(FGameState* gameState, FGameInput* input)
typedef GAME_UPDATE(FGameUpdate);

#endif FADO_H