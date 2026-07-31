// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#include "fado.h"
#include "fado_math.h"
#include "fado_assets.h"
#include "fado_input.h"
#include "fado_level.h"
#include "Levels/level_countdown.h"

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- UI --
// Currently shared between both showcase levels, so it lives here.

// Creates a stylized button and pushes it to ui bucket.
// Returns if the button was clicked.
u32 UIButton(FGameState* gameState, FGameInput* input, v4 rect, cc8* text, FUIButtonStyle* style)
{
    FUI* ui = &gameState->shared->ui;

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
        v4 scaledRect = UIScaleRect(rect, ui->scale); // screen space
        hovered = UIPointInRect(mousePos, scaledRect);
        clicked = hovered && input->mouse.buttons[0].isDown && !input->mouse.buttons[0].wasDown;
    }

    UIPushButton(rect, text, ui, style, gameState->font, clicked, hovered);

    if (clicked)
    {
        SoundPlay2D(gameState->soundManager, gameState->shared->assets.hUIClickSFX, ESoundCategory::Sound_UI, 0.25f, false);
    }

    return clicked;
}

// ──────────────────────────────────────────────────────────────────────────────────────────

#if FADO_DEBUG
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
    ray.origin = shared->transforms.positions[cam->handle];
    ray.direction = V3Normalize(dir);
    return ray;
}
#endif // FADO_DEBUG

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

#if FADO_DEBUG
        gameState->shared->canSelect = true;
#endif // FADO_DEBUG

        FDirectionalLight* dirLight = &gameState->shared->dirLight;
        dirLight->ambientColor = { 0.5f, 0.35f, 0.25f, 1.0f };
        dirLight->diffuseColor = { 1.75f, 1.0f, 1.0f, 1.0f };
        dirLight->lightDirection = { 1.75f, -1.0f, 1.0f };

        LoadLevel(gameState, SetupLevel_Countdown());
    }

    if (gameState->currentLevel->Update)
    {
        gameState->currentLevel->Update(gameState, gameState->input->deltaTime);
    }

#if FADO_DEBUG
    // Clicking on entites
    FSharedStuff* shared = gameState->shared;
    if (Pressed(&input->mouse.buttons[0]) && shared->canSelect)
    {
        FRay ray = ScreenPointToRay(gameState->shared, (f32)input->mouse.x, (f32)input->mouse.y);
        i32 picked = PickEntity(ray, &shared->collisionWorld);
        if (picked >= 0)
        {
            shared->selectedEntity = picked;
        }
    }
#endif // FADO_DEBUG
}

// ────────────────────────────────────────────────────────────────────────