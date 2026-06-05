#include "fado.h"
#include "fado_math.h"

internal bool32 IsStickHeld(v2 stickAverage, EStickDirection direction)
{
    bool32 result = false;
    f32 threshHold = 0.5f;

    switch (direction)
    {
        case EStickDirection::Up:
        {
            result = stickAverage.y > threshHold;
        } break;

        case EStickDirection::Down:
        {
            result = stickAverage.y < -threshHold;
        } break;

        case EStickDirection::Left:
        {
            result = stickAverage.x < -threshHold;
        } break;

        case EStickDirection::Right:
        {
            result = stickAverage.x > threshHold;
        } break;

        default:
        {
        } break;
    }

    return result;
}

internal void HandleGameInput(FGameState* gameState, FGameInput* input)
{
    for (u32 controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
    {
        FGameControllerInput* controllerInput = &input->controllers[controllerIndex];
        if (!controllerInput->isConnected)
        {
            return;
        }

        quat camRot = gameState->transforms->rotations[gameState->hCamera];
        v3* camPos = &gameState->transforms->positions[gameState->hCamera];

        v3 forward = QuatForward(camRot);
        v3 right = QuatRight(camRot);
        v3 up = QuatUp(camRot);

        // Movement
        f32 moveSpeed = 10.0f * input->deltaTime;
        if ((controllerInput->dpadUp.isDown) || (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Up)))
        {
            *camPos += forward * moveSpeed;
        }
        if ((controllerInput->dpadDown.isDown) || (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Down)))
        {
            *camPos -= forward * moveSpeed;
        }
        if ((controllerInput->dpadRight.isDown) || (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Right)))
        {
            *camPos += right * moveSpeed;
        }
        if ((controllerInput->dpadLeft.isDown) || (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Left)))
        {
            *camPos -= right * moveSpeed;
        }
        if (controllerInput->rightShoulder.isDown)
        {
            *camPos += up * moveSpeed;
        }
        if (controllerInput->leftShoulder.isDown)
        {
            *camPos -= up * moveSpeed;
        }
        
        // Rotation
        f32 sensitivity = 100.0f * input->deltaTime;
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Up))
        {
            gameState->cameraPitch -= sensitivity;
        }
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Down))
        {
            gameState->cameraPitch += sensitivity;
        }
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Right))
        {
            gameState->cameraYaw += sensitivity;
        }
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Left))
        {
            gameState->cameraYaw -= sensitivity;
        }
        gameState->cameraPitch = Clampf32(gameState->cameraPitch, -89.0f, 89.0f);
        SetRotation(gameState->transforms, gameState->hCamera,
            { gameState->cameraPitch, gameState->cameraYaw, 0 });
       
        // Mouse Rotation
        // Mouse deltaY maps to pitch and deltaX maps to yaw.
        // The sensitivity value is much smaller than the controller one because mouse deltas are in pixels, not a -1 to 1 range.
        f32 mouseSensitivity = 0.1f;
        f32 mouseYaw = input->mouse.deltaX * mouseSensitivity;
        f32 mousePitch = input->mouse.deltaY * mouseSensitivity;

        // Clamp to prevent gimbal lock at the poles. When we pitch up or down past 90 degrees, 
        // the camera flips because there's no restriction on how far you can pitch.
        gameState->cameraPitch += Clampf32(mousePitch, -89.0f, 89.0f);
        gameState->cameraYaw += mouseYaw;

        // We rotate with mouse only if the mouse right-click is down.
        if ((mouseYaw != 0 || mousePitch != 0) && (input->mouse.isRotating))
        {
            // Rebuild quat from the two angles.
            SetRotation(gameState->transforms, gameState->hCamera,
                { gameState->cameraPitch, gameState->cameraYaw, 0 });
        }

        if (controllerInput->back.isDown)
        {
            gameState->running = false;
        }
    }
}

void GameUpdate(FEngineMemory* memory, FGameState* gameState, FGameInput* input)
{
	HandleGameInput(gameState, input);
}

/*
* Quick debug messasge
#include <stdio.h>
#include <windows.h>
    for (i32 i = 0; i < ArrayCount(controllerInput->buttons); ++i)
    {
        if (controllerInput->buttons[i].isDown)
        {
            char logBuffer[256];
            sprintf_s(logBuffer, "Button %i is down, Held Time: %f\n", i, controllerInput->buttons[i].heldLength);
            OutputDebugStringA(logBuffer);
        }
    }
*/