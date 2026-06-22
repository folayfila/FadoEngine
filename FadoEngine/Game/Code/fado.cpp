#include "fado.h"
#include "fado_math.h"
#include "fado_collision.h"

internal HEntity SpawnEntity(FEntityTable* entities, FTransformTable* transforms, HMesh hMesh, HTexture hTex, v4 color, EShaderTypes shaderType)
{
    HEntity handle = entities->count++;
    FEntity* e = &entities->entities[handle];
    e->hMesh = hMesh;
    e->hTexture = hTex;
    e->hTransform = transforms->count++;
    e->color = color;
    e->shaderType = shaderType;
    transforms->scales[e->hTransform] = V3One();
    transforms->rotations[e->hTransform] = QuatIdentity();
    return handle;
}

internal void Initialize(FGameState* gameState)
{
    gameState->initialized = true;

    FSharedStuff* shared = gameState->shared;
    shared->canSelect = true;

    // >> IMPORTANT: Camera MUST be handle 0!
    shared->camera.handle = SpawnEntity(shared->entityTable, shared->transforms, INVALID_HANDLE, INVALID_HANDLE, {}, EShaderTypes::Shader_None);
    shared->transforms->positions[shared->camera.handle] = { 0.0f, 2.5f, -10.0f };

    gameState->infinitePlane = SpawnEntity(shared->entityTable, shared->transforms, gameState->hPlaneMesh, gameState->hGridTexture, {}, EShaderTypes::UnlitTexture);
    shared->transforms->scales[gameState->infinitePlane] = { 1000.0f, 1.0f, 1000.0f };

    gameState->skyBox = SpawnEntity(shared->entityTable, shared->transforms, gameState->hCubeMesh, gameState->hSkyBoxTexture, {}, EShaderTypes::UnlitTexture);
    shared->transforms->positions[gameState->skyBox] = { 10.0f, 10.0f, 10.0f };

    gameState->cube1 = SpawnEntity(shared->entityTable, shared->transforms, gameState->hCubeMesh, 0, { 0.63f, 1, 0.21f, 1 }, EShaderTypes::Color);
    gameState->cube2 = SpawnEntity(shared->entityTable, shared->transforms, gameState->hCubeMesh, 0, { 1, 0.21f, 0.63f, 1 }, EShaderTypes::Color);
    gameState->sphere1 = SpawnEntity(shared->entityTable, shared->transforms, gameState->hSphereMesh, gameState->hGraniteTexture, {}, EShaderTypes::LitTexture);
    gameState->sphere2 = SpawnEntity(shared->entityTable, shared->transforms, gameState->hSphereMesh, gameState->hMosaicTexture, {}, EShaderTypes::LitTexture);

    shared->transforms->positions[gameState->cube1] = { -3.5f, 5.0f, 0 };
    shared->transforms->scales[gameState->cube1] = { 2.5f, 0.25f, 1.0f };
    shared->transforms->positions[gameState->cube2] = { 1.5f, 5.0f, 0 };
    shared->transforms->positions[gameState->sphere1] = { -1.5f, 2.0f, 0 };
    shared->transforms->positions[gameState->sphere2] = { 1.5f, 2.0f, 0 };

    CollisionInitialize(shared->collisionWorld);

    CollisionAddCollider(shared->collisionWorld, shared->camera.handle,
        shared->entityTable->entities[shared->camera.handle].hTransform,
        { 1.0f, 1.0f, 1.0f }, ECollisionFlags::Physics);

    CollisionAddCollider(shared->collisionWorld, gameState->infinitePlane,
        shared->entityTable->entities[gameState->infinitePlane].hTransform,
        { 1.0f, 0.01f, 1.0f }, ECollisionFlags::Static);

    v3 extents = {1.0f, 1.0f, 1.0f};
    CollisionAddCollider(shared->collisionWorld, gameState->sphere1,
        shared->entityTable->entities[gameState->sphere1].hTransform, extents, ECollisionFlags::Dynamic);
    CollisionAddCollider(shared->collisionWorld, gameState->sphere2,
        shared->entityTable->entities[gameState->sphere2].hTransform, extents, ECollisionFlags::Physics);
    CollisionAddCollider(shared->collisionWorld, gameState->cube1,
        shared->entityTable->entities[gameState->cube1].hTransform, extents, ECollisionFlags::Kinematic);
    CollisionAddCollider(shared->collisionWorld, gameState->cube2,
        shared->entityTable->entities[gameState->cube2].hTransform, extents, ECollisionFlags::Static);

    CollisionAddCollider(shared->collisionWorld, gameState->skyBox,
        shared->entityTable->entities[gameState->skyBox].hTransform, extents, ECollisionFlags::Static);
}

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
    {
    } break;
    }

    return result;
}

