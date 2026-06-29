// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#include "fado_collision.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

internal b32 AABBOverlap(const FAABB& a, const FAABB& b)
{
    if ((a.max.x < b.min.x) || (a.min.x > b.max.x)) { return false; }
    if ((a.max.y < b.min.y) || (a.min.y > b.max.y)) { return false; }
    if ((a.max.z < b.min.z) || (a.min.z > b.max.z)) { return false; }
    return true;
}

internal FAABB AABBFromTransform(v3 position, v3 scale, v3 halfExtents)
{
    v3 scaledHalf = { halfExtents.x * scale.x,
                      halfExtents.y * scale.y,
                      halfExtents.z * scale.z };
    FAABB result = {};
    result.min = { position.x - scaledHalf.x,
                   position.y - scaledHalf.y,
                   position.z - scaledHalf.z };
    result.max = { position.x + scaledHalf.x,
                   position.y + scaledHalf.y,
                   position.z + scaledHalf.z };
    return result;
}

// Project an OBB's half-extents onto axis (returns the OBB's "radius" along axis).
inline f32 OBBProjectedRadius(const FOBB& box, v3 axis)
{
    f32 r = (Absf32(V3Dot(axis, box.axes[0])) * box.halfExtents.x) +
            (Absf32(V3Dot(axis, box.axes[1])) * box.halfExtents.y) +
            (Absf32(V3Dot(axis, box.axes[2])) * box.halfExtents.z);
    return r;
}

/*
 * Check if 2 OBBs are overlapping using the Separating Axis Theorem(SAT) test for two oriented bounding boxes.
 * Tests 15 candidate separating axes: each box's 3 face normals (6 total),
 * plus all 9 cross products between A's and B's face normals (catches edge-edge separation cases that face axes alone miss).
 * If any axis shows separation, the boxes don't overlap -> return false.
 * Otherwise, tracks the axis with the SMALLEST overlap (the MTV axis),
 * and outputs a normal pointing A -> B plus the penetration depth along it. 
 */
internal b32 OBBOverlap(const FOBB& a, const FOBB& b, v3* outNormal, f32* outPenetration)
{
    v3 axes[15] = {};
    u32 axisCount = 0;

    // 3 face axes of A
    axes[axisCount++] = a.axes[0];
    axes[axisCount++] = a.axes[1];
    axes[axisCount++] = a.axes[2];

    // 3 face axes of B
    axes[axisCount++] = b.axes[0];
    axes[axisCount++] = b.axes[1];
    axes[axisCount++] = b.axes[2];

    // Cross products of each A axis with each B axis (9). Catches edge-vs-edge separation.
    for (u32 i = 0; i < 3; ++i)
    {
        for (u32 j = 0; j < 3; ++j)
        {
            axes[axisCount++] = V3Cross(a.axes[i], b.axes[j]);
        }
    }

    // Vector from A's center to B's center — used to measure how far apart
    // the boxes are along each candidate axis, and to orient the final normal.
    v3  centerDelta = b.center - a.center; // A -> B

    f32 minPenetration = 0.0f;
    v3  minAxis = {};
    b32 first = true;

    for (u32 i = 0; i < axisCount; ++i)
    {
        v3 axis = axes[i];

        // Parallel face axes (e.g. both boxes axis-aligned the same way) produce
        // zero-length cross products. Skip these degenerate axes.
        f32 axisLenSq = V3Dot(axis, axis);
        if (axisLenSq < 0.0001f)
        {
            continue;
        }
        axis = V3Normalize(axis);

        // Project each box's half-extents onto this axis ("radius" of the box along this direction),
        // and measure the distance between centers along the same axis.
        f32 ra = OBBProjectedRadius(a, axis);
        f32 rb = OBBProjectedRadius(b, axis);
        f32 dist = Absf32(V3Dot(centerDelta, axis));

        // If the combined radii are smaller than the center distance, there's
        // a gap on this axis -> the boxes don't overlap at all.
        f32 overlap = (ra + rb) - dist;
        if (overlap <= 0.0f)
        {
            // Separating axis found — no collision.
            return false;
        }

        // Keep track of the axis with the smallest overlap — this is the
        // direction of minimum translation needed to separate the boxes.
        if (first || (overlap < minPenetration))
        {
            minPenetration = overlap;
            minAxis = axis;
            first = false;
        }
    }

    // minAxis points along the MTV line, but its sign is arbitrary (SAT axes
    // have no inherent direction). Orient it so it points from A toward B,
    // matching the convention CollisionResolve expects (A is pushed opposite
    // to normal, B is pushed along normal).
    if (V3Dot(centerDelta, minAxis) < 0.0f)
    {
        minAxis = { -minAxis.x, -minAxis.y, -minAxis.z };
    }

    *outNormal = minAxis;
    *outPenetration = minPenetration;
    return true;
}

