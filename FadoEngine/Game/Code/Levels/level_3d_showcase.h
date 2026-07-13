// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef LEVEL_3D_SHOWCASE
#define LEVEL_3D_SHOWCASE

#include "fado.h"
#include "fado_level.h"
#include "fado_input.h"
#include "fado_collision.h"

// ────────────────────────────────────────
// -- Level_3DShowcase --

struct FLevel_3DShowcase : FLevel
{
    HEntity infinitePlane;
    HEntity skyBox;
    HEntity cube1;
    HEntity cube2;
    HEntity sphere1;
    HEntity sphere2;
    HEntity fire;

    HSound hFireSFXInstance;
};

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Init --
internal void Level_3DShowcase_Init(FGameState* gameState)
{
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = shared->transforms;
    FAssetsHandles* assets = gameState->shared->assets;
    FLevel_3DShowcase* level = (FLevel_3DShowcase*)gameState->currentLevel;

    shared->camera.handle = SpawnEntity(shared, INVALID_HANDLE, INVALID_HANDLE);
    shared->transforms->positions[shared->camera.handle] = { 0.0f, 2.5f, -10.0f };

    // infinite plane
    level->infinitePlane = SpawnEntity(shared, assets->hPlaneMesh, assets->hGridTexture);
    transforms->scales[level->infinitePlane] = { 1000.0f, 1.0f, 1000.0f };

    // sky box
    level->skyBox = SpawnEntity(shared, assets->hSkyBoxMesh, assets->hSkyBoxTexture, V4One());
    transforms->scales[level->skyBox] = { 500.0f, 500.0f, 500.0f };

    // Other entities
    level->cube1 = SpawnEntity(shared, assets->hCubeMesh, 0, { 0.63f, 1, 0.21f, 1.0f }, Material_Lit);
    transforms->positions[level->cube1] = { -3.5f, 5.0f, 0 };
    transforms->scales[level->cube1] = { 2.5f, 0.25f, 1.0f };

    level->cube2 = SpawnEntity(shared, assets->hCubeMesh, 0, { 1, 0.21f, 0.63f, 0.75f }, Material_Lit);
    transforms->positions[level->cube2] = { 1.5f, 5.0f, 0 };

    level->sphere1 = SpawnEntity(shared, assets->hSphereMesh, assets->hGraniteTexture, { 1,1,1, 0.25f }, Material_Lit | Material_CastShadow);
    transforms->positions[level->sphere1] = { -1.5f, 2.0f, 0 };

    level->sphere2 = SpawnEntity(shared, assets->hSphereMesh, assets->hMosaicTexture, { 1,1,1, 1 }, Material_Lit | Material_CastShadow);
    transforms->positions[level->sphere2] = { 1.5f, 2.0f, 0 };

    level->fire = SpawnEntity(shared, assets->hCubeMesh, 0, { 1, 0, 0, 1 }, Material_Lit);
    transforms->positions[level->fire] = { 5.0f, 2.0f, 0 };
    transforms->scales[level->fire] = { 0.25f, 0.25f, 0.25f };
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Begin --
internal void Level_3DShowcase_Begin(FGameState* gameState)
{
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = shared->transforms;
    FAssetsHandles* assets = gameState->shared->assets;
    FLevel_3DShowcase* level = (FLevel_3DShowcase*)gameState->currentLevel;

    gameState->input->mode = Input_Game;
    shared->camera.type = Camera_Perspective;

    // Sound 
    level->hFireSFXInstance = SoundPlay3D(gameState->soundManager, level->fire, assets->hFireSFX,
        ESoundCategory::Sound_SFX, 1.0f, true, transforms->positions[level->fire], 0.0f, 5.0f);
    SoundPlay2D(gameState->soundManager, assets->hMusic, ESoundCategory::Sound_Music, 0.5f, true);

    // Collision
    CollisionInitialize(shared->collisionWorld);
    v3 extents = { 1.0f, 1.0f, 1.0f };

    CollisionAddCollider(shared->collisionWorld, shared->camera.handle, { 1.0f, 1.0f, 1.0f }, Collision_Physics);

    CollisionAddCollider(shared->collisionWorld, level->infinitePlane, { 1.0f, 0.01f, 1.0f }, Collision_Static);

    CollisionAddCollider(shared->collisionWorld, level->cube1, extents, Collision_Kinematic);

    CollisionAddCollider(shared->collisionWorld, level->cube2, extents, Collision_Static);

    CollisionAddCollider(shared->collisionWorld, level->sphere1, extents, Collision_Dynamic);

    CollisionAddCollider(shared->collisionWorld, level->sphere2, extents, Collision_Physics);

    CollisionAddCollider(shared->collisionWorld, level->fire, extents, Collision_Physics);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Update --

internal void LeveL_3DShowcase_HandleUIInput(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
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

internal void LeveL_3DShowcase_HandleGameInput(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    // Movement
    FSharedStuff* shared = gameState->shared;
    f32 dt = input->deltaTime;

    FCamera* cam = &shared->camera;
    v3 forward = cam->forward;
    v3 right = cam->right;
    v3 up = cam->up;

    f32 moveSpeed = 10.0f * dt;
    v3* camPos = &shared->transforms->positions[shared->camera.handle];
    if (IsStickHeld(controller->leftStickAverage, Stick_Up) || Down(&controller->dpadUp))
    {
        *camPos += forward * moveSpeed;
    }
    if (IsStickHeld(controller->leftStickAverage, Stick_Down) || Down(&controller->dpadDown))
    {
        *camPos -= forward * moveSpeed;
    }
    if (IsStickHeld(controller->leftStickAverage, Stick_Right) || Down(&controller->dpadRight))
    {
        *camPos += right * moveSpeed;
    }
    if (IsStickHeld(controller->leftStickAverage, Stick_Left) || Down(&controller->dpadLeft))
    {
        *camPos -= right * moveSpeed;
    }
    if (controller->rightShoulder.isDown)
    {
        *camPos += up * moveSpeed;
    }
    if (controller->leftShoulder.isDown)
    {
        *camPos -= up * moveSpeed;
    }

    // Rotation
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

internal void LeveL_3DShowcase_HandleInput(FGameState* gameState, FGameInput* input)
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
            LeveL_3DShowcase_HandleGameInput(gameState, input, controller);
        }
        if (input->mode & Input_UI)
        {
            LeveL_3DShowcase_HandleUIInput(gameState, input, controller);
        }

        // Quit Game
        if (controller->back.isDown)
        {
            gameState->running = false;
        }
    }
}

internal void Level_3DShowcase_Update(FGameState* gameState, f32 dt)
{
    FSharedStuff* shared = gameState->shared;
    FGameInput* input = gameState->input;
    FTransforms* transforms = shared->transforms;
    FAssetsHandles* assets = gameState->shared->assets;
    FLevel_3DShowcase* level = (FLevel_3DShowcase*)gameState->currentLevel;

    // Each frame, feed camera into the listener for 3D audio.
    FSoundListener listener = {};
    v3 camPos = transforms->positions[shared->camera.handle];
    listener.position = camPos;
    listener.forward = shared->camera.forward;
    listener.up = shared->camera.up;
    gameState->soundManager->listener = listener;

    LeveL_3DShowcase_HandleInput(gameState, input);

    // The infinite plane follows the camera to give the illusion of infinite stretch.
    transforms->positions[level->infinitePlane].x = transforms->positions[shared->camera.handle].x;
    transforms->positions[level->infinitePlane].z = transforms->positions[shared->camera.handle].z;

    // The skybox follows the camera to give the of infinite sky.
    transforms->positions[level->skyBox].x = transforms->positions[shared->camera.handle].x;
    transforms->positions[level->skyBox].z = transforms->positions[shared->camera.handle].z;

    Rotate(transforms, level->cube1, { 50.0f * input->deltaTime, 50.0f * input->deltaTime, 0.0f });
    Rotate(transforms, level->cube2, { -50.0f * input->deltaTime, 0.0f, 0.0f });
    Rotate(transforms, level->sphere1, { 0.0f, 50.0f * input->deltaTime, 0.0f });
    Rotate(transforms, level->sphere2, { 0.0f, -50.0f * input->deltaTime, 0.0f });

    //  -- Test and update collisions --
    // 1. Calculate and detect.
    CollisionUpdate(shared->collisionWorld, transforms, &shared->arena->scratch);
    // 2. Resolve (push solid objects apart).
    CollisionResolve(shared->collisionWorld, transforms);
    // 3. React (Iterate contacts for game logic).
    for (u32 i = 0; i < shared->collisionWorld->contactCount; ++i)
    {
        FContactInfo* c = &shared->collisionWorld->contacts[i];
        if (c->entityA == gameState->shared->camera.handle || c->entityB == gameState->shared->camera.handle)
        {
            SoundPlay2D(gameState->soundManager, assets->hCollideSFX, ESoundCategory::Sound_SFX, 0.1f, false);
        }
    }

    // Update the fire sfx pos to match the fire entity's.
    Update3DSoundsPositions(gameState->soundManager->assetBank, shared);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Make --
inline FLevel SetupLevel_3DShowcase()
{
    FLevel_3DShowcase level = {};
    level.Init = Level_3DShowcase_Init;
    level.Begin = Level_3DShowcase_Begin;
    level.Update = Level_3DShowcase_Update;
    level.name = "level_3d_showcase";
    return level;
};


#endif	// LEVEL_3D_SHOWCASE