internal i32 PickEntity(FRay ray, FCollisionWorld* collisions)
{
    i32 closestIndex = 0;
    f32 closestDist = MAX_FLOAT;

    // start from 1 to skip the camera
    for (u32 i = 1; i < collisions->colliders.count; ++i)
    {
        v3 aabbMin = collisions->colliders.colliders[i].worldAABB.min; // adjust to your actual AABB storage
        v3 aabbMax = collisions->colliders.colliders[i].worldAABB.max;

        f32 dist;
        if (RayIntersectsAABB(ray, aabbMin, aabbMax, &dist))
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

internal b8 IsInputButtonClick(FGameButtonState* button)
{
    b8 isClick = (button->isDown && !button->wasDown);
    return isClick;
}

// Returns true on the frame the button was clicked.
internal b8 UIButton(FUICommandBucket* bucket, FFont* font, FGameInput* input, FUINavState* nav, v4 rect, cc8* text, FUIButtonStyle* style)
{
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

    v4 color = style->idleColor;
    if (clicked)
    {
        color = style->pressedColor;
    }
    else if (hovered)
    {
        color = style->hoverColor;
    }

    UIPushRect(bucket, rect, { 0, 0, 1, 1 }, color, style->texture);

    // > TODO: Replace with real text measurement later
    f32 textX = rect.x + 20;
    f32 textY = rect.y + rect.height * 0.5f;
    UIPushText(bucket, font, text, { textX, textY }, style->textColor);

    return clicked;
}

internal void UpdateUI(FGameState* gameState, FGameInput* input)
{
    gameState->uiNavState.buttonCount = 0;

    FUICommandBucket* uiBucket = gameState->shared->uiCommands;

    f32 buttonsYOffset = 20.0f;
    v4 rect = { 350, 450, 200, 75 };
    FUIButtonStyle buttonStyle = {
        /*idle*/    { 0.251f, 0.596f, 0.369f, 1.0f },
        /*hover*/   { 0.82f, 0.796f, 0.584f, 1.0f },
        /*pressed*/ { 0.102f, 0.392f, 0.306f, 1.0f },
        /*text*/    { 0.039f, 0.102f, 0.184f, 1 },
        gameState->hWhiteTexture
    };
    if (UIButton(uiBucket, gameState->font, input, &gameState->uiNavState, rect, "Fado Engine", &buttonStyle))
    {
        v2 textPos = { 15, 15 };
        v4 textColor = { 1, 0.5f, 1, 1 };
        UIGuiPushText(uiBucket, textPos, textColor, "Button Is Working !!! :))");
    }
    rect.y += rect.height + buttonsYOffset;
    if (UIButton(uiBucket, gameState->font, input, &gameState->uiNavState, rect, "Is The", &buttonStyle))
    {
        v2 textPos = { 15, 15 };
        v4 textColor = { 1, 0.5f, 1, 1 };
        UIGuiPushText(uiBucket, textPos, textColor, "Button Is Working !!! :))");
    }
    rect.y += rect.height + buttonsYOffset;
    if (UIButton(uiBucket, gameState->font, input, &gameState->uiNavState, rect, "Best!!!", &buttonStyle))
    {
        v2 textPos = { 15, 15 };
        v4 textColor = { 1, 0.5f, 1, 1 };
        UIGuiPushText(uiBucket, textPos, textColor, "Button Is Working !!! :))");
    }
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

        quat camRot = shared->transforms->rotations[shared->camera.handle];
        v3* camPos = &shared->transforms->positions[shared->camera.handle];

        v3* sphere1Pos = &shared->transforms->positions[gameState->sphere1];

        FCamera* cam = &shared->camera;
        v3 forward = cam->forward;
        v3 right = cam->right;
        v3 up = cam->up;

        // Movement
        f32 moveSpeed = 10.0f * input->deltaTime;
        if (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Up))
        {
            *camPos += forward * moveSpeed;
            //*sphere1Pos += forward * moveSpeed;
        }
        if (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Down))
        {
            *camPos -= forward * moveSpeed;
            //*sphere1Pos -= forward * moveSpeed;
        }
        if (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Right))
        {
            *camPos += right * moveSpeed;
            //*sphere1Pos += right * moveSpeed;
        }
        if (IsStickHeld(controllerInput->leftStickAverage, EStickDirection::Left))
        {
            *camPos -= right * moveSpeed;
            //*sphere1Pos -= right * moveSpeed;
        }
        if (controllerInput->rightShoulder.isDown)
        {
            *camPos += up * moveSpeed;
            //*sphere1Pos += up * moveSpeed;
        }
        if (controllerInput->leftShoulder.isDown)
        {
            *camPos -= up * moveSpeed;
            //*sphere1Pos -= up * moveSpeed;
        }

        if (IsInputButtonClick(&controllerInput->dpadDown))
        {
            UINavigateNext(&gameState->uiNavState, true);
        }
        if (IsInputButtonClick(&controllerInput->dpadUp))
        {
            UINavigateBack(&gameState->uiNavState, true);
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

        if (controllerInput->back.isDown)
        {
            gameState->running = false;
        }
    }
}

