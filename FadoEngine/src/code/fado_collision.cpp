// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#include "fado_collision.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

bool32 AABBOverlap(const FAABB& a, const FAABB& b)
{
    if ((a.max.x < b.min.x) || (a.min.x > b.max.x)) { return false; }
    if ((a.max.y < b.min.y) || (a.min.y > b.max.y)) { return false; }
    if ((a.max.z < b.min.z) || (a.min.z > b.max.z)) { return false; }
    return true;
}

FAABB AABBFromTransform(v3 position, v3 scale, v3 halfExtents)
{
    v3 scaledHalf = { halfExtents.x * scale.x,
                      halfExtents.y * scale.y,
                      halfExtents.z * scale.z };
    FAABB result;
    result.min = { position.x - scaledHalf.x,
                   position.y - scaledHalf.y,
                   position.z - scaledHalf.z };
    result.max = { position.x + scaledHalf.x,
                   position.y + scaledHalf.y,
                   position.z + scaledHalf.z };
    return result;
}

// Convert a world-space X position to a grid column index.
// Clamped so we never write outside the grid array.
internal i32 GridCellX(const FUniformGrid* grid, f32 worldX)
{
    i32 cellX = (i32)((worldX - grid->originX) / GRID_CELL_SIZE);
    cellX = Clampi32(cellX, 0, (GRID_WIDTH - 1));
    return cellX;
}

// Convert a world-space Z (Y in 2D) position to a grid column index.
// Clamped so we never write outside the grid array.
internal i32 GridCellZ(const FUniformGrid* grid, f32 worldZ)
{
    i32 cellZ = (i32)((worldZ - grid->originZ) / GRID_CELL_SIZE);
    cellZ = Clampi32(cellZ, 0, (GRID_HEIGHT - 1));
    return cellZ;
}

internal i32 GridIndex(i32 cellX, i32 cellZ)
{
    i32 result = (cellZ * GRID_WIDTH) + cellX;
    return result;
}

internal void GridInsert(FUniformGrid* grid, i32 cellX, i32 cellZ, u32 colliderIndex)
{
    i32 idx = GridIndex(cellX, cellZ);
    FGridCell* cell = &grid->cells[idx];
    if (cell->count < GRID_MAX_PER_CELL)
    {
        cell->indices[cell->count++] = colliderIndex;
    }
    // If count == GRID_MAX_PER_CELL the collider is silently dropped from this cell.
    // Increase GRID_MAX_PER_CELL or GRID_CELL_SIZE if this happens.
}

