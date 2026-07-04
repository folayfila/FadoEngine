// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_LEVEL_H
#define FADO_LEVEL_H

#include "fado_types.h"
#include "fado.h"
#include "fado_asset_format.h"
#include <stdio.h>

// ──────────────────────────────────────────────────────────────────────────────────────────
// Just an extension to fado.cpp
// Handles all the levels loading + default init values

/*
* Levels:
  - To create a new level, you must add a new entry to ELevel and add it in the different level switch statements.
  - Saving a level is simply writing a header and a bunch of FEntityDesc into a file.
  - Loading the level unloads the current one first by simply resetting FGameState memebers that need reseting.
  - Every level must have an init function which sets the intial values of all entities in that level. If a level doesn't have a save
    file, it resorts to its init function. Currently all saved entities are in the game state.
    However, both loading and init must call their BeginLevel_ function, which inits things that aren't saved like sounds.
  - Only Save/Load functions are exposed.
*/

// ──────────────────────────────────────────────────────────────────────────────────────────
// All levels in the game
enum ELevel
{
    Level_None = 0,
    Level_01 = 1,
    Level_02 = 2,
    LEVEL_COUNT
};

internal void GetLevelPathFromId(c8* outPath, ELevel level)
{
    cc8* levelName = "";
    switch (level)
    {
        case Level_01:
        {
            levelName = "Level_01";
        } break;

        case Level_02:
        {
            levelName = "Level_02";
        } break;

        default:
        {} break;
    }

    snprintf(outPath, FMAX_PATH, "Assets\\Levels\\%s.flevel", levelName);
}


// ──────────────────────────────────────────────────────────────────────────────────────────

// Manually check the type of the entity and assign it to the gameState's handle.
internal void AssignGameStateEntityFromType(FGameState* gameState, EEntityType type, HEntity hEntity)
{
    switch (type)
    {
    case EntityType_None:
    {
    } break;

    case EntityType_Camera:
    {
        gameState->shared->camera.handle = hEntity;
    } break;

    case EntityType_Plane:
    {
        gameState->infinitePlane = hEntity;
    } break;

    case EntityType_Skybox:
    {
        gameState->skyBox = hEntity;
    } break;

    case EntityType_Cube1:
    {
        gameState->cube1 = hEntity;
    } break;

    case EntityType_Cube2:
    {
        gameState->cube2 = hEntity;
    } break;

    case EntityType_Sphere1:
    {
        gameState->sphere1 = hEntity;
    } break;

    case EntityType_Sphere2:
    {
        gameState->sphere2 = hEntity;
    } break;

    case EntityType_Fire:
    {
        gameState->fire = hEntity;
    } break;

    default:
    {} break;
    }
}

// Adds an entity to the entity table and gives it a transform.
// - No dynamic allocation of any sorts, just setting values to an existing array.
internal HEntity SpawnEntity(FSharedStuff* shared, EEntityType type, HMesh hMesh, HTexture hTex = WHITE_TEXTURE, v4 color = V4One(), b8 isLit = true)
{
    FEntityTable* entityTable = shared->entityTable;
    FTransformTable* transforms = shared->transforms;

    HEntity handle = entityTable->count++;
    FEntity* e = &entityTable->entities[handle];
    e->type = type;
    e->hMesh = hMesh;

    FMaterial mat = {};
    mat.color = color;
    mat.texture = hTex;
    mat.isLit = isLit;
    e->material = mat;

    e->hTransform = transforms->count++;
    transforms->scales[e->hTransform] = V3One();
    transforms->rotations[e->hTransform] = QuatIdentity();
    return handle;
}

