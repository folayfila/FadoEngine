// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_LEVEL_H
#define FADO_LEVEL_H

#include "fado_types.h"
#include "fado.h"
#include "fado_asset_format.h"
#include <stdio.h>

// ──────────────────────────────────────────────────────────────────────────────────────────
// Handles all levels loading and saving.

/*
* Levels:
  - To create a new level, just add a new header in Game/Code/Levels
  - Create a new level struct that inherits from FLevel.
  - Add a "Make" function that assigns the function pointers to the level's custom functions.
  - Check showcase levels as an example.

  - Saving a level is simply writing a header and a bunch of FEntityDesc into a file.
  - Loading the level unloads the current one first by simply resetting the level and FGameState memebers that need reseting.
  - Every level must have an init function which sets the intial values of all entities in that level. If a level doesn't have a save
    file, it resorts to its init function.
  - Each level has its own entities/handles, but use the general assets handles from the FAssetHandler.
*/

struct FLevel
{
    // Function pointers to the main 3 per-level functions.
    void (*Init)(FGameState*);
    void (*Begin)(FGameState*);
    void (*Update)(FGameState*, f32 dt);

    cc8* name;
};

// ──────────────────────────────────────────────────────────────────────────────────────────

// outPath = Assets\\Levels\\LevelName
inline void GetLevelPathFromName(c8* outPath, cc8* levelName)
{
    snprintf(outPath, FMAX_PATH, "Assets\\Levels\\%s.flevel", levelName);
}

// Adds an entity to the entity table and gives it a transform.
// - No dynamic allocation of any sorts, just setting values to an existing array.
inline HEntity SpawnEntity(FSharedStuff* shared, EEntityType type, HMesh hMesh, HTexture hTex = WHITE_TEXTURE, v4 color = V4One(), b8 isLit = true)
{
    FTransforms* transforms = shared->transforms;

    HEntity handle = shared->entityTable->count++;
    FEntity* entity = &shared->entityTable->entities[handle];
    entity->type = type;
    entity->hMesh = hMesh;

    FMaterial mat = {};
    mat.color = color;
    mat.texture = hTex;
    mat.isLit = isLit;
    entity->material = mat;

    // By default full texture.
    entity->spriteRect = { 0, 0, 1, 1 };

    transforms->positions[handle] = {};
    transforms->scales[handle] = V3One();
    transforms->rotations[handle] = QuatIdentity();
    return handle;
}

// Helper that wraps regular SpawnEntity. Used to easily spawn sprite entities.
inline HEntity SpawnSprite(FSharedStuff* shared, HMesh quad, HTexture hTex, v4 rect = {0.0f, 0.0f, 1.0f, 1.0f}, v4 color = V4One())
{
    HEntity handle = SpawnEntity(shared, EntityType_Sprite, quad, hTex, color, false);
    shared->entityTable->entities[handle].material.isTransparent = true;
    shared->entityTable->entities[handle].spriteRect = rect;
    return handle;
}

// Adds an entity to the entity table and sets it up from a loaded FEntityDesc.
// - No dynamic allocation of any sorts, just setting values to an existing array.
inline HEntity SpawnEntityFromDesc(FGameState* gameState, FEntityDesc* desc)
{
    FTransforms* transforms = gameState->shared->transforms;

    HEntity handle = gameState->shared->entityTable->count++;
    FEntity* entity = &gameState->shared->entityTable->entities[handle];
    entity->type = desc->type;
    entity->hMesh = desc->hMesh;
    entity->material = desc->material;
    transforms->positions[handle] = desc->pos;
    transforms->scales[handle] = desc->scale;
    transforms->rotations[handle] = desc->rot;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────────────────────────

// Unloads the current level
internal void UnloadLevel(FGameState* gameState)
{
    SetGamePaused(gameState, false);

    FadoZeroStruct(gameState->shared->entityTable);
    FadoZeroStruct(gameState->shared->transforms);
    FadoZeroStruct(gameState->shared->uiCommands);
    FadoZeroStruct(gameState->shared->collisionWorld);

#if FADO_DEBUG
    gameState->shared->selectedEntity = 0;
#endif // FADO_DEBUG

    FadoZeroMemory(gameState->currentLevel, LEVEL_ARENA_SIZE);

    SoundStopAll(gameState->soundManager);
}

inline b8 SaveCurrentLevel(FGameState* gameState)
{
    c8 dst[FMAX_PATH];
    GetLevelPathFromName(dst, gameState->currentLevel->name);

    FILE* file;
    fopen_s(&file, dst, "wb");
    if (!file)
    {
        return false;
    }

    FEntity* entities = gameState->shared->entityTable->entities;
    u32 entitiesCount = gameState->shared->entityTable->count;

    FAssetHeader assetHeader = {};
    assetHeader.magic = FASSET_MAGIC;
    assetHeader.assetType = FASSET_TYPE_LEVEL;
    assetHeader.version = FASSET_VERSION;
    assetHeader.reserved = 0;
    fwrite(&assetHeader, sizeof(assetHeader), 1, file);

    FLevelHeader levelHeader = {};
    levelHeader.entityCount = entitiesCount;
    levelHeader.flags = 0;
    fwrite(&levelHeader, sizeof(FLevelHeader), 1, file);

    // TODO: Add EntityFlags of some sorts so we can only save those that need saving
    FTransforms* transformTable = gameState->shared->transforms;
    FEntityDesc entityDesc;
    for (u32 i = 0; i < entitiesCount; ++i)
    {
        entityDesc = {};
        entityDesc.type = entities[i].type;
        entityDesc.pos = GetEntityPosition(gameState->shared, i);
        entityDesc.rot = GetEntityRotation(gameState->shared, i);
        entityDesc.scale = GetEntityScale(gameState->shared, i);
        entityDesc.hMesh = entities[i].hMesh;
        entityDesc.material = entities[i].material;
        entityDesc.reserved = 0;
        fwrite(&entityDesc, sizeof(FEntityDesc), 1, file);
    }

    fclose(file);
    return true;
}

inline b8 LoadLevel(FGameState* gameState, FLevel level)
{
    *gameState->currentLevel = level;
    c8 src[FMAX_PATH];
    GetLevelPathFromName(src, level.name);

    FILE* file;
    fopen_s(&file, src, "rb");
    if (!file)
    {
        level.Init(gameState);
        level.Begin(gameState);
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

    for (u32 i = 0; i < levelHeader.entityCount; ++i)
    {
        FEntityDesc desc;
        fread(&desc, sizeof(FEntityDesc), 1, file);
        HEntity handle = SpawnEntityFromDesc(gameState, &desc);
    }

    // Call begin level
    level.Begin(gameState);

    fclose(file);
    return true;
}

#endif FADO_LEVEL_H