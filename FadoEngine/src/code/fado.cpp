#include "fado.h"

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

        // Movement
        if ((controllerInput->dpadUp.isDown) || (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Up)))
        {
            gameState->transforms->positions[gameState->hCameraTransform].z += 1.0f;
        }
        if ((controllerInput->dpadDown.isDown) || (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Down)))
        {
            gameState->transforms->positions[gameState->hCameraTransform].z -= 1.0f;
        }
        if ((controllerInput->dpadRight.isDown) || (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Right)))
        {
            gameState->transforms->positions[gameState->hCameraTransform].x += 1.0f;
        }
        if ((controllerInput->dpadLeft.isDown) || (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Left)))
        {
            gameState->transforms->positions[gameState->hCameraTransform].x -= 1.0f;
        }
        if (controllerInput->rightShoulder.isDown)
        {
            gameState->transforms->positions[gameState->hCameraTransform].y += 1.0f;
        }
        if (controllerInput->leftShoulder.isDown)
        {
            gameState->transforms->positions[gameState->hCameraTransform].y -= 1.0f;
        }
        
        // Rotation
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Up))
        {
            gameState->transforms->rotation[gameState->hCameraTransform].x += 1.0f;
        }
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Down))
        {
            gameState->transforms->rotation[gameState->hCameraTransform].x -= 1.0f;
        }
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Right))
        {
            gameState->transforms->rotation[gameState->hCameraTransform].y += 1.0f;
        }
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Left))
        {
            gameState->transforms->rotation[gameState->hCameraTransform].y -= 1.0f;
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