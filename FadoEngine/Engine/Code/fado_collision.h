// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_COLIISION_H
#define FADO_COLIISION_H

#include "fado_math.h"
#include "fado_types.h"

typedef u32 HCollider;

// ─────────────────────────────────────────────
//  Limits
// ─────────────────────────────────────────────
#define FMAX_COLLIDERS           1024
#define FMAX_COLLISION_PAIRS     4096   // broad-phase candidate pairs per frame
#define FMAX_CONTACTS            2048   // confirmed narrow-phase contacts per frame

#define GRID_CELL_SIZE          4.0f   // world-units per cell (set ≈ your average entity diameter)
#define GRID_WIDTH              128    // cells along X  (GRID_WIDTH  * GRID_CELL_SIZE = world extent)
#define GRID_HEIGHT             128    // cells along Z  (3-D)  / Y (2-D)
#define GRID_MAX_PER_CELL       32     // max colliders that fit in one cell
/*─────────────────────────────────────────────*/

// ─────────────────────────────────────────────
//  Collider flags
// ─────────────────────────────────────────────
enum ECollisionFlags : u32
{
	Collision_Ignore =	  0,			// no detection, no response, effectively disabled
	Collision_Trigger =	  (1 << 0),		// overlap event only — no physics response
	Collision_Static =	  (1 << 1),		// never moves, never rebuilt; always pushes the other side fully
	Collision_Kinematic = (1 << 2),		// like Static but rebuilt each frame (rotating/moving objects); always pushes the other side fully
	Collision_Dynamic =	  (1 << 3),		// normal solid mover (e.g. player); pushed fully by Static/Kinematic, shares push 50/50 with other Dynamic
	Collision_Physics =	  (1 << 4),		// lowest-priority mover; pushed fully by Static/Kinematic/Dynamic, shares 50/50 with other Physics
	Collision_Is2D =	  (1 << 5),		// ignore Y axis (flatten AABB to XZ plane)
};

#define COLLISION_SOLID_MASK (Collision_Static | Collision_Kinematic | Collision_Dynamic | Collision_Physics)

inline ECollisionFlags operator|(ECollisionFlags a, ECollisionFlags b)
{
	ECollisionFlags result = (ECollisionFlags)((u32)a | (u32)b);
	return result;
}
inline ECollisionFlags operator&(ECollisionFlags a, ECollisionFlags b)
{
	ECollisionFlags result = (ECollisionFlags)((u32)a & (u32)b);
	return result;
}

// ─────────────────────────────────────────────
//  FAABB  — axis-aligned bounding box
//  min/max are in WORLD space (rebuilt each frame from transform + half-extents)
// ─────────────────────────────────────────────
struct FAABB
{
	v3 min;
	v3 max;
};

// ─────────────────────────────────────────────
//  FOBB — oriented bounding box
// ─────────────────────────────────────────────
struct FOBB
{
	v3 center;
	v3 halfExtents;
	v3 axes[3];   // world-space rotation basis (columns of rotation matrix)
};

// ─────────────────────────────────────────────
//  FRay
// ─────────────────────────────────────────────
struct FRay
{
	v3 origin;
	v3 direction;
};

// ─────────────────────────────────────────────
//  FCollider  — one entry per entity that can collide
// ─────────────────────────────────────────────
struct FCollider
{
	HEntity entityID;		// owning entity
	v3 halfExtents;			// LOCAL half-size; set once on spawn (scaled by transform each frame)
	ECollisionFlags flags;
	FAABB worldAABB;		// Rebuilt every frame by CollisionBuildAABBs()
	FOBB  worldOBB;			// valid only when rotation isn't identity and flags are solid
	b32 useOBB;			// computed each frame in BuildAABBs
};

// ─────────────────────────────────────────────
//  FColliderTable  — SOA pattern
// ─────────────────────────────────────────────
struct FColliderTable
{
	FCollider   colliders[FMAX_COLLIDERS];
	u32         count;
};

// ─────────────────────────────────────────────
//  FCollisionPair  — a candidate pair from broad phase
// ─────────────────────────────────────────────
struct FCollisionPair
{
	u32 a;   // indices into FColliderTable::colliders[]
	u32 b;
};

// ─────────────────────────────────────────────
//  FContactInfo  — result from narrow phase
// ─────────────────────────────────────────────
struct FContactInfo
{
	HEntity entityA;
	HEntity entityB;
	v3      normal;       // points from B -> A (push A out)
	f32     penetration;  // depth of overlap
	b32  isTrigger;		  // at least one side is a trigger no MTV applied
};
// MTV: Minimum Translation Vector. It's the smallest possible movement to separate two overlapping objects.

// ─────────────────────────────────────────────
//  FUniformGrid  — broad-phase spatial structure
//  One flat array of cell buckets; each bucket holds collider indices.
//  Rebuilt from scratch every frame (cheap for < 1k dynamic objects).
// ─────────────────────────────────────────────
struct FGridCell
{
	u32 indices[GRID_MAX_PER_CELL];
	u32 count;
};

struct FUniformGrid
{
	FGridCell cells[GRID_WIDTH * GRID_HEIGHT];

	// World-space origin of the grid (bottom-left corner).
	// Set this to -(GRID_WIDTH * GRID_CELL_SIZE * 0.5) to centre on the world.
	f32 originX;
	union
	{
		f32 originZ;	// for 3D
		f32 originY;	// for 2D
	};
};

// ─────────────────────────────────────────────
//  FCollisionWorld  — everything in one place
// ─────────────────────────────────────────────
struct FCollisionWorld
{
	FColliderTable  colliders;
	FUniformGrid    grid;

	// Scratch buffers, reset each frame:
	FCollisionPair  pairs[FMAX_COLLISION_PAIRS];
	u32             pairCount;

	FContactInfo    contacts[FMAX_CONTACTS];
	u32             contactCount;
};

// ─────────────────────────────────────────────
//  Public API
// ────────────────────────────────────────────

// Called once to set up the grid origin.
void CollisionInitialize(FCollisionWorld* collisionWorld);

// Register an entity with the collision system.
// halfExtents — LOCAL half-size in each axis.
// Pass EColliderFlags::Is2D for top-down / platformer games (Y extent still used for 3-D height checks).
HCollider CollisionAddCollider(FCollisionWorld* collisionWorld, HEntity entityID,
	v3 halfExtents, ECollisionFlags flags = Collision_Ignore);

// Main per-frame call — runs all three stages:
//   1. BuildAABBs   (world-space AABB from transform + halfExtents)
//   2. BroadPhase   (populate grid, emit candidate pairs)
//   3. NarrowPhase  (AABB on non rotating objects, OBB on rotating ones)
// Results sit in collisionWorld->contacts[0..contactCount].
void CollisionUpdate(FCollisionWorld* collisionWorld, FTransforms* transforms, FMemoryArena* scracthArena);

// Apply MTV (minimum translation vector) to resolve solid colliders.
// Called after CollisionUpdate, before rendering.
void CollisionResolve(FCollisionWorld* collisionWorld, FTransforms* transforms);

// Checks if contactInfo->entityA and contactInfo->entityB are the same 2 entity handles passed.
b8 AreEntitiesColliding(FContactInfo* contactInfo, HEntity hEntityA, HEntity hEntityB);

// Checks if an entity is either contactInfo->entityA or contactInfo->entityB.
b8 IsEntityInPair(FContactInfo* contactInfo, HEntity hEntity);

// Checks if the ray intersects an AABB.
b8 RayIntersectsAABB(FRay ray, FAABB aabb, f32* outDistance);

// ────────────────────────────────────────────────────────────────────────

#endif // FADO_COLIISION_H