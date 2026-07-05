// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#include "fado.h"
#include "fado_math.h"
#include "fado_input.h"
#include "fado_collision.h"
#include "fado_level.h"

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- UI --

// Creates a stylized button and pushes it to ui bucket.
// Returns if the button was clicked.
internal u32 UIButton(FGameState* gameState, FGameInput* input, v4 rect, cc8* text, FUIButtonStyle* style)
{
    FUINavState* nav = &gameState->uiNavState;
    i32 myIndex = nav->buttonCount++;

    b8 hovered = false;
    b8 clicked = false;

    if (input->controllers[1].isConnected)
    {
        hovered = (myIndex == nav->focusedIndex);
        clicked = hovered && Pressed(&input->controllers[1].actionDown);
    }
    else
    {
        v2 mousePos = { (f32)input->mouse.x, (f32)input->mouse.y };
        hovered = UIPointInRect(mousePos, rect);
        clicked = hovered && input->mouse.buttons[0].isDown && !input->mouse.buttons[0].wasDown;
    }

    FUICommandBucket* bucket = gameState->shared->uiCommands;
    UIPushButton(rect, text, bucket, style, gameState->font, clicked, hovered);

    if (clicked)
    {
        SoundPlay2D(gameState->soundManager, gameState->hUIClickSFX, ESoundCategory::Sound_UI, 0.25f, false);
    }

    return clicked;
}