// ─────────────────────────────────────────────────────────────────────────────
//  Stage 1 — Build world-space AABBs
// ─────────────────────────────────────────────────────────────────────────────
internal void CollisionBuildAABBs(FCollisionWorld* collisionWorld, FTransformTable* transforms)
{
    for (u32 i = 0; i < collisionWorld->colliders.count; ++i)
    {
        FCollider* collider = &collisionWorld->colliders.colliders[i];

        v3 pos = transforms->positions[collider->hTransform];
        v3 scale = transforms->scales[collider->hTransform];

        // Side-scroller: flatten Z so everything sits on XY plane
        if (collider->flags & ECollisionFlags::Is2D)
        {
            scale.z = 1.0f;
        }

        collider->worldAABB = AABBFromTransform(pos, scale, collider->halfExtents);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Stage 2 — Broad phase  (uniform grid)
// ─────────────────────────────────────────────────────────────────────────────
internal void CollisionBroadPhase(FCollisionWorld* collisionWorld, FMemoryArena* scratchArena)
{
    // Clear grid.
    for (i32 i = 0; i < (GRID_WIDTH * GRID_HEIGHT); ++i)
    {
        collisionWorld->grid.cells[i].count = 0;
    }
    collisionWorld->pairCount = 0;

    // Insert every collider into all cells its AABB overlaps.
    for (u32 i = 0; i < collisionWorld->colliders.count; ++i)
    {
        const FAABB& box = collisionWorld->colliders.colliders[i].worldAABB;

        i32 x0 = GridCellX(&collisionWorld->grid, box.min.x);
        i32 x1 = GridCellX(&collisionWorld->grid, box.max.x);
        i32 z0 = GridCellZ(&collisionWorld->grid, box.min.z);
        i32 z1 = GridCellZ(&collisionWorld->grid, box.max.z);

        for (i32 cz = z0; cz <= z1; ++cz)
        {
            for (i32 cx = x0; cx <= x1; ++cx)
            {
                GridInsert(&collisionWorld->grid, cx, cz, i);
            }
        }
    }

    // --- Walk every cell, emit unique pairs ---
    // We use a simple tag array (per-frame stamp) to avoid duplicate pairs
    // when a large AABB spans multiple cells.
    // A pair (a,b) is canonical when a < b.
    u32* pairTagA = ArenaPushArray(scratchArena, MAX_COLLISION_PAIRS, u32);
    u32* pairTagB = ArenaPushArray(scratchArena, MAX_COLLISION_PAIRS, u32);
    // We will just do a linear scan to de-duplicate — fast enough for < 4k pairs.

    for (i32 cellIdx = 0; cellIdx < (GRID_WIDTH * GRID_HEIGHT); ++cellIdx)
    {
        FGridCell* cell = &collisionWorld->grid.cells[cellIdx];
        for (u32 ai = 0; ai < cell->count; ++ai)
        {
            for (u32 bi = ai + 1; bi < cell->count; ++bi)
            {
                u32 a = cell->indices[ai];
                u32 b = cell->indices[bi];
                if (a > b)
                {
                    u32 tmp = a;
                    a = b;
                    b = tmp;
                    // canonical: a < b
                }

                // Check for duplicate pair
                bool32 found = false;
                for (u32 pi = 0; pi < collisionWorld->pairCount; ++pi)
                {
                    if ((pairTagA[pi] == a) && (pairTagB[pi] == b))
                    {
                        found = true;
                        break;
                    }
                }

                if (!found && (collisionWorld->pairCount < MAX_COLLISION_PAIRS))
                {
                    pairTagA[collisionWorld->pairCount] = a;
                    pairTagB[collisionWorld->pairCount] = b;
                    collisionWorld->pairs[collisionWorld->pairCount].a = a;
                    collisionWorld->pairs[collisionWorld->pairCount].b = b;
                    ++collisionWorld->pairCount;
                }
            }
        }
    }
    ArenaReset(scratchArena);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Stage 3 — Narrow phase  (exact AABB vs AABB + contact generation)
// ─────────────────────────────────────────────────────────────────────────────
internal void CollisionNarrowPhase(FCollisionWorld* collisionWorld)
{
    collisionWorld->contactCount = 0;

    for (u32 pi = 0; pi < collisionWorld->pairCount; ++pi)
    {
        u32 ai = collisionWorld->pairs[pi].a;
        u32 bi = collisionWorld->pairs[pi].b;

        FCollider* ca = &collisionWorld->colliders.colliders[ai];
        FCollider* cb = &collisionWorld->colliders.colliders[bi];

        if (!AABBOverlap(ca->worldAABB, cb->worldAABB))
        {
            continue;
        }

        // Skip if neither side is solid or trigger.
        if (!(ca->flags & (COLLISION_SOLID_MASK | ECollisionFlags::Trigger)) ||
            !(cb->flags & (COLLISION_SOLID_MASK | ECollisionFlags::Trigger)))
        {
            continue;
        }

        // --- Compute overlap on each axis ---
        f32 overlapX_pos = ca->worldAABB.max.x - cb->worldAABB.min.x;
        f32 overlapX_neg = cb->worldAABB.max.x - ca->worldAABB.min.x;
        f32 overlapY_pos = ca->worldAABB.max.y - cb->worldAABB.min.y;
        f32 overlapY_neg = cb->worldAABB.max.y - ca->worldAABB.min.y;
        f32 overlapZ_pos = ca->worldAABB.max.z - cb->worldAABB.min.z;
        f32 overlapZ_neg = cb->worldAABB.max.z - ca->worldAABB.min.z;

        // Pick the minimum penetration axis (SAT on AABB = just find smallest overlap).
        f32 minOverlapX = (overlapX_pos < overlapX_neg) ? overlapX_pos : overlapX_neg;
        f32 minOverlapY = (overlapY_pos < overlapY_neg) ? overlapY_pos : overlapY_neg;
        f32 minOverlapZ = (overlapZ_pos < overlapZ_neg) ? overlapZ_pos : overlapZ_neg;

        v3  normal = {};
        f32 penetration = 0.0f;

        if ((minOverlapX <= minOverlapY) && (minOverlapX <= minOverlapZ))
        {
            penetration = minOverlapX;
            normal.x = (overlapX_pos < overlapX_neg) ? 1.0f : -1.0f;
        }
        else if ((minOverlapY <= minOverlapX) && (minOverlapY <= minOverlapZ))
        {
            penetration = minOverlapY;
            normal.y = (overlapY_pos < overlapY_neg) ? 1.0f : -1.0f;
        }
        else
        {
            penetration = minOverlapZ;
            normal.z = (overlapZ_pos < overlapZ_neg) ? 1.0f : -1.0f;
        }

        if (collisionWorld->contactCount < MAX_CONTACTS)
        {
            FContactInfo* contact = &collisionWorld->contacts[collisionWorld->contactCount++];
            contact->entityA = ca->hEntity;
            contact->entityB = cb->hEntity;
            contact->normal = normal;
            contact->penetration = penetration;
            contact->isTrigger = (ca->flags & ECollisionFlags::Trigger) ||
                                 (cb->flags & ECollisionFlags::Trigger);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CollisionResolve
//
// Removes overlap by pushing colliders apart along the collision normal.
//
// Mass values are correction weights:
//     Dynamic = 2
//     Physics = 1
//     Static/Kinematic = 0
//
// Correction is distributed inversely by weight, causing heavier objects to
// move less and lighter objects to move more. Static objects never move.
//
// The total correction always equals the penetration depth, fully resolving
// the overlap in a single pass.
// ─────────────────────────────────────────────────────────────────────────────
internal f32 CollisionMass(ECollisionFlags flags)
{
    // Immovable by default -> 0 share.
    f32 mass = 0.0f;

    if (flags & ECollisionFlags::Dynamic)
    {
        mass = 2.0f;
    }
    else if (flags & ECollisionFlags::Physics)
    {
        mass = 1.0f;
    }

    return mass;
}

void CollisionResolve(FCollisionWorld* collisionWorld, FTransformTable* transforms)
{
    for (u32 ci = 0; ci < collisionWorld->contactCount; ++ci)
    {
        FContactInfo* contact = &collisionWorld->contacts[ci];
        if (contact->isTrigger)
        {
            continue;
        }

        FCollider* ca = nullptr;
        FCollider* cb = nullptr;
        for (u32 i = 0; i < collisionWorld->colliders.count; ++i)
        {
            if (collisionWorld->colliders.colliders[i].hEntity == contact->entityA)
            {
                ca = &collisionWorld->colliders.colliders[i];
            }
            if (collisionWorld->colliders.colliders[i].hEntity == contact->entityB)
            {
                cb = &collisionWorld->colliders.colliders[i];
            }
        }
        if (!ca || !cb)
        {
            continue;
        }

        f32 massA = CollisionMass(ca->flags);
        f32 massB = CollisionMass(cb->flags);

        // Both immovable -> nothing to resolve.
        if ((massA == 0.0f) && (massB == 0.0f))
        {
            continue;
        }

        if (contact->penetration <= 0.0f)
        {
            continue;
        }

        // shareA = fraction of penetration applied to A; shareB = fraction applied to B (opposite direction)
        f32 shareA, shareB;
        if ((massA == 0.0f) || (massB == 0.0f))
        {
            shareA = (massA == 0.0f) ? 0.0f : 1.0f;
            shareB = (massB == 0.0f) ? 0.0f : 1.0f;
        }
        else
        {
            f32 totalMass = massA + massB;
            shareA = massB / totalMass;
            shareB = massA / totalMass;
        }

        // Convert penetration depth into world-space movement vectors.
        // shareA + shareB always equals 1, so the full penetration is removed.
        v3 deltaA = { contact->normal.x * contact->penetration * shareA,
                       contact->normal.y * contact->penetration * shareA,
                       contact->normal.z * contact->penetration * shareA };
        v3 deltaB = { contact->normal.x * contact->penetration * shareB,
                       contact->normal.y * contact->penetration * shareB,
                       contact->normal.z * contact->penetration * shareB };

        if (shareA > 0.0f)
        {
            transforms->positions[ca->hTransform].x -= deltaA.x;
            transforms->positions[ca->hTransform].y -= deltaA.y;
            transforms->positions[ca->hTransform].z -= deltaA.z;
        }
        if (shareB > 0.0f)
        {
            transforms->positions[cb->hTransform].x += deltaB.x;
            transforms->positions[cb->hTransform].y += deltaB.y;
            transforms->positions[cb->hTransform].z += deltaB.z;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────
void CollisionInitialize(FCollisionWorld* collisionWorld)
{
    collisionWorld->colliders.count = 0;
    collisionWorld->pairCount = 0;
    collisionWorld->contactCount = 0;

    // Centre the grid on the world origin.
    collisionWorld->grid.originX = -(GRID_WIDTH * GRID_CELL_SIZE * 0.5f);
    collisionWorld->grid.originZ = -(GRID_HEIGHT * GRID_CELL_SIZE * 0.5f);

    for (i32 i = 0; i < GRID_WIDTH * GRID_HEIGHT; ++i)
    {
        collisionWorld->grid.cells[i].count = 0;
    }
}

HCollider CollisionAddCollider(FCollisionWorld* collisionWorld, HEntity hEntity, HTransform hTransform,
    v3 halfExtents, ECollisionFlags flags)
{
    Assert(collisionWorld->colliders.count < MAX_COLLIDERS);
    u32 handle = collisionWorld->colliders.count++;
    FCollider* c = &collisionWorld->colliders.colliders[handle];
    c->hEntity = hEntity;
    c->hTransform = hTransform;
    c->halfExtents = halfExtents;
    c->flags = flags;
    c->worldAABB = {};
    return handle;
}

void CollisionUpdate(FCollisionWorld* collisionWorld, FTransformTable* transforms, FMemoryArena* scracthArena)
{
    CollisionBuildAABBs(collisionWorld, transforms);
    CollisionBroadPhase(collisionWorld, scracthArena);
    CollisionNarrowPhase(collisionWorld);
}

bool32 AreEntitiesColliding(FContactInfo* contactInfo, HEntity hEntityA, HEntity hEntityB)
{

    bool32 areColliding = ((contactInfo->entityA == hEntityA) && (contactInfo->entityB == hEntityB)) ||
                          ((contactInfo->entityA == hEntityB) && (contactInfo->entityB == hEntityA));
    return areColliding;
}
