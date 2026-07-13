// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef LEVEL_2D_SHOWCASE
#define LEVEL_2D_SHOWCASE

#include "fado.h"
#include "fado_level.h"
#include "fado_input.h"
#include "fado_collision.h"
#include "fado_sprite_anim.h"

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Level_2DShowcase --

struct FLevel_2DShowcase : FLevel
{
    HEntity background;
    HEntity borders[4];
    HEntity quads[4];
    HEntity fire;
    HEntity folayfila;

    HSound hFireSFXInstance;
};

enum EFolayfilaAnimState
{
    Folayfila_Idle = 0,
    Folayfila_Run  = 1
};

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Init --
internal void Level_2DShowcase_Init(FGameState* gameState)
{
    FAssetsHandles* assets = gameState->shared->assets;
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = shared->transforms;
    FLevel_2DShowcase* level = (FLevel_2DShowcase*)gameState->currentLevel;

    shared->camera.handle = SpawnEntity(shared, INVALID_HANDLE, INVALID_HANDLE);
    shared->transforms->positions[shared->camera.handle] = { 0.0f, 2.5f, -10.0f };

    // Background - just a big blue plane
    level->background = SpawnEntity(shared, assets->hQuadMesh, assets->hWhiteTexture, FColor::LightBlue());
    transforms->positions[level->background] = { 0, 0, 1.0f };
    transforms->scales[level->background] = { 1000.0f, 1000.0f, 0.0f };

    // folayfila
    level->folayfila = SpawnEntity(shared, assets->hQuadMesh, assets->hFolayfilaTex, V4One(), Material_Transparent);
    transforms->positions[level->folayfila] = V3Zero();

    // quads
    level->quads[0] = SpawnEntity(shared, assets->hQuadMesh, assets->hMosaicTexture, { 0.38f, 0.81f, 1, 0.75f });
    transforms->positions[level->quads[0]] = { -3.5f, 5.0f, 0 };
    transforms->scales[level->quads[0]] = { 1, 1, 0 };

    level->quads[1] = SpawnEntity(shared, assets->hQuadMesh, assets->hWhiteTexture, { 1, 0.57f, 0.38f, 1 });
    transforms->positions[level->quads[1]] = { 1.5f, 5.0f, 0 };
    transforms->scales[level->quads[1]] = { 0.5f, 0.5f, 0 };

    level->quads[2] = SpawnEntity(shared, assets->hQuadMesh, assets->hWhiteTexture, { 1, 0.5f, 0.875f, 1.5f });
    transforms->positions[level->quads[2]] = { -1.5f, 2.0f, 0 };

    level->quads[3] = SpawnEntity(shared, assets->hQuadMesh, assets->hGraniteTexture, V4One());
    transforms->positions[level->quads[3]] = { 1.5f, 2.0f, 0 };

    // borders
    level->borders[0] = SpawnEntity(shared, assets->hQuadMesh, assets->hWhiteTexture, FColor::Brown());
    transforms->positions[level->borders[0]] = { -10, 0, 0 }; // left border
    transforms->scales[level->borders[0]] = { 1, 20, 0 };

    level->borders[1] = SpawnEntity(shared, assets->hQuadMesh, assets->hWhiteTexture, FColor::Brown());
    transforms->positions[level->borders[1]] = { 10, 0, 0 }; // right
    transforms->scales[level->borders[1]] = { 1, 20, 0 };

    level->borders[2] = SpawnEntity(shared, assets->hQuadMesh, assets->hWhiteTexture, FColor::Brown());
    transforms->positions[level->borders[2]] = { 0, 10, 0 }; // top
    transforms->scales[level->borders[2]] = { 21, 1, 0 };

    level->borders[3] = SpawnEntity(shared, assets->hQuadMesh, assets->hWhiteTexture, FColor::Brown());
    transforms->positions[level->borders[3]] = { 0, -10, 0 }; // bottom
    transforms->scales[level->borders[3]] = { 21, 1, 0 };

    // fire
    level->fire = SpawnEntity(shared, assets->hQuadMesh, assets->hMosaicTexture, V4One());
    transforms->positions[level->fire] = { 0, 2.0f, 0 };
    transforms->scales[level->fire] = { 0.5f, 0.5f, 0 };
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Begin --
internal void Level_2DShowcase_Begin(FGameState* gameState)
{
    FAssetsHandles* assets = gameState->shared->assets;
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = shared->transforms;
    FLevel_2DShowcase* level = (FLevel_2DShowcase*)gameState->currentLevel;

    gameState->input->mode = Input_Game;
    gameState->shared->camera.type = Camera_Orthographic;

    AddClip(&shared->spriteSheetTable->sheets[assets->hFolayfilaSheet], 0, 2, 2.0f, true);
    AddClip(&shared->spriteSheetTable->sheets[assets->hFolayfilaSheet], 2, 2, 10.0f, true);
    FAnimState* anim = &shared->entityTable->entities[level->folayfila].animState;
    SetClip(anim, Folayfila_Run);

    // Sound 
    level->hFireSFXInstance = SoundPlay3D(gameState->soundManager, level->fire, assets->hFireSFX,
        ESoundCategory::Sound_SFX, 1.0f, true, transforms->positions[level->fire], 0.0f, 5.0f);
    SoundPlay2D(gameState->soundManager, assets->hMusic, ESoundCategory::Sound_Music, 0.5f, true);

    // Collision
    CollisionInitialize(shared->collisionWorld);
    v3 extents = { 0.5f, 0.5f, 1 };

    // folayfila
    CollisionAddCollider(shared->collisionWorld, level->folayfila, extents, Collision_Physics | Collision_Is2D);

    // quads
    CollisionAddCollider(shared->collisionWorld, level->quads[0], extents, Collision_Physics | Collision_Is2D);
    CollisionAddCollider(shared->collisionWorld, level->quads[1], extents, Collision_Physics | Collision_Is2D);
    CollisionAddCollider(shared->collisionWorld, level->quads[2], extents, Collision_Physics | Collision_Is2D);
    CollisionAddCollider(shared->collisionWorld, level->quads[3], extents, Collision_Physics | Collision_Is2D);

    // borders
    v3 bordersExtents = V3One();
    CollisionAddCollider(shared->collisionWorld, level->borders[0], extents, Collision_Static | Collision_Is2D);
    CollisionAddCollider(shared->collisionWorld, level->borders[1], extents, Collision_Static | Collision_Is2D);
    CollisionAddCollider(shared->collisionWorld, level->borders[2], extents, Collision_Static | Collision_Is2D);
    CollisionAddCollider(shared->collisionWorld, level->borders[3], extents, Collision_Static | Collision_Is2D);

    // fire
    CollisionAddCollider(shared->collisionWorld, level->fire, extents, Collision_Kinematic | Collision_Is2D);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Update --

internal void LeveL_2DShowcase_HandleUIInput(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
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

internal void LeveL_2DShowcase_HandleGameInput(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    // Movement
    f32 dt = input->deltaTime;
    FSharedStuff* shared = gameState->shared;
    FLevel_2DShowcase* level = (FLevel_2DShowcase*)gameState->currentLevel;

    f32 moveSpeed = 10.0f * dt;
    v3* folayfilaPos = &shared->transforms->positions[level->folayfila];
    if (IsStickHeld(controller->leftStickAverage, Stick_Up) || Down(&controller->dpadUp))
    {
        folayfilaPos->y += moveSpeed;
    }
    if (IsStickHeld(controller->leftStickAverage, Stick_Down) || Down(&controller->dpadDown))
    {
        folayfilaPos->y -= moveSpeed;
    }
    if (IsStickHeld(controller->leftStickAverage, Stick_Right) || Down(&controller->dpadRight))
    {
        folayfilaPos->x += moveSpeed;

        // Flip sprite direction to the right.
        shared->transforms->scales[level->folayfila].x = 1.0f;
    }
    if (IsStickHeld(controller->leftStickAverage, Stick_Left) || Down(&controller->dpadLeft))
    {
        folayfilaPos->x -= moveSpeed;

        // Flip sprite direction to the left.
        shared->transforms->scales[level->folayfila].x = -1.0f;
    }
}

internal void LeveL_2DShowcase_HandleInput(FGameState* gameState, FGameInput* input)
{
    // Save the player's pos before all controllers and update the animation after they've all applied their movement.
    FLevel_2DShowcase* level = (FLevel_2DShowcase*)gameState->currentLevel;
    v3 oldPos = gameState->shared->transforms->positions[level->folayfila];

    for (u32 controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
    {
        FGameControllerInput* controller = &input->controllers[controllerIndex];
        if (!controller->isConnected)
        {
            continue;
        }

        // Pause/Unpasue
        if (Pressed(&controller->start))
        {
            SetGamePaused(gameState, !gameState->paused);
        }

        if (input->mode & Input_Game)
        {
            LeveL_2DShowcase_HandleGameInput(gameState, input, controller);
        }
        if (input->mode & Input_UI)
        {
            LeveL_2DShowcase_HandleUIInput(gameState, input, controller);
        }

        // Quit Game
        if (controller->back.isDown)
        {
            gameState->running = false;
        }
    }

    // Update the player sprite anim
    v3 newPos = gameState->shared->transforms->positions[level->folayfila];
    v3 delta = AbsV3(newPos - oldPos);
    FAnimState* anim = &gameState->shared->entityTable->entities[level->folayfila].animState;
    if (delta == V3Zero())
    {
        SetClip(anim, Folayfila_Idle);
    }
    else
    {
        SetClip(anim, Folayfila_Run);
    }
}

internal void Level_2DShowcase_Update(FGameState* gameState, f32 dt)
{
    FAssetsHandles* assets = gameState->shared->assets;
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = shared->transforms;
    FGameInput* input = gameState->input;
    FLevel_2DShowcase* level = (FLevel_2DShowcase*)gameState->currentLevel;

    // Each frame, feed camera into the listener for 3D audio.
    quat folayfilaRot = transforms->rotations[level->folayfila];
    gameState->soundManager->listener.position = transforms->positions[level->folayfila];
    gameState->soundManager->listener.forward = QuatForward(folayfilaRot);
    gameState->soundManager->listener.up = QuatUp(folayfilaRot);

    LeveL_2DShowcase_HandleInput(gameState, input);

    // The infinite plane follows the camera to give the illusion of infinite stretch.
    transforms->positions[level->background].x = transforms->positions[shared->camera.handle].x;
    transforms->positions[level->background].y = transforms->positions[shared->camera.handle].y;

    //  -- Test and update collisions --
    // 1. Calculate and detect.
    CollisionUpdate(shared->collisionWorld, transforms, &shared->arena->scratch);
    // 2. Resolve (push solid objects apart).
    CollisionResolve(shared->collisionWorld, transforms);
    // 3. React (Iterate contacts for game logic).
    for (u32 i = 0; i < shared->collisionWorld->contactCount; ++i)
    {
        FContactInfo* c = &shared->collisionWorld->contacts[i];
        if ((c->entityA == level->folayfila) || (c->entityB == level->folayfila))
        {
            SoundPlay2D(gameState->soundManager, assets->hCollideSFX, ESoundCategory::Sound_SFX, 0.05f, false);
        }
    }

    // Camera follows the player after all position and collision update
    v3 playerPos = gameState->shared->transforms->positions[level->folayfila];
    v3* camPos = &gameState->shared->transforms->positions[gameState->shared->camera.handle];
    camPos->x = playerPos.x;
    camPos->y = playerPos.y;

    // Update the fire sfx pos to match the fire entity's.
    Update3DSoundsPositions(gameState->soundManager->assetBank, shared);

    UpdateAnimState(&shared->entityTable->entities[level->folayfila], &shared->spriteSheetTable->sheets[assets->hFolayfilaSheet], dt);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Make --
inline FLevel SetupLevel_2DShowcase()
{
    FLevel_2DShowcase level = {};
    level.Init = Level_2DShowcase_Init;
    level.Begin = Level_2DShowcase_Begin;
    level.Update = Level_2DShowcase_Update;
    level.name = "level_2d_showcase";
    return level;
};

#endif	// LEVEL_2D_SHOWCASE