#ifndef FADO_H
#define FADO_H

#include "fado_types.h"
#include "fado_ui.h"

// Forward declaractions:
struct FCollisionWorld;

// ──────────────────────────────────────────────────────────────────────────────────────────

struct FGameButtonState
{
	bool32 wasDown;
	bool32 isDown;
	f32 heldLength;	// Time since the button has been pressed and held.
};

struct FMouseInput
{
    FGameButtonState buttons[5];
    i32 x, y, z;
    i32 deltaX, deltaY; // Difference in mouse position between the last and current frame.
    bool32 isRotating;  // Used to track mouse rotation so we prevent snappy rotatiots when delta is huge.
};

enum EStickDirection
{
    StickDirection_None,
    Up,
    Down,
    Left,
    Right
};

struct FGameControllerInput
{
    v2 leftStickAverage;
    v2 rightStickAverage;
    bool32 isConnected;
    bool32 isAnalog;

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

struct FGameState
{
    bool32 running;
    bool32 initialized;

    HEntity hCamera; // Camera MUST always be handle 0!
    f32 cameraYaw;   // degrees, accumulates freely
    f32 cameraPitch; // degrees, clamped to [-89, 89]

    FEntityTable* entityTable;
    FTransformTable* transforms;
    FCollisionWorld* collisionWorld;
    FUICommandBucket* uiCommands;

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
#define GAME_UPDATE(name) void name(FEngineMemory* memory, FGameState* gameState, FGameInput* input)
typedef GAME_UPDATE(FGameUpdate);

#endif FADO_H