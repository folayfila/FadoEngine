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
  - To create a new level, just add a new header.
  - Create a new level struct that inherits from FLevel.
  - Add a "Make" function that assigns the function pointers to the level's custom functions.
  - Check showcase levels as an example.

  - Saving a level is simply writing a header and a bunch of FEntityDesc into a file.
  - Loading a level first unloads the current level by simply resetting the level and FGameState memebers that need reseting.
  - Every level must have an init function which sets the intial values of all entities in that level. The function always gets called
    whether he level has a saved file or not. If it does have a saved file, the entities data is overriden in order.
  - Each level has its own entities/handles, but use the general assets handles from the FAssetHandler.
  - ** ORDER IS VERY IMPORTANT **
    - Loaded entities are overriden in order, so always spawn saved entities first and keep that order.
*/

/*
* FLevel
* A name and bunch of function pointers.
* Ideally, each level has a header file with all the functions and a "Make" function
* that assigns them accordingly.
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
inline HEntity SpawnEntity(FSharedStuff* shared, HMesh hMesh, HTexture hTex = WHITE_TEXTURE, 
    v4 color = V4One(), u8 materialFlags = Material_None, v4 rect = { 0, 0, 1, 1 })
{
    FTransforms* transforms = shared->transforms;

    HEntity handle = shared->entityTable->count++;
    FEntity* entity = &shared->entityTable->entities[handle];
    entity->hMesh = hMesh;

    FMaterial mat = {};
    mat.color = color;
    mat.texture = hTex;
    mat.flags = materialFlags;
    entity->material = mat;

    // By default full texture.
    entity->spriteRect = rect;

    transforms->positions[handle] = {};
    transforms->scales[handle] = V3One();
    transforms->rotations[handle] = QuatIdentity();
    return handle;
}

// ──────────────────────────────────────────────────────────────────────────────────────────

// Unloads the current level and resets all entities.
internal void UnloadCurrentLevel(FGameState* gameState)
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

// Saves the current level.
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
        entityDesc.entity = entities[i];
        entityDesc.pos = GetEntityPosition(gameState->shared, i);
        entityDesc.rot = GetEntityRotation(gameState->shared, i);
        entityDesc.scale = GetEntityScale(gameState->shared, i);
        fwrite(&entityDesc, sizeof(FEntityDesc), 1, file);
    }

    fclose(file);
    return true;
}

// Loads the passed level by its name.
// Always calls the levels Init, then overrides the saved entities in order if the saved file exists.
// Always calls Begin afterwards.
inline b8 LoadLevel(FGameState* gameState, FLevel level)
{
    UnloadCurrentLevel(gameState);

    *gameState->currentLevel = level;
    level.Init(gameState);

    c8 src[FMAX_PATH];
    GetLevelPathFromName(src, level.name);

    FILE* file;
    fopen_s(&file, src, "rb");
    if (file)
    {
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

        // Override saved entities data from the saved file.
        FTransforms* transforms = gameState->shared->transforms;
        for (u32 i = 0; i < levelHeader.entityCount; ++i)
        {
            FEntityDesc desc;
            fread(&desc, sizeof(FEntityDesc), 1, file);
            FEntity* entity = &gameState->shared->entityTable->entities[i];
            *entity = desc.entity;
            transforms->positions[i] = desc.pos;
            transforms->scales[i] = desc.scale;
            transforms->rotations[i] = desc.rot;
        }
        fclose(file);
    }

    // Call begin on the level.
    level.Begin(gameState);
    return true;
}

#endif FADO_LEVEL_H