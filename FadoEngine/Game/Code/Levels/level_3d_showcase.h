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
    FAssetsHandles* assets = &gameState->assets;
    FLevel_3DShowcase* level = (FLevel_3DShowcase*)gameState->currentLevel;

    shared->camera.handle = SpawnEntity(shared, EntityType_Camera, INVALID_HANDLE, INVALID_HANDLE);
    shared->transforms->positions[shared->camera.handle] = { 0.0f, 2.5f, -10.0f };

    // infinite plane
    level->infinitePlane = SpawnEntity(shared, EntityType_Plane, assets->hPlaneMesh, assets->hGridTexture);
    transforms->scales[level->infinitePlane] = { 1000.0f, 1.0f, 1000.0f };

    // sky box
    level->skyBox = SpawnEntity(shared, EntityType_Skybox, assets->hSkyBoxMesh, assets->hSkyBoxTexture, V4One(), false);
    transforms->scales[level->skyBox] = { 500.0f, 500.0f, 500.0f };

    // Other entities
    level->cube1 = SpawnEntity(shared, EntityType_Cube1, assets->hCubeMesh, 0, { 0.63f, 1, 0.21f, 1.0f });
    transforms->positions[level->cube1] = { -3.5f, 5.0f, 0 };
    transforms->scales[level->cube1] = { 2.5f, 0.25f, 1.0f };

    level->cube2 = SpawnEntity(shared, EntityType_Cube2, assets->hCubeMesh, 0, { 1, 0.21f, 0.63f, 0.75f });
    transforms->positions[level->cube2] = { 1.5f, 5.0f, 0 };

    level->sphere1 = SpawnEntity(shared, EntityType_Sphere1, assets->hSphereMesh, assets->hGraniteTexture, { 1,1,1, 0.25f });
    transforms->positions[level->sphere1] = { -1.5f, 2.0f, 0 };

    level->sphere2 = SpawnEntity(shared, EntityType_Sphere2, assets->hSphereMesh, assets->hMosaicTexture, { 1,1,1, 1 });
    transforms->positions[level->sphere2] = { 1.5f, 2.0f, 0 };

    level->fire = SpawnEntity(shared, EntityType_Fire, assets->hCubeMesh, 0, { 1, 0, 0, 1 });
    transforms->positions[level->fire] = { 5.0f, 2.0f, 0 };
    transforms->scales[level->fire] = { 0.25f, 0.25f, 0.25f };
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Begin --
internal void Level_3DShowcase_Begin(FGameState* gameState)
{
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = shared->transforms;
    FAssetsHandles* assets = &gameState->assets;
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
internal void Level_3DShowcase_Update(FGameState* gameState, f32 dt)
{
    FSharedStuff* shared = gameState->shared;
    FGameInput* input = gameState->input;
    FTransforms* transforms = shared->transforms;
    FAssetsHandles* assets = &gameState->assets;
    FLevel_3DShowcase* level = (FLevel_3DShowcase*)gameState->currentLevel;

    // Each frame, feed camera into the listener for 3D audio.
    FSoundListener listener = {};
    v3 camPos = transforms->positions[shared->camera.handle];
    listener.position = camPos;
    listener.forward = shared->camera.forward;
    listener.up = shared->camera.up;
    gameState->soundManager->listener = listener;

    HandleInput(gameState, input);

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