internal FRay ScreenPointToRay(FSharedStuff* shared, f32 mouseX, f32 mouseY)
{
    FCamera* cam = &shared->camera;

    // Convert to NDC [-1, 1]
    f32 ndcX = (2.0f * mouseX) / shared->viewport.width - 1.0f;
    f32 ndcY = 1.0f - (2.0f * mouseY) / shared->viewport.height; // flip Y

    f32 halfHeight = tanf(cam->fovY * 0.5f);
    f32 halfWidth = halfHeight * cam->aspect;

    v3 dir = cam->forward + (cam->right * (ndcX * halfWidth)) + (cam->up * (ndcY * halfHeight));

    FRay ray = {};
    ray.origin = shared->transforms->positions[cam->handle];
    ray.direction = V3Normalize(dir);
    return ray;
}

extern "C" __declspec(dllexport)
GAME_UPDATE(GameUpdate)
{
    if (!gameState->initialized)
    {
        Initialize(gameState);
    }

    FSharedStuff* shared = gameState->shared;

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

	HandleGameInput(gameState, input);
    UpdateUI(gameState, input);

    shared->transforms->positions[gameState->infinitePlane].x = shared->transforms->positions[shared->camera.handle].x;
    shared->transforms->positions[gameState->infinitePlane].z = shared->transforms->positions[shared->camera.handle].z;

    Rotate(shared->transforms, gameState->cube1, { 50.0f*input->deltaTime, 50.0f * input->deltaTime, 0.0f });
    Rotate(shared->transforms, gameState->cube2, { -50.0f * input->deltaTime, 0.0f, 0.0f });
    Rotate(shared->transforms, gameState->sphere1, { 0.0f, 50.0f * input->deltaTime, 0.0f });
    Rotate(shared->transforms, gameState->sphere2, { 0.0f, -50.0f * input->deltaTime, 0.0f });

    // Test and update collisions.
    // 1. Calculate and detect.
    CollisionUpdate(shared->collisionWorld, shared->transforms, shared->scratchArena);
    // 2. Resolve (push solid objects apart).
    CollisionResolve(shared->collisionWorld, shared->transforms);
    // 3. React (Iterate contacts for game logic).
    for (u32 i = 0; i < shared->collisionWorld->contactCount; ++i)
    {
        FContactInfo* c = &shared->collisionWorld->contacts[i];
        if (c->isTrigger)
        {
            // Example: Doing something specific if sphere1 collides with sphere2
            if (AreEntitiesColliding(c, gameState->sphere1, gameState->sphere2))
            {

            }
        }
    }
}

/*
* Quick debug messasge
#include <stdio.h>
#include <windows.h>
    for (i32 i = 0; i < ArrayCount(controllerInput->buttons); ++i)
    {
        if (controllerInput->buttons[i].isDown)
        {
            char logBuffer[256];
            sprintf_s(logBuffer, "Button %i is down, Held Time: %f\n", i, controllerInput->buttons[i].heldLength);
            OutputDebugStringA(logBuffer);
        }
    }
*/