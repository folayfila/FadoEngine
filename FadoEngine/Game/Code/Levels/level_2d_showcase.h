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
    HEntity cube1;
    HEntity cube2;
    HEntity sphere1;
    HEntity sphere2;
    HEntity fire;
    HEntity folayfila;

    HSound hFireSFXInstance;
};

enum EFolayfilaAnimState
{
    Folayfila_Idle,
    Folayfila_Run
};

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Init --
internal void Level_2DShowcase_Init(FGameState* gameState)
{
    FAssetsHandles* assets = &gameState->assets;
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = shared->transforms;
    FLevel_2DShowcase* level = (FLevel_2DShowcase*)gameState->currentLevel;

    shared->camera.handle = SpawnEntity(shared, INVALID_HANDLE, INVALID_HANDLE);
    shared->transforms->positions[shared->camera.handle] = { 0.0f, 2.5f, -10.0f };

    // Background - just a big blue plane
    level->background = SpawnEntity(shared, assets->hPlaneMesh, WHITE_TEXTURE, { 0, 0.5f, 0.9f, 1 }, false);
    transforms->scales[level->background] = { 1000.0f, 0.1f, 1000.0f };
    transforms->positions[level->background].z = 200.0f;
    SetRotation(transforms, level->background, { -90, 0, 0 });

    // Other entities
    level->cube1 = SpawnEntity(shared, assets->hCubeMesh, assets->hMosaicTexture, { 0.38f, 0.81f, 1, 0.75f });
    transforms->positions[level->cube1] = { -3.5f, 5.0f, 0 };
    transforms->scales[level->cube1] = { 0.5f, 0.5f, 0.5f };

    level->cube2 = SpawnEntity(shared, assets->hCubeMesh, 0, { 1, 0.57f, 0.38f, 1 });
    transforms->positions[level->cube2] = { 1.5f, 5.0f, 0 };
    transforms->scales[level->cube2] = { 0.5f, 0.5f, 1.0f };

    level->sphere1 = SpawnEntity(shared, assets->hSphereMesh, 0, { 1, 0.5f, 0.875f, 1.5f });
    transforms->positions[level->sphere1] = { -1.5f, 2.0f, 0 };

    level->sphere2 = SpawnEntity(shared, assets->hSphereMesh, assets->hGraniteTexture);
    transforms->positions[level->sphere2] = { 1.5f, 2.0f, 0 };

    level->fire = SpawnEntity(shared, assets->hCubeMesh, assets->hMosaicTexture);
    transforms->positions[level->fire] = { -5.0f, 2.0f, 0 };
    transforms->scales[level->fire] = { 0.25f, 0.25f, 0.25f };

    level->folayfila = SpawnSprite(shared, assets->hQuadMesh, assets->hFolayfilaTex);
    transforms->positions[level->folayfila] = { 5.0f, 5.0f, 0 };
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Begin --
internal void Level_2DShowcase_Begin(FGameState* gameState)
{
    FAssetsHandles* assets = &gameState->assets;
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = shared->transforms;
    FLevel_2DShowcase* level = (FLevel_2DShowcase*)gameState->currentLevel;

    gameState->input->mode = Input_Game;
    gameState->shared->camera.type = Camera_Orthographic;

    AddClip(&shared->spriteSheetTable->sheets[assets->hFolayfilaSheet], 0, 2, 1.0f, true);
    AddClip(&shared->spriteSheetTable->sheets[assets->hFolayfilaSheet], 2, 2, 10.0f, true);
    FAnimState* anim = &shared->entityTable->entities[level->folayfila].animState;
    InitAnimState(anim, assets->hFolayfilaSheet, Folayfila_Run);

    // Sound 
    level->hFireSFXInstance = SoundPlay3D(gameState->soundManager, level->fire, assets->hFireSFX,
        ESoundCategory::Sound_SFX, 1.0f, true, transforms->positions[level->fire], 0.0f, 5.0f);
    SoundPlay2D(gameState->soundManager, assets->hMusic, ESoundCategory::Sound_Music, 0.5f, true);

    // Collision
    CollisionInitialize(shared->collisionWorld);
    v3 extents = { 1.0f, 1.0f, 1.0f };

    CollisionAddCollider(shared->collisionWorld, level->background, { 1.0f, 0.01f, 1.0f }, Collision_Static);

    CollisionAddCollider(shared->collisionWorld, level->cube1, extents, Collision_Kinematic);

    CollisionAddCollider(shared->collisionWorld, level->cube2, extents, Collision_Static);

    CollisionAddCollider(shared->collisionWorld, level->sphere1, extents, Collision_Dynamic);

    CollisionAddCollider(shared->collisionWorld, level->sphere2, extents, Collision_Physics);

    CollisionAddCollider(shared->collisionWorld, level->fire, extents, Collision_Physics);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Update --
internal void Level_2DShowcase_Update(FGameState* gameState, f32 dt)
{
    FAssetsHandles* assets = &gameState->assets;
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = shared->transforms;
    FGameInput* input = gameState->input;
    FLevel_2DShowcase* level = (FLevel_2DShowcase*)gameState->currentLevel;

    // Each frame, feed camera into the listener for 3D audio.
    quat folayfilaRot = transforms->rotations[level->folayfila];
    gameState->soundManager->listener.position = transforms->positions[level->folayfila];
    gameState->soundManager->listener.forward = QuatForward(folayfilaRot);
    gameState->soundManager->listener.up = QuatUp(folayfilaRot);

    HandleInput(gameState, input);

    // The infinite plane follows the camera to give the illusion of infinite stretch.
    transforms->positions[level->background].x = transforms->positions[shared->camera.handle].x;
    transforms->positions[level->background].y = transforms->positions[shared->camera.handle].y;

    Rotate(transforms, level->cube1, { 50.0f * dt, 50.0f * dt, 0.0f });
    Rotate(transforms, level->cube2, { -50.0f * dt, 0.0f, 0.0f });
    Rotate(transforms, level->sphere1, { 0.0f, 50.0f * dt, 0.0f });
    Rotate(transforms, level->sphere2, { 0.0f, -50.0f * dt, 0.0f });

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

    UpdateAnimState(&shared->entityTable->entities[level->folayfila], &shared->spriteSheetTable->sheets[assets->hFolayfilaSheet], input->deltaTime);
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