internal FOBB OBBFromTransform(v3 position, quat rotation, v3 scale, v3 halfExtents)
{
    FOBB result = {};
    result.center = position;
    result.halfExtents = { halfExtents.x * scale.x,
                            halfExtents.y * scale.y,
                            halfExtents.z * scale.z };
    result.axes[0] = QuatRight(rotation);
    result.axes[1] = QuatUp(rotation);
    result.axes[2] = QuatForward(rotation);
    return result;
}

internal FOBB OBBFromAABB(const FAABB& aabb)
{
    FOBB result = {};
    result.center = { (aabb.min.x + aabb.max.x) * 0.5f,
                       (aabb.min.y + aabb.max.y) * 0.5f,
                       (aabb.min.z + aabb.max.z) * 0.5f };
    result.halfExtents = { (aabb.max.x - aabb.min.x) * 0.5f,
                            (aabb.max.y - aabb.min.y) * 0.5f,
                            (aabb.max.z - aabb.min.z) * 0.5f };
    result.axes[0] = { 1, 0, 0 };
    result.axes[1] = { 0, 1, 0 };
    result.axes[2] = { 0, 0, 1 };
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

// Always builds an AABB that is used for broad faze,
// and builds an OBB if the rotation isn't identity and the collision is solid,
// otherwise use AABB also for the narrow faze.
internal void CollisionBuildAABBsAndOBBs(FCollisionWorld* collisionWorld, FTransformTable* transforms)
{
    for (u32 i = 0; i < collisionWorld->colliders.count; ++i)
    {
        FCollider* collider = &collisionWorld->colliders.colliders[i];

        v3 pos = transforms->positions[collider->hTransform];
        v3 scale = transforms->scales[collider->hTransform];
        quat rot = transforms->rotations[collider->hTransform];

        // Side-scroller: flatten Z so everything sits on XY plane
        if (collider->flags & ECollisionFlags::Is2D)
        {
            scale.z = 1.0f;
        }

        collider->worldAABB = AABBFromTransform(pos, scale, collider->halfExtents);
        
        // Decide if this collider should use OBB in narrow phase.
        collider->useOBB = ((collider->flags & COLLISION_SOLID_MASK) && !IsQuatIdentity(rot));

        if (collider->useOBB)
        {
            collider->worldOBB = OBBFromTransform(pos, rot, scale, collider->halfExtents);
        }
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
    u32* pairTagA = ArenaPushArray(scratchArena, u32, MAX_COLLISION_PAIRS);
    u32* pairTagB = ArenaPushArray(scratchArena, u32, MAX_COLLISION_PAIRS);
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
                b32 found = false;
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
//  Stage 3 — Narrow phase  (AABB on non rotating objects, OBB on rotating ones)
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

        v3  normal = {};
        f32 penetration = 0.0f;

        if (ca->useOBB || cb->useOBB)
        {
            FOBB obbA = ca->useOBB ? ca->worldOBB : OBBFromAABB(ca->worldAABB);
            FOBB obbB = cb->useOBB ? cb->worldOBB : OBBFromAABB(cb->worldAABB);

            if (!OBBOverlap(obbA, obbB, &normal, &penetration))
            {
                continue;
            }
        }

        else
        {
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
    CollisionBuildAABBsAndOBBs(collisionWorld, transforms);
    CollisionBroadPhase(collisionWorld, scracthArena);
    CollisionNarrowPhase(collisionWorld);
}

b8 AreEntitiesColliding(FContactInfo* contactInfo, HEntity hEntityA, HEntity hEntityB)
{

    b8 areColliding = ((contactInfo->entityA == hEntityA) && (contactInfo->entityB == hEntityB)) ||
                          ((contactInfo->entityA == hEntityB) && (contactInfo->entityB == hEntityA));
    return areColliding;
}

b8 RayIntersectsAABB(FRay ray, FAABB aabb, f32* outDistance)
{
    f32 tMin = 0.0f;
    f32 tMax = F32_MAX_VALUE;

    for (i32 axis = 0; axis < 3; ++axis)
    {
        f32 origin = ray.origin.e[axis];
        f32 dir = ray.direction.e[axis];
        f32 minB = aabb.min.e[axis];
        f32 maxB = aabb.max.e[axis];

        if (fabsf(dir) < 1e-8f)
        {
            // Ray parallel to slab — no hit if origin outside slab
            if (origin < minB || origin > maxB) 
            { return false; }
        }
        else
        {
            f32 t1 = (minB - origin) / dir;
            f32 t2 = (maxB - origin) / dir;
            if (t1 > t2)
            {
                f32 tmp = t1;
                t1 = t2;
                t2 = tmp;
            }

            tMin = Max(tMin, t1);
            tMax = Min(tMax, t2);

            if (tMin > tMax)
            { return false; }
        }
    }

    *outDistance = tMin;
    return true;
}