// Adds an entity to the entity table and sets it up from a loaded FEntityDesc.
// - No dynamic allocation of any sorts, just setting values to an existing array.
internal HEntity SpawnEntityFromDesc(FGameState* gameState, FEntityDesc* desc)
{
    FEntityTable* entityTable = gameState->shared->entityTable;
    FTransformTable* transforms = gameState->shared->transforms;

    HEntity handle = entityTable->count++;
    FEntity* e = &entityTable->entities[handle];
    e->type = desc->type;
    e->hMesh = desc->hMesh;
    e->material = desc->material;
    e->hTransform = transforms->count++;
    transforms->positions[e->hTransform] = desc->pos;
    transforms->scales[e->hTransform] = desc->scale;
    transforms->rotations[e->hTransform] = desc->rot;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// Levels Default inits

// ────────────────────────────────────────
// -- Level_01 --

internal void BeginLevel_01(FGameState* gameState)
{
    FSharedStuff* shared = gameState->shared;
    FTransformTable* transforms = shared->transforms;
    FEntityTable* entityTable = shared->entityTable;

    // Sound 
    gameState->hFireSFXInstance = SoundPlay3D(gameState->soundManager, gameState->hFireSFX, ESoundCategory::Sound_SFX, 1.0f, true, transforms->positions[GetTransformHandle(entityTable, gameState->fire)], 0.0f, 5.0f);
    SoundPlay2D(gameState->soundManager, gameState->hMusic, ESoundCategory::Sound_Music, 0.5f, true);

    // Collision
    CollisionInitialize(shared->collisionWorld);
    v3 extents = { 1.0f, 1.0f, 1.0f };

    CollisionAddCollider(shared->collisionWorld, shared->camera.handle,
        entityTable->entities[shared->camera.handle].hTransform, { 1.0f, 1.0f, 1.0f }, ECollisionFlags::Collision_Physics);

    CollisionAddCollider(shared->collisionWorld, gameState->infinitePlane,
        entityTable->entities[gameState->infinitePlane].hTransform, { 1.0f, 0.01f, 1.0f }, ECollisionFlags::Collision_Static);

    CollisionAddCollider(shared->collisionWorld, gameState->cube1, GetTransformHandle(entityTable, gameState->cube1), extents, ECollisionFlags::Collision_Kinematic);

    CollisionAddCollider(shared->collisionWorld, gameState->cube2, GetTransformHandle(entityTable, gameState->cube2), extents, ECollisionFlags::Collision_Static);

    CollisionAddCollider(shared->collisionWorld, gameState->sphere1, GetTransformHandle(entityTable, gameState->sphere1), extents, ECollisionFlags::Collision_Dynamic);

    CollisionAddCollider(shared->collisionWorld, gameState->sphere2, GetTransformHandle(entityTable, gameState->sphere2), extents, ECollisionFlags::Collision_Physics);

    CollisionAddCollider(shared->collisionWorld, gameState->fire, GetTransformHandle(entityTable, gameState->fire), extents, ECollisionFlags::Collision_Physics);
}

internal void InitLevel_01(FGameState* gameState)
{
    FSharedStuff* shared = gameState->shared;
    FTransformTable* transforms = shared->transforms;
    FEntityTable* entityTable = shared->entityTable;

    shared->camera.handle = SpawnEntity(shared, EntityType_Camera, INVALID_HANDLE, INVALID_HANDLE);
    shared->transforms->positions[shared->camera.handle] = { 0.0f, 2.5f, -10.0f };

    // infinite plane
    gameState->infinitePlane = SpawnEntity(shared, EntityType_Plane, gameState->hPlaneMesh, gameState->hGridTexture);
    transforms->scales[gameState->infinitePlane] = { 1000.0f, 1.0f, 1000.0f };

    // sky box
    gameState->skyBox = SpawnEntity(shared, EntityType_Skybox, gameState->hSkyBoxMesh, gameState->hSkyBoxTexture, V4One(), false);
    transforms->scales[gameState->skyBox] = { 500.0f, 500.0f, 500.0f };

    // Other entities
    gameState->cube1 = SpawnEntity(shared, EntityType_Cube1, gameState->hCubeMesh, 0, { 0.63f, 1, 0.21f, 1.0f });
    transforms->positions[gameState->cube1] = { -3.5f, 5.0f, 0 };
    transforms->scales[gameState->cube1] = { 2.5f, 0.25f, 1.0f };

    gameState->cube2 = SpawnEntity(shared, EntityType_Cube2, gameState->hCubeMesh, 0, { 1, 0.21f, 0.63f, 0.75f });
    transforms->positions[gameState->cube2] = { 1.5f, 5.0f, 0 };

    gameState->sphere1 = SpawnEntity(shared, EntityType_Sphere1, gameState->hSphereMesh, gameState->hGraniteTexture, {1,1,1, 0.25f});
    transforms->positions[gameState->sphere1] = { -1.5f, 2.0f, 0 };

    gameState->sphere2 = SpawnEntity(shared, EntityType_Sphere2, gameState->hSphereMesh, gameState->hMosaicTexture, {1,1,1, 1});
    transforms->positions[gameState->sphere2] = { 1.5f, 2.0f, 0 };

    gameState->fire = SpawnEntity(shared, EntityType_Fire, gameState->hCubeMesh, 0, { 1, 0, 0, 1 });
    HTransform hFireTransform = GetTransformHandle(entityTable, gameState->fire);
    transforms->positions[hFireTransform] = { 5.0f, 2.0f, 0 };
    transforms->scales[hFireTransform] = { 0.25f, 0.25f, 0.25f };

    BeginLevel_01(gameState);
}

// ────────────────────────────────────────
// -- Level_02 --

internal void BeginLevel_02(FGameState* gameState)
{
    FSharedStuff* shared = gameState->shared;
    FTransformTable* transforms = shared->transforms;
    FEntityTable* entityTable = shared->entityTable;

    // Sound 
    gameState->hFireSFXInstance = SoundPlay3D(gameState->soundManager, gameState->hFireSFX, ESoundCategory::Sound_SFX, 1.0f, true, transforms->positions[GetTransformHandle(entityTable, gameState->fire)], 0.0f, 5.0f);
    SoundPlay2D(gameState->soundManager, gameState->hMusic, ESoundCategory::Sound_Music, 0.5f, true);

    // Collision
    CollisionInitialize(shared->collisionWorld);
    v3 extents = { 1.0f, 1.0f, 1.0f };

    CollisionAddCollider(shared->collisionWorld, shared->camera.handle,
        entityTable->entities[shared->camera.handle].hTransform, { 1.0f, 1.0f, 1.0f }, ECollisionFlags::Collision_Physics);

    CollisionAddCollider(shared->collisionWorld, gameState->infinitePlane,
        entityTable->entities[gameState->infinitePlane].hTransform, { 1.0f, 0.01f, 1.0f }, ECollisionFlags::Collision_Static);

    CollisionAddCollider(shared->collisionWorld, gameState->cube1, GetTransformHandle(entityTable, gameState->cube1), extents, ECollisionFlags::Collision_Kinematic);

    CollisionAddCollider(shared->collisionWorld, gameState->cube2, GetTransformHandle(entityTable, gameState->cube2), extents, ECollisionFlags::Collision_Static);

    CollisionAddCollider(shared->collisionWorld, gameState->sphere1, GetTransformHandle(entityTable, gameState->sphere1), extents, ECollisionFlags::Collision_Dynamic);

    CollisionAddCollider(shared->collisionWorld, gameState->sphere2, GetTransformHandle(entityTable, gameState->sphere2), extents, ECollisionFlags::Collision_Physics);

    CollisionAddCollider(shared->collisionWorld, gameState->fire, GetTransformHandle(entityTable, gameState->fire), extents, ECollisionFlags::Collision_Physics);
}

internal void InitLevel_02(FGameState* gameState)
{
    FSharedStuff* shared = gameState->shared;
    FTransformTable* transforms = shared->transforms;
    FEntityTable* entityTable = shared->entityTable;

    shared->camera.handle = SpawnEntity(shared, EntityType_Camera, INVALID_HANDLE, INVALID_HANDLE);
    shared->transforms->positions[shared->camera.handle] = { 0.0f, 2.5f, -10.0f };

    // infinite plane
    gameState->infinitePlane = SpawnEntity(shared, EntityType_Plane, gameState->hPlaneMesh, gameState->hGridTexture);
    transforms->scales[gameState->infinitePlane] = { 1000.0f, 1.0f, 1000.0f };

    // sky box
    gameState->skyBox = SpawnEntity(shared, EntityType_Skybox, gameState->hSkyBoxMesh, gameState->hSkyBoxTexture, V4One(), false);
    transforms->scales[gameState->skyBox] = { 500.0f, 500.0f, 500.0f };

    // Other entities
    gameState->cube1 = SpawnEntity(shared, EntityType_Cube1, gameState->hCubeMesh, 0, { 0.38f, 0.81f, 1, 1 });
    transforms->positions[gameState->cube1] = { -3.5f, 5.0f, 0 };
    transforms->scales[gameState->cube1] = { 0.5f, 0.5f, 0.5f };

    gameState->cube2 = SpawnEntity(shared, EntityType_Cube2, gameState->hCubeMesh, 0, { 1, 0.57f, 0.38f, 1 });
    transforms->positions[gameState->cube2] = { 1.5f, 5.0f, 0 };
    transforms->scales[gameState->cube2] = { 0.5f, 0.5f, 1.0f };

    gameState->sphere1 = SpawnEntity(shared, EntityType_Sphere1, gameState->hSphereMesh, 0, { 1, 0.5f, 0.875f, 1 });
    transforms->positions[gameState->sphere1] = { -1.5f, 2.0f, 0 };

    gameState->sphere2 = SpawnEntity(shared, EntityType_Sphere2, gameState->hSphereMesh, gameState->hGraniteTexture);
    transforms->positions[gameState->sphere2] = { 1.5f, 2.0f, 0 };

    gameState->fire = SpawnEntity(shared, EntityType_Fire, gameState->hCubeMesh, gameState->hMosaicTexture);
    HTransform hFireTransform = GetTransformHandle(entityTable, gameState->fire);
    transforms->positions[hFireTransform] = { -5.0f, 2.0f, 0 };
    transforms->scales[hFireTransform] = { 0.25f, 0.25f, 0.25f };

    BeginLevel_02(gameState);
}

// ──────────────────────────────────────────────────────────────────────────────────────────

internal void BeginLevelById(FGameState* gameState, ELevel level)
{
    switch (level)
    {
    case Level_01:
    {
        BeginLevel_01(gameState);
    } break;

    case Level_02:
    {
        BeginLevel_02(gameState);
    } break;

    default:
    {} break;
    }
}

internal void UnloadLevel(FGameState* gameState)
{
    FadoZeroStruct(gameState->shared->entityTable);
    FadoZeroStruct(gameState->shared->transforms);
    FadoZeroStruct(gameState->shared->uiCommands);
    FadoZeroStruct(gameState->shared->collisionWorld);

#if FADO_DEBUG
    gameState->shared->selectedEntity = 0;
#endif // FADO_DEBUG

    // Entites
    gameState->infinitePlane = 0;
    gameState->skyBox = 0;
    gameState->cube1 = 0;
    gameState->cube2 = 0;
    gameState->sphere1 = 0;
    gameState->sphere2 = 0;
    gameState->fire = 0;
    gameState->hFireSFXInstance = 0;
    gameState->currentLevel = Level_None;

    SoundStopAll(gameState->soundManager);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
inline b8 SaveCurrentLevel(FGameState* gameState)
{
    c8 dst[FMAX_PATH];
    GetLevelPathFromId(dst, gameState->currentLevel);

    FILE* file;
    fopen_s(&file, dst, "wb");
    if (!file)
    {
        return false;
    }

    FAssetHeader assetHeader = {};
    assetHeader.magic = FASSET_MAGIC;
    assetHeader.assetType = FASSET_TYPE_LEVEL;
    assetHeader.version = FASSET_VERSION;
    assetHeader.reserved = 0;
    fwrite(&assetHeader, sizeof(assetHeader), 1, file);

    FEntityTable* entityTable = gameState->shared->entityTable;

    FLevelHeader levelHeader = {};
    levelHeader.index = (u32)gameState->currentLevel;
    levelHeader.entityCount = entityTable->count;
    levelHeader.flags = 0;
    fwrite(&levelHeader, sizeof(FLevelHeader), 1, file);

    // TODO: Add EntityFlags of some sorts so we can only save those that need saving
    FTransformTable* transformTable = gameState->shared->transforms;
    FEntityDesc entityDesc;
    for (u32 i = 0; i < entityTable->count; ++i)
    {
        entityDesc = {};
        entityDesc.type = entityTable->entities[i].type;
        entityDesc.pos = GetEntityPosition(gameState->shared, i);
        entityDesc.rot = GetEntityRotation(gameState->shared, i);
        entityDesc.scale = GetEntityScale(gameState->shared, i);
        entityDesc.hMesh = entityTable->entities[i].hMesh;
        entityDesc.material = entityTable->entities[i].material;
        entityDesc.reserved = 0;
        fwrite(&entityDesc, sizeof(FEntityDesc), 1, file);
    }

    fclose(file);
    return true;
}

inline b8 LoadLevelById(FGameState* gameState, ELevel level)
{
    UnloadLevel(gameState);

    c8 src[FMAX_PATH];
    GetLevelPathFromId(src, level);

    FILE* file;
    fopen_s(&file, src, "rb");
    if (!file)
    {
        // Init the level if it hasn't been saved.
        switch (level)
        {
            case Level_01:
            {
                InitLevel_01(gameState);
            } break;

            case Level_02:
            {
                InitLevel_02(gameState);
            } break;
            
            default:
            {} break;
        }
        gameState->currentLevel = level;
        return false;
    }

    FAssetHeader assetHeader = {};
    fread(&assetHeader, sizeof(FAssetHeader), 1, file);

    if (assetHeader.magic != FASSET_MAGIC ||
        assetHeader.assetType != FASSET_TYPE_LEVEL ||
        assetHeader.version != FASSET_VERSION)
    {
        fclose(file);
        return false;
    }

    FLevelHeader levelHeader = {};
    fread(&levelHeader, sizeof(FLevelHeader), 1, file);

    // store which level is active
    gameState->currentLevel = (ELevel)levelHeader.index;

    for (u32 i = 0; i < levelHeader.entityCount; ++i)
    {
        FEntityDesc desc;
        fread(&desc, sizeof(FEntityDesc), 1, file);
        HEntity handle = SpawnEntityFromDesc(gameState, &desc);
        AssignGameStateEntityFromType(gameState, desc.type, handle);
    }

    // Call begin level
    BeginLevelById(gameState, level);

    fclose(file);
    return true;
}

#endif FADO_LEVEL_H