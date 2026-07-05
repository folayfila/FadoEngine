// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#include "fado_input.h"
#include "fado.h"

internal void HandleUIInput(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    // UI input
    if (Pressed(&controller->dpadDown))
    {
        UINavigateNext(&gameState->uiNavState, true);
    }
    if (Pressed(&controller->dpadUp))
    {
        UINavigateBack(&gameState->uiNavState, true);
    }
}

internal void HandleGameInput(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    // Movement
    FSharedStuff* shared = gameState->shared;
    f32 dt = input->deltaTime;

    FCamera* cam = &shared->camera;
    v3 forward = cam->forward;
    v3 right = cam->right;
    v3 up = cam->up;

    f32 moveSpeed = 10.0f * dt;
    v3* movedPos = &shared->transforms->positions[shared->camera.handle];
    if (IsStickHeld(controller->leftStickAverage, Stick_Up) || Down(&controller->dpadUp))
    {
        *movedPos += forward * moveSpeed;
    }
    if (IsStickHeld(controller->leftStickAverage, Stick_Down) || Down(&controller->dpadDown))
    {
        *movedPos -= forward * moveSpeed;
    }
    if (IsStickHeld(controller->leftStickAverage, Stick_Right) || Down(&controller->dpadRight))
    {
        *movedPos += right * moveSpeed;
    }
    if (IsStickHeld(controller->leftStickAverage, Stick_Left) || Down(&controller->dpadLeft))
    {
        *movedPos -= right * moveSpeed;
    }
    if (controller->rightShoulder.isDown)
    {
        *movedPos += up * moveSpeed;
    }
    if (controller->leftShoulder.isDown)
    {
        *movedPos -= up * moveSpeed;
    }

    // Rotation
    // Allow camera rotation only to Perspective cameras.
    if (cam->type == Camera_Perspective)
    {
        f32 sensitivity = 100.0f * dt;
        if (IsStickHeld(controller->rightStickAverage, Stick_Up))
        {
            gameState->cameraPitch -= sensitivity;
        }
        if (IsStickHeld(controller->rightStickAverage, Stick_Down))
        {
            gameState->cameraPitch += sensitivity;
        }
        if (IsStickHeld(controller->rightStickAverage, Stick_Right))
        {
            gameState->cameraYaw += sensitivity;
        }
        if (IsStickHeld(controller->rightStickAverage, Stick_Left))
        {
            gameState->cameraYaw -= sensitivity;
        }
        gameState->cameraPitch = Clampf32(gameState->cameraPitch, -89.0f, 89.0f);
        if ((gameState->cameraPitch != 0.0f) || (gameState->cameraYaw != 0.0f))
        {
            SetRotation(shared->transforms, shared->camera.handle,
                { gameState->cameraPitch, gameState->cameraYaw, 0 });
        }

        // Mouse Rotation
        if ((input->mouse.deltaX != 0.0f) || (input->mouse.deltaY != 0.0f))
        {
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
                SetRotation(shared->transforms, shared->camera.handle,
                    { gameState->cameraPitch, gameState->cameraYaw, 0 });
            }
        }
    }
}

void HandleInput(FGameState* gameState, FGameInput* input)
{
    for (u32 controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
    {
        FGameControllerInput* controller = &input->controllers[controllerIndex];
        if (!controller->isConnected)
        {
            return;
        }

        // Pause/Unpasue
        if (Pressed(&controller->start))
        {
            SetGamePaused(gameState, !gameState->paused);
        }

        if (input->mode & Input_Game)
        {
            HandleGameInput(gameState, input, controller);
        }
        if (input->mode & Input_UI)
        {
            HandleUIInput(gameState, input, controller);
        }

        // Quit Game
        if (controller->back.isDown)
        {
            gameState->running = false;
        }
    }
}