// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#include "fado.h"
#include "fado_math.h"
#include "fado_collision.h"

// Adds an entity to the entity table and gives it a transform.
//  - No dynamic allocation of any sorts, just setting values to an existing array.
internal HEntity SpawnEntity(FSharedStuff* shared, HMesh hMesh, HTexture hTex, v4 color, EShaderTypes shaderType)
{
    FEntityTable* entityTable = shared->entityTable;
    FTransformTable* transforms = shared->transforms;

    HEntity handle = entityTable->count++;
    FEntity* e = &entityTable->entities[handle];
    e->hMesh = hMesh;
    e->hTexture = hTex;
    e->hTransform = transforms->count++;
    e->color = color;
    e->shaderType = shaderType;
    transforms->scales[e->hTransform] = V3One();
    transforms->rotations[e->hTransform] = QuatIdentity();
    return handle;
}

// Called once when the game starts
internal void Initialize(FGameState* gameState)
{
    gameState->initialized = true;

    FSharedStuff* shared = gameState->shared;
    shared->canSelect = true;

    FTransformTable* transforms = shared->transforms;
    FEntityTable* entityTable = shared->entityTable;

    CollisionInitialize(shared->collisionWorld);
    v3 extents = { 1.0f, 1.0f, 1.0f };

    // >> IMPORTANT: Camera MUST be handle 0!
    shared->camera.handle = SpawnEntity(shared, INVALID_HANDLE, INVALID_HANDLE, {}, EShaderTypes::Shader_None);
    shared->transforms->positions[shared->camera.handle] = { 0.0f, 2.5f, -10.0f };
    CollisionAddCollider(shared->collisionWorld, shared->camera.handle,
        entityTable->entities[shared->camera.handle].hTransform,
        { 1.0f, 1.0f, 1.0f }, ECollisionFlags::Physics);

    // infinite plane
    gameState->infinitePlane = SpawnEntity(shared, gameState->hPlaneMesh, gameState->hGridTexture, {}, EShaderTypes::LitTexture);
    transforms->scales[gameState->infinitePlane] = { 1000.0f, 1.0f, 1000.0f };
    CollisionAddCollider(shared->collisionWorld, gameState->infinitePlane,
        entityTable->entities[gameState->infinitePlane].hTransform,
        { 1.0f, 0.01f, 1.0f }, ECollisionFlags::Static);

    // sky box
    gameState->skyBox = SpawnEntity(shared, gameState->hSkyBoxMesh, gameState->hSkyBoxTexture, {}, EShaderTypes::UnlitTexture);
    transforms->scales[gameState->skyBox] = {500.0f, 500.0f, 500.0f };

    // Other entities
    gameState->cube1 = SpawnEntity(shared, gameState->hCubeMesh, 0, { 0.63f, 1, 0.21f, 1 }, EShaderTypes::Color);
    transforms->positions[gameState->cube1] = { -3.5f, 5.0f, 0 };
    transforms->scales[gameState->cube1] = { 2.5f, 0.25f, 1.0f };
    CollisionAddCollider(shared->collisionWorld, gameState->cube1,
        GetTransformHandle(entityTable, gameState->cube1), extents, ECollisionFlags::Kinematic);

    gameState->cube2 = SpawnEntity(shared, gameState->hCubeMesh, 0, { 1, 0.21f, 0.63f, 1 }, EShaderTypes::Color);
    transforms->positions[gameState->cube2] = { 1.5f, 5.0f, 0 };
    CollisionAddCollider(shared->collisionWorld, gameState->cube2,
        GetTransformHandle(entityTable, gameState->cube2), extents, ECollisionFlags::Static);

    gameState->sphere1 = SpawnEntity(shared, gameState->hSphereMesh, gameState->hGraniteTexture, {}, EShaderTypes::LitTexture);
    transforms->positions[gameState->sphere1] = { -1.5f, 2.0f, 0 };
    CollisionAddCollider(shared->collisionWorld, gameState->sphere1,
        GetTransformHandle(entityTable, gameState->sphere1), extents, ECollisionFlags::Dynamic);

    gameState->sphere2 = SpawnEntity(shared, gameState->hSphereMesh, gameState->hMosaicTexture, {}, EShaderTypes::LitTexture);
    transforms->positions[gameState->sphere2] = { 1.5f, 2.0f, 0 };
    CollisionAddCollider(shared->collisionWorld, gameState->sphere2,
        GetTransformHandle(entityTable, gameState->sphere2), extents, ECollisionFlags::Physics);

    gameState->fire = SpawnEntity(shared, gameState->hCubeMesh, 0, {1, 0, 0, 1}, EShaderTypes::Color);
    CollisionAddCollider(shared->collisionWorld, gameState->fire,
        GetTransformHandle(entityTable, gameState->fire), extents, ECollisionFlags::Physics);
    HTransform hFireTransform = GetTransformHandle(entityTable, gameState->fire);
    transforms->positions[hFireTransform] = { 5.0f, 2.0f, 0 };
    transforms->scales[hFireTransform] = { 0.25f, 0.25f, 0.25f };
    gameState->hFireSFXInstance = SoundPlay3D(gameState->soundManager, gameState->hFireSFX, ESoundCategory::SFX, 1.0f, true, transforms->positions[hFireTransform], 0.0f, 5.0f);

    // Sound
    SoundPlay2D(gameState->soundManager, gameState->hMusic, ESoundCategory::Music, 0.5f, true);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- UI --

internal b8 IsInputButtonClick(FGameButtonState* button);   // forward

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
        clicked = hovered && IsInputButtonClick(&input->controllers[1].actionDown);
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
        SoundPlay2D(gameState->soundManager, gameState->hUIClickSFX, ESoundCategory::UI, 0.25f, false);
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
    if (UIButton(gameState, input, rect, "Fado Engine", &buttonStyle))
    {
        UIPushText(uiBucket, gameState->font, "Button Is Working !!! :))", textPos, textColor);
    }
    rect.y += rect.height + buttonsYOffset;
    if (UIButton(gameState, input, rect, "Is The", &buttonStyle))
    {
        UIPushText(uiBucket, gameState->font, "Button Is Working !!! :))", textPos, textColor);
    }
    rect.y += rect.height + buttonsYOffset;
    if (UIButton(gameState, input, rect, "Best!!!", &buttonStyle))
    {
        UIPushText(uiBucket, gameState->font, "Button Is Working !!! :))", textPos, textColor);
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Input --

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
    return collisions->colliders.colliders[closestIndex].hEntity;
}

// Return true if a button was just clicked.
// TODO: Move common input functions to their own file.
internal b8 IsInputButtonClick(FGameButtonState* button)
{
    b8 isClick = (button->isDown && !button->wasDown);
    return isClick;
}

// Return true if a button was just clicked.
internal b8 IsInputButtonHeld(FGameButtonState* button)
{
    b8 isHeld = (button->isDown && button->wasDown);
    return isHeld;
}

// Checks if a stick is held in a specific directions
internal b8 IsStickHeld(v2 stickAverage, EStickDirection direction)
{
    b8 result = false;
    f32 threshHold = 0.5f;

    switch (direction)
    {
    case EStickDirection::Up:
    {
        result = stickAverage.y > threshHold;
    } break;

    case EStickDirection::Down:
    {
        result = stickAverage.y < -threshHold;
    } break;

    case EStickDirection::Left:
    {
        result = stickAverage.x < -threshHold;
    } break;

    case EStickDirection::Right:
    {
        result = stickAverage.x > threshHold;
    } break;

    default:
    {} break;
    }

    return result;
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

internal void HandleGameInput(FGameState* gameState, FGameInput* input)
{
    FSharedStuff* shared = gameState->shared;

    for (u32 controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
    {
        FGameControllerInput* controllerInput = &input->controllers[controllerIndex];
        if (!controllerInput->isConnected)
        {
            return;
        }

        // Movement
        FCamera* cam = &shared->camera;
        v3 forward = cam->forward;
        v3 right = cam->right;
        v3 up = cam->up;

        f32 moveSpeed = 10.0f * input->deltaTime;
        v3* movedPos = &shared->transforms->positions[shared->camera.handle];
        if (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Up) || IsInputButtonHeld(&controllerInput->dpadUp))
        {
            *movedPos += forward * moveSpeed;
        }
        if (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Down) || IsInputButtonHeld(&controllerInput->dpadDown))
        {
            *movedPos -= forward * moveSpeed;
        }
        if (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Right) || IsInputButtonHeld(&controllerInput->dpadRight))
        {
            *movedPos += right * moveSpeed;
        }
        if (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Left) || IsInputButtonHeld(&controllerInput->dpadLeft))
        {
            *movedPos -= right * moveSpeed;
        }
        if (controllerInput->rightShoulder.isDown)
        {
            *movedPos += up * moveSpeed;
        }
        if (controllerInput->leftShoulder.isDown)
        {
            *movedPos -= up * moveSpeed;
        }

        // Rotation
        f32 sensitivity = 100.0f * input->deltaTime;
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Up))
        {
            gameState->cameraPitch -= sensitivity;
        }
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Down))
        {
            gameState->cameraPitch += sensitivity;
        }
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Right))
        {
            gameState->cameraYaw += sensitivity;
        }
        if (IsStickHeld(controllerInput->rightStickAverage, EStickDirection::Left))
        {
            gameState->cameraYaw -= sensitivity;
        }
        gameState->cameraPitch = Clampf32(gameState->cameraPitch, -89.0f, 89.0f);
        SetRotation(shared->transforms, shared->camera.handle,
            { gameState->cameraPitch, gameState->cameraYaw, 0 });
       
        // Mouse Rotation
        // Mouse deltaY maps to pitch and deltaX maps to yaw.
        // The sensitivity value is much smaller than the controller one because mouse deltas are in pixels, not a -1 to 1 range.
        f32 mouseSensitivity = 0.1f;
        f32 mouseYaw = input->mouse.deltaX * mouseSensitivity;
        f32 mousePitch = input->mouse.deltaY * mouseSensitivity;

        // Clamp to prevent gimbal lock at the poles. When we pitch up or down past 90 degrees, 
        // the camera flips because there's no restriction on how far you can pitch.
        gameState->cameraPitch += Clampf32(mousePitch, -89.0f, 89.0f);
        gameState->cameraYaw += mouseYaw;

        // We rotate with mouse only if the mouse right-click is down.
        if ((mouseYaw != 0 || mousePitch != 0) && (input->mouse.isRotating))
        {
            // Rebuild quat from the two angles.
            SetRotation(shared->transforms, shared->camera.handle,
                { gameState->cameraPitch, gameState->cameraYaw, 0 });
        }

        // UI input
        if (IsInputButtonClick(&controllerInput->dpadDown))
        {
            UINavigateNext(&gameState->uiNavState, true);
        }
        if (IsInputButtonClick(&controllerInput->dpadUp))
        {
            UINavigateBack(&gameState->uiNavState, true);
        }

        if (controllerInput->back.isDown)
        {
            gameState->running = false;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────

// Main game update function, the only function that exported the engine (.exe).
extern "C" __declspec(dllexport)
GAME_UPDATE(GameUpdate)
{
    if (!gameState->initialized)
    {
        Initialize(gameState);
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
    if (IsInputButtonClick(&input->mouse.buttons[0]) && shared->canSelect)
    {
        FRay ray = ScreenPointToRay(gameState->shared, (f32)input->mouse.x, (f32)input->mouse.y);
        i32 picked = PickEntity(ray, shared->collisionWorld);
        if (picked >= 0)
        {
            shared->selectedEntity = picked;
        }
    }
#endif // FADO_DEBUG

	HandleGameInput(gameState, input);
    UpdateUI(gameState, input);

    // The infinite plane follows the camera to give the illusion of infinite stretch.
    shared->transforms->positions[gameState->infinitePlane].x = shared->transforms->positions[shared->camera.handle].x;
    shared->transforms->positions[gameState->infinitePlane].z = shared->transforms->positions[shared->camera.handle].z;

    // The skybox follows the camera to give the of infinite sky.
    FEntity* skybox = GetEntity(shared->entityTable, gameState->skyBox);
    shared->transforms->positions[skybox->hTransform] = shared->transforms->positions[shared->camera.handle];


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
            SoundPlay2D(gameState->soundManager, gameState->hCollideSFX, ESoundCategory::SFX, 0.1f, false);
        }
    }

    // Update the fire sfx pos to match the fire entity's.
    gameState->soundManager->assetBank->instances[gameState->hFireSFXInstance].position = shared->transforms->positions[GetTransformHandle(shared->entityTable, gameState->fire)];
}