internal void UpdateUI(FGameState* gameState, FGameInput* input)
{
    gameState->uiNavState.buttonCount = 0;

    FUICommandBucket* uiBucket = gameState->shared->uiCommands;

    f32 buttonsYOffset = 20.0f;
    v4 rect = { 350, 450, 200, 65 };
    FUIButtonStyle buttonStyle = {
        /*idle*/    { 0.251f, 0.596f, 0.369f, 1.0f },
        /*hover*/   { 0.82f, 0.796f, 0.584f, 1.0f },
        /*pressed*/ { 0.102f, 0.392f, 0.306f, 1.0f },
        /*text*/    { 0.039f, 0.102f, 0.184f, 1 },
        gameState->hWhiteTexture
    };

    v2 textPos = { 500, 500 };
    v4 textColor = { 1, 0.5f, 1, 1 };
    if (UIButton(gameState, input, rect, "Save Level", &buttonStyle))
    {
        if (SaveCurrentLevel(gameState))
        {
            UIPushText(uiBucket, gameState->font, "Level Saved", textPos, { 0, 1, 0, 1 });
        }
    }
    rect.y += rect.height + buttonsYOffset;
    if (UIButton(gameState, input, rect, "Load 3D Level", &buttonStyle))
    {
        LoadLevelById(gameState, Level_01);
    }
    rect.y += rect.height + buttonsYOffset;
    if (UIButton(gameState, input, rect, "Load 2D Level", &buttonStyle))
    {
        LoadLevelById(gameState, Level_02);
    }

    rect.y += rect.height + buttonsYOffset;
    if (UIButton(gameState, input, rect, "Switch Camera", &buttonStyle))
    {
        FCamera* cam = &gameState->shared->camera;

        if (cam->type == Camera_Perspective)
        {
            cam->type = Camera_Orthographic;
        }
        else
        {
            cam->type = Camera_Perspective;
        }

        SetGamePaused(gameState, false);

    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────

// Returns the handle of the closest entity that the ray passed through.
// The entity must have a collider for it to register.
internal HEntity PickEntity(FRay ray, FCollisionWorld* collisions)
{
    i32 closestIndex = 0;
    f32 closestDist = F32_MAX_VALUE;

    // start from 1 to skip the camera
    for (u32 i = 1; i < collisions->colliders.count; ++i)
    {
        FAABB aabb = collisions->colliders.colliders[i].worldAABB;
        f32 dist;
        if (RayIntersectsAABB(ray, aabb, &dist))
        {
            if (dist < closestDist)
            {
                closestDist = dist;
                closestIndex = i;
            }
        }
    }
    return collisions->colliders.colliders[closestIndex].entityID;
}

// Creates a ray from the mouse position.
internal FRay ScreenPointToRay(FSharedStuff* shared, f32 mouseX, f32 mouseY)
{
    FCamera* cam = &shared->camera;

    // Convert mouse coordinates to normalized device coordinates [-1, 1].
    // Screen space: (0..width, 0..height)
    // NDC space:    (-1..1, -1..1)
    f32 ndcX = (2.0f * mouseX) / shared->viewport.width - 1.0f;
    f32 ndcY = 1.0f - (2.0f * mouseY) / shared->viewport.height; // flip Y

    // Compute the size of the camera view plane at unit distance.
    f32 halfHeight = tanf(cam->fovY * 0.5f);
    f32 halfWidth = halfHeight * cam->aspect;

    // Build a ray through the mouse position on the view plane.
    v3 dir = cam->forward + 
             (cam->right * (ndcX * halfWidth)) +
             (cam->up * (ndcY * halfHeight));

    FRay ray = {};
    ray.origin = shared->transforms->positions[cam->handle];
    ray.direction = V3Normalize(dir);
    return ray;
}

// ──────────────────────────────────────────────────────────────────────────────────────────

void SetGamePaused(FGameState* gameState, b8 pause)
{
    gameState->paused = pause;
    gameState->input->mode = pause ? Input_UI : Input_Game;
}

// ──────────────────────────────────────────────────────────────────────────────────────────

// Main game update function, the only function that exported the engine (.exe).
extern "C" __declspec(dllexport)
GAME_UPDATE(GameUpdate)
{
    if (!gameState->initialized)
    {
        gameState->initialized = true;
        input->mode = Input_Game;

#if FADO_DEBUG
        gameState->shared->canSelect = true;
#endif // FADO_DEBUG

        // Load level 01 by default.
        LoadLevelById(gameState, Level_01);
    }

    FSharedStuff* shared = gameState->shared;

    // Each frame, feed camera into the listener for 3D audio.
    FSoundListener listener = {};
    v3 camPos = shared->transforms->positions[shared->camera.handle];
    listener.position = camPos;
    listener.forward = shared->camera.forward;
    listener.up = shared->camera.up;
    gameState->soundManager->listener = listener;

#if FADO_DEBUG
    // Clicking on entites
    if (Pressed(&input->mouse.buttons[0]) && shared->canSelect)
    {
        FRay ray = ScreenPointToRay(gameState->shared, (f32)input->mouse.x, (f32)input->mouse.y);
        i32 picked = PickEntity(ray, shared->collisionWorld);
        if (picked >= 0)
        {
            shared->selectedEntity = picked;
        }
    }
#endif // FADO_DEBUG

	HandleInput(gameState, input);

    if (gameState->paused)
    {
        UpdateUI(gameState, input);
    }

    // The infinite plane follows the camera to give the illusion of infinite stretch.
    shared->transforms->positions[gameState->infinitePlane].x = shared->transforms->positions[shared->camera.handle].x;
    shared->transforms->positions[gameState->infinitePlane].z = shared->transforms->positions[shared->camera.handle].z;

    // The skybox follows the camera to give the of infinite sky.
    FEntity* skybox = &shared->entities[gameState->skyBox];
    //shared->transforms->positions[skybox->hTransform].x = shared->transforms->positions[shared->camera.handle].x;

    Rotate(shared->transforms, gameState->cube1, { 50.0f*input->deltaTime, 50.0f * input->deltaTime, 0.0f });
    Rotate(shared->transforms, gameState->cube2, { -50.0f * input->deltaTime, 0.0f, 0.0f });
    Rotate(shared->transforms, gameState->sphere1, { 0.0f, 50.0f * input->deltaTime, 0.0f });
    Rotate(shared->transforms, gameState->sphere2, { 0.0f, -50.0f * input->deltaTime, 0.0f });

    //  -- Test and update collisions --
    // 1. Calculate and detect.
    CollisionUpdate(shared->collisionWorld, shared->transforms, shared->scratchArena);
    // 2. Resolve (push solid objects apart).
    CollisionResolve(shared->collisionWorld, shared->transforms);
    // 3. React (Iterate contacts for game logic).
    for (u32 i = 0; i < shared->collisionWorld->contactCount; ++i)
    {
        FContactInfo* c = &shared->collisionWorld->contacts[i];
        if (c->entityA == gameState->shared->camera.handle || c->entityB == gameState->shared->camera.handle)
        {
            SoundPlay2D(gameState->soundManager, gameState->hCollideSFX, ESoundCategory::Sound_SFX, 0.1f, false);
        }
    }

    // Update the fire sfx pos to match the fire entity's.
    Update3DSoundsPositions(gameState->soundManager->assetBank, shared);
}
