#include "fado.h"
#include "fado_math.h"

internal HEntity SpawnEntity(FEntityTable* entities, FTransformTable* transforms, HMesh hMesh, HTexture hTex, v4 color, EShaderTypes shaderType)
{
    HEntity handle = entities->count++;
    FEntity* e = &entities->entities[handle];
    e->hMesh = hMesh;
    e->hTexture = hTex;
    e->hTransform = transforms->count++;
    e->color = color;
    e->shaderType = shaderType;
    transforms->scales[e->hTransform] = V3One();
    transforms->rotations[e->hTransform] = QuatIndentity();
    return handle;
}

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

internal void Initialize(FGameState* gameState)
{
    gameState->initialized = true;

    // >> IMPORTANT: Camera MUST be handle 0!
    gameState->hCamera = SpawnEntity(gameState->entityTable, gameState->transforms, INVALID_HANDLE, INVALID_HANDLE, {}, EShaderTypes::Shader_None);
    gameState->transforms->positions[gameState->hCamera] = { 0.0f, 1.0f, -10.0f };

    gameState->infinitePlane = SpawnEntity(gameState->entityTable, gameState->transforms, gameState->hPlane, gameState->hGridTexture, {}, EShaderTypes::UnlitTexture);
    gameState->transforms->scales[gameState->infinitePlane] = { 1000.0f, 1.0f, 1000.0f};

    gameState->cube1 = SpawnEntity(gameState->entityTable, gameState->transforms, gameState->hCube, 0, { 0.63f, 1, 0.21f, 1 }, EShaderTypes::Color);
    gameState->cube2 = SpawnEntity(gameState->entityTable, gameState->transforms, gameState->hCube, 0, { 1, 0.21f, 0.63f, 1 }, EShaderTypes::Color);
    gameState->sphere1 = SpawnEntity(gameState->entityTable, gameState->transforms, gameState->hSphere, gameState->hMosaicTexture, {}, EShaderTypes::LitTexture);
    gameState->sphere2 = SpawnEntity(gameState->entityTable, gameState->transforms, gameState->hSphere, gameState->hMosaicTexture, {}, EShaderTypes::UnlitTexture);

    gameState->transforms->positions[gameState->cube1] = { -1.5f, 5.0f, 0 };
    gameState->transforms->scales[gameState->cube1] = { 1.5f, 0.5f, 1.0f };
    gameState->transforms->positions[gameState->cube2] = { 1.5f, 5.0f, 0 };
    gameState->transforms->positions[gameState->sphere1] = { -1.5f, 2.0f, 0 };
    gameState->transforms->positions[gameState->sphere2] = { 1.5f, 2.0f, 0 };
}

void GameUpdate(FEngineMemory* memory, FGameState* gameState, FGameInput* input)
{
    if (!gameState->initialized)
    {
        Initialize(gameState);
    }

	HandleGameInput(gameState, input);

    gameState->transforms->positions[gameState->infinitePlane].x = gameState->transforms->positions[gameState->hCamera].x;
    gameState->transforms->positions[gameState->infinitePlane].z = gameState->transforms->positions[gameState->hCamera].z;

    Rotate(gameState->transforms, gameState->cube1, { 50.0f*input->deltaTime, 0.0f, 0.0f });
    Rotate(gameState->transforms, gameState->cube2, { -50.0f * input->deltaTime, 0.0f, 0.0f });
    Rotate(gameState->transforms, gameState->sphere1, { 0.0f, 50.0f * input->deltaTime, 0.0f });
    Rotate(gameState->transforms, gameState->sphere2, { 0.0f, -50.0f * input->deltaTime, 0.0f });
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