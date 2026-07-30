// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_SHARED_H
#define FADO_SHARED_H

// ─────────────────────────────────────────────

#include "fado_types.h"
#include "fado_assets.h"
#include "fado_sprite_anim.h"
#include "fado_collision.h"
#include "fado_ui.h"
#include "fado_particles.h"

// ──────────────── Shared Stuff ───────────────
/*
 * Given the structure of the engine, some data needs to be accessed in both the renderer and
 * game, so the following section covers this data which is shared between the egine and game.
*/

// Containes data that both the renderer and the game use/access.
struct FSharedStuff
{
	FCamera camera;
	FViewPort viewport;
	FDirectionalLight dirLight;

	FAssetsHandles assets;

	FEntityTable entityTable;
	FTransforms transforms;
	FCollisionWorld collisionWorld;
	FSpriteSheetTable spriteSheetTable;
	FParticleEmitter particles[FMAX_PARTICLE_EMITTERS];
	FUICommandsBucket uiBucket;

	FEngineMemory* arena;

#if FADO_DEBUG
	// Currently only used in debug mode.
	HEntity selectedEntity;
	b32 canSelect;
#endif // FADO_DEBUG
};

// ───── Helpers ─────

ForceInline v3 GetEntityPosition(FSharedStuff* shared, HEntity hEntity)
{
	return shared->transforms.positions[hEntity];
}

ForceInline quat GetEntityRotation(FSharedStuff* shared, HEntity hEntity)
{
	return shared->transforms.rotations[hEntity];
}

ForceInline v3 GetEntityScale(FSharedStuff* shared, HEntity hEntity)
{
	return shared->transforms.scales[hEntity];
}

ForceInline f32 GetEntityScaleAverage(FSharedStuff* shared, HEntity hEntity)
{
	v3 scale = shared->transforms.scales[hEntity];
	f32 avg = (scale.x + scale.y + scale.z) / 3.0f;
	return avg;
}

// ─────────────────────────────────────────────

#endif // FADO_SHARED_H