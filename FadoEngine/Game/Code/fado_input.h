// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_INPUT_H
#define FADO_INPUT_H

#include "fado_types.h"

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Input -- 

// Input Modes
// Input_Game: during gameplay, and
// Input_UI: during pause, menu or any UI interaction.
// Can be both.
enum EInputMode : u32
{
    Input_Game = (1 << 0),		    // Game input mode.
    Input_UI =   (1 << 1),		    // UI/Menu input mode.
};

#define INPUT_MODE_ALL (Input_Game | Input_UI)

// ────────────────────────────────────────────

// State of a button
/*
*  Pressed()   ->   !wasDown && isDown
*  Down()      ->    wasDown && isDown
*  Released()  ->    wasDown && !isDown
*  Up()        ->   !wasDown && !isDown
*/
struct FGameButtonState
{
    b8 wasDown;
    b8 isDown;
    f32 heldLength;	// Time since the button has been pressed and held.
};

ForceInline b8 Pressed(FGameButtonState* buttonState)
{
    return (!buttonState->wasDown && buttonState->isDown);
}
ForceInline b8 Down(FGameButtonState* buttonState)
{
    return (buttonState->wasDown && buttonState->isDown);
}
ForceInline b8 Released(FGameButtonState* buttonState)
{
    return (buttonState->wasDown && !buttonState->isDown);
}
ForceInline b8 Up(FGameButtonState* buttonState)
{
    return (!buttonState->wasDown && !buttonState->isDown);
}

// ────────────────────────────────────────────

struct FMouseInput
{
    FGameButtonState buttons[5];    // Mouse Buttons: 0 left, 1 middle, 2 right
    i32 x, y, z;                    // Position 
    i32 deltaX, deltaY;             // Difference in mouse position between the last and current frame.
    b32 isRotating;                 // Used to track mouse rotation so we prevent snappy rotatiots when delta is huge.
};

// ────────────────────────────────────────────

enum EStickDirection
{
    StickDirection_None,
    Stick_Up,
    Stick_Down,
    Stick_Left,
    Stick_Right
};

// Checks if a stick is held in a specific directions
ForceInline b8 IsStickHeld(v2 stickAverage, EStickDirection direction)
{
    b8 result = false;
    f32 threshHold = 0.5f;

    switch (direction)
    {
    case EStickDirection::Stick_Up:
    {
        result = stickAverage.y > threshHold;
    } break;

    case EStickDirection::Stick_Down:
    {
        result = stickAverage.y < -threshHold;
    } break;

    case EStickDirection::Stick_Left:
    {
        result = stickAverage.x < -threshHold;
    } break;

    case EStickDirection::Stick_Right:
    {
        result = stickAverage.x > threshHold;
    } break;

    default:
    {} break;
    }

    return result;
}

// ────────────────────────────────────────────

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

            //>> All buttons must be added above this one.
            FGameButtonState terminator;
        };
    };
};

struct FGameInput
{
    FGameControllerInput controllers[5];    // 0->Keyboard, 1-4>Controller
    FMouseInput mouse;
    EInputMode mode;
    f32 deltaTime;
};

// ──────────────────────────────────────────────────────────────────────────────────────────

#endif // FADO_INPUT_H