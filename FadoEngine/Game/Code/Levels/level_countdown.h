// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef LEVEL_COUNTDOWN
#define LEVEL_COUNTDOWN

#include "fado.h"
#include "fado_level.h"
#include "fado_input.h"
#include "fado_collision.h"
#include "fado_particles.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#pragma warning(push)
#pragma warning(disable : 26437)

// ────────────────────────────────────────
// -- Level_Countdown --

enum ELevel_Countdown_State
{
    Level_Countdown_State_MainMenu,
    Level_Countdown_State_Game
};

enum EMiniGame_State
{
    MiniGame_Init,
    MiniGame_Countdown,
    MiniGame_Update,
    MiniGame_End
};

enum EMiniGames
{
    MG_Race,
    MG_Parachute,
    MG_Rocket,
    MG_Coffee,
    MG_RedLight,
    MG_Basketball,
    MG_Bull,
    MG_Movie,
    MG_Bomb,
    MG_Camera,

    EMiniGames_Size
};

// ────────────────────────────────────────
// -- Race --
enum ERacerAnimStates
{
    Racer_Ready,
    Racer_Walk,
    Racer_Run
};

// ────────────────────────────────────────
// -- Coffee --
enum ECoffeeAnimStates
{
    Coffee_Warm,
    Coffee_Ok,
    Coffee_Cold
};

// ────────────────────────────────────────
// -- Parachute --
enum EParachuteAnimStates
{
    Parachute_Fall,
    Parachute_Open
};

// ────────────────────────────────────────

struct FLevel_Countdown : FLevel
{
    HEntity background;
    HEntity highScore;

    ELevel_Countdown_State state;

    f32 timer;
    f32 timeSpeedMult;
    EMiniGame_State miniGameState;

    u32 miniGames[EMiniGames_Size];
    u32 mgIndex;

    b8 wonLastMiniGame;


    //--------------------
    HEntity racer;

    //--------------------
    HEntity book;
    HEntity evilGuy;
    HEntity coffee;

    //--------------------
    HEntity building;
    HEntity parachuteQuad;
    HEntity parachute;

    //--------------------
    HEntity rocket;

    //--------------------
    HEntity ball;
    HEntity basketMan_0;
    HEntity basketMan_1;
    HEntity jumpBar;
};

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Init --
internal void Level_Countdown_Init(FGameState* gameState)
{
    srand((u32)time(NULL));

    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = &shared->transforms;
    FAssetsHandles* assets = &gameState->shared->assets;
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    level->state = Level_Countdown_State_MainMenu;
    level->timeSpeedMult = 1.0f;

    level->miniGameState = MiniGame_Init;
    level->mgIndex = EMiniGames_Size;   // start from last so we shuffle
    level->timer = 3.0f;
    for (u32 i = 0; i < EMiniGames_Size; ++i)
    {
        level->miniGames[i] = MG_Basketball;
    }

    shared->camera.handle = SpawnEntity(shared, INVALID_HANDLE, INVALID_HANDLE);
    shared->transforms.positions[shared->camera.handle] = { 0.0f, 0.0f, -10.0f };

    // High score
    level->highScore = SpawnEntity(shared, INVALID_HANDLE, INVALID_HANDLE);

    // Background
    level->background = SpawnEntity(shared, assets->hQuadMesh, assets->hLinesBGTexture, FColor::White());
    transforms->positions[level->background] = { 0, 0, 1.0f };
    transforms->scales[level->background] = { 100.0f, 30.0f, 0.0f };

    // ───────────────────────────────────────────────
    // racer
    level->racer = SpawnEntity(shared, assets->hQuadMesh, assets->hRacerTex, FColor::White(), Material_Transparent);
    transforms->positions[level->racer] = { 0, 0, -100.0f };
    transforms->scales[level->racer] = { 5.0f, 5.0f, 0};
    // racer anim
    AddClip(&shared->spriteSheetTable.sheets[assets->hRacerSheet], 0, 1, 2.0f, false);
    AddClip(&shared->spriteSheetTable.sheets[assets->hRacerSheet], 2, 2, 5.0f, true);
    AddClip(&shared->spriteSheetTable.sheets[assets->hRacerSheet], 4, 2, 10.0f, true);

    // ───────────────────────────────────────────────
    // coffee
    level->book = SpawnEntity(shared, assets->hQuadMesh, assets->hBookTex, FColor::White(), Material_Transparent);
    transforms->positions[level->book] = { 0, 0, -100.0f };
    transforms->scales[level->book] = { 2.0f, 6.0f, 0 };

    level->evilGuy = SpawnEntity(shared, assets->hQuadMesh, assets->hEvilGuyTex, FColor::White(), Material_Transparent);
    transforms->positions[level->evilGuy] = { 0, 0, -100.0f };
    transforms->scales[level->evilGuy] = { 4, 8, 0};

    level->coffee = SpawnEntity(shared, assets->hQuadMesh, assets->hCoffeeTex, FColor::White(), Material_Transparent);
    transforms->positions[level->coffee] = { 0, 0, -100.0f };
    transforms->scales[level->coffee] = { 3.0f, 7.0f, 0 };
    // Coffee anim
    AddClip(&shared->spriteSheetTable.sheets[assets->hCoffeeSheet], 0, 2, 1.0f, true);
    AddClip(&shared->spriteSheetTable.sheets[assets->hCoffeeSheet], 2, 2, 1.0f, true);
    AddClip(&shared->spriteSheetTable.sheets[assets->hCoffeeSheet], 4, 2, 1.0f, true);

    // ───────────────────────────────────────────────
    // parachute
    level->building = SpawnEntity(shared, assets->hQuadMesh, assets->hBuildingTex, FColor::White(), Material_Transparent);
    transforms->positions[level->building] = { 0, 0, -100.0f };
    transforms->scales[level->building] = { 5.0f, 15.0f, 0 };

    level->parachuteQuad = SpawnEntity(shared, assets->hQuadMesh, assets->hWhiteTexture, {1, 0, 0, 0.5f});
    transforms->positions[level->parachuteQuad] = { 0, 0, -100.0f };
    transforms->scales[level->parachuteQuad] = { 20, -1, 0 };

    level->parachute = SpawnEntity(shared, assets->hQuadMesh, assets->hParachuteTex, FColor::White(), Material_Transparent);
    transforms->positions[level->parachute] = { 0, 0, -100.0f };
    transforms->scales[level->parachute] = { 4.0f, 4.0f, 0 };
    // parachute anim
    AddClip(&shared->spriteSheetTable.sheets[assets->hParachuteSheet], 0, 2, 2.0f, true);
    AddClip(&shared->spriteSheetTable.sheets[assets->hParachuteSheet], 2, 2, 2.0f, true);

    // ───────────────────────────────────────────────
    // rocket
    level->rocket = SpawnEntity(shared, assets->hQuadMesh, assets->hRocketTex, FColor::White(), Material_Transparent);
    transforms->positions[level->rocket] = { 0, 0, -100.0f };
    transforms->scales[level->rocket] = { 5.0f, 5.0f, 0 };

    // rocket anim
    AddClip(&shared->spriteSheetTable.sheets[assets->hRocketSheet], 0, 3, 5.0f, true);


    // ───────────────────────────────────────────────
    // basketball
    level->ball = SpawnEntity(shared, assets->hQuadMesh, assets->hBallTex, FColor::White(), Material_Transparent);
    transforms->positions[level->ball] = { 0, 0, -100.0f };

    level->jumpBar = SpawnEntity(shared, assets->hQuadMesh, assets->hWhiteTexture, {1, 1, 1, 0.75f}, Material_Transparent);
    transforms->positions[level->jumpBar] = { 0, 0, -100.0f };
    transforms->scales[level->jumpBar] = { 0.5, 1, 0};

    level->basketMan_0 = SpawnEntity(shared, assets->hQuadMesh, assets->hBasketballTex, FColor::Red(), Material_Transparent);
    transforms->positions[level->basketMan_0] = { 0, 0, -100.0f };
    transforms->scales[level->basketMan_0] = { 4.0f, 4.0f, 0 };
    AddClip(&shared->spriteSheetTable.sheets[assets->hBasketballSheet_0], 0, 2, 5.0f, true);
    AddClip(&shared->spriteSheetTable.sheets[assets->hBasketballSheet_0], 2, 2, 5.0f, true);
    AddClip(&shared->spriteSheetTable.sheets[assets->hBasketballSheet_0], 4, 2, 5.0f, true);

    level->basketMan_1 = SpawnEntity(shared, assets->hQuadMesh, assets->hBasketballTex, FColor::Blue(), Material_Transparent);
    transforms->positions[level->basketMan_1] = { 0, 0, -100.0f };
    transforms->scales[level->basketMan_1] = { -4.0f, 4.0f, 0 };
    AddClip(&shared->spriteSheetTable.sheets[assets->hBasketballSheet_1], 0, 2, 5.0f, true);
    AddClip(&shared->spriteSheetTable.sheets[assets->hBasketballSheet_1], 2, 2, 5.0f, true);
    AddClip(&shared->spriteSheetTable.sheets[assets->hBasketballSheet_1], 4, 2, 5.0f, true);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Begin --
internal void Level_Countdown_Begin(FGameState* gameState)
{
    FAssetsHandles* assets = &gameState->shared->assets;
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = &shared->transforms;
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    gameState->input->mode = Input_UI;
    gameState->shared->camera.type = Camera_Orthographic;
}

// ──────────────────────────────────────────────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- UI Input --
internal void Level_Countdown_HandleUIInput(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    // UI input
    if (Pressed(&controller->dpadDown))
    {
        UINavigateNext(&gameState->uiNavState, true);
    }
    if (Pressed(&controller->dpadUp))
    {
        UINavigateBack(&gameState->uiNavState, true);
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Main Menu --
internal void Level_Countdown_Pasue(FGameState* gameState, FGameInput* input)
{
    gameState->uiNavState.buttonCount = 0;

    FUICommandsBucket* uiBucket = &gameState->shared->uiBucket;
    f32 buttonsYOffset = 20.0f;

    f32 buttonWidth = 200.0f + gameState->font->size;
    f32 buttonHeight = 65.0f + gameState->font->size;

    f32 screenWidth = gameState->shared->viewport.width;
    f32 screenHeight = gameState->shared->viewport.height;

    f32 rectPosX = (screenWidth / 2.0f) - (buttonWidth / 2.0f);
    f32 rectPosY = (screenHeight / 2.0f) - (buttonHeight);

    v4 rect = { rectPosX, rectPosY, buttonWidth, buttonHeight };

    FUIButtonStyle buttonStyle = {
        /*idle*/    FColor::Cyan(),
        /*hover*/   FColor::LightRed(),
        /*pressed*/ FColor::Green(),
        /*text*/    FColor::DarkBlue(),
        gameState->shared->assets.hWhiteTexture
    };

    v2 textPos = { 500, 500 };
    if (UIButton(gameState, input, rect, "Main Menu", &buttonStyle))
    {
        gameState->input->mode = Input_UI;

        FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;
        level->state = Level_Countdown_State_MainMenu;
        level->miniGameState = MiniGame_Init;
        level->mgIndex = EMiniGames_Size;   // start from last so we shuffle
        level->timer = 3.0f;

        SetGamePaused(gameState, false);
    }
    rect.y += rect.height + buttonsYOffset;
    if (UIButton(gameState, input, rect, "Quit", &buttonStyle))
    {
        gameState->running = false;
    }
}

internal void Level_Countdown_MainMenu(FGameState* gameState, FGameInput* input)
{
    gameState->uiNavState.buttonCount = 0;

    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    FUICommandsBucket* uiBucket = &gameState->shared->uiBucket;

    f32 buttonWidth = 200.0f + gameState->font->size;
    f32 buttonHeight = 65.0f + gameState->font->size;

    f32 screenWidth = gameState->shared->viewport.width;
    f32 screenHeight = gameState->shared->viewport.height;

    f32 rectPosX = (screenWidth / 2.0f) - (buttonWidth / 2.0f);
    f32 rectPosY = (screenHeight / 2.0f) - (buttonHeight);

    v4 rect = { rectPosX, rectPosY, buttonWidth, buttonHeight };

    FUIButtonStyle buttonStyle = {
        /*idle*/    FColor::Cyan(),
        /*hover*/   FColor::LightRed(),
        /*pressed*/ FColor::Green(),
        /*text*/    FColor::DarkBlue(),
        gameState->shared->assets.hWhiteTexture
    };

    v2 titlePos = { screenWidth / 2.7f, screenHeight / 5.0f };
    UIPushText(uiBucket, gameState->font, "3-2-1 Mayhem!!", titlePos, FColor::DarkBlue());

    c8 scoreBuf[64];
    sprintf(scoreBuf, "High Score: %d", (u32)gameState->shared->transforms.positions[level->highScore].y);
    cc8* scoreText = scoreBuf;
    titlePos.y += 100.0f;
    UIPushText(&gameState->shared->uiBucket, gameState->font, scoreText, titlePos, FColor::HotPink());

    if (UIButton(gameState, input, rect, "Play", &buttonStyle))
    {
        FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;
        level->state = Level_Countdown_State_Game;
        gameState->input->mode = Input_Game;
    }

    f32 buttonsYOffset = 20.0f;
    rect.y += rect.height + buttonsYOffset;
    if (UIButton(gameState, input, rect, "Quit", &buttonStyle))
    {
        gameState->running = false;
    }

    for (u32 controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
    {
        FGameControllerInput* controller = &input->controllers[controllerIndex];
        if (!controller->isConnected)
        {
            return;
        }
        Level_Countdown_HandleUIInput(gameState, input, controller);
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// ─────────────────────────── Game Section ─────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- MiniGame_Init --

internal void ShuffleMiniGames(u32* Array, u32 Count)
{
    for (u32 i = Count - 1; i > 0; --i)
    {
        u32 j = rand() % (i + 1);

        u32 Temp = Array[i];
        Array[i] = Array[j];
        Array[j] = Temp;
    }
}


// ──────────────────────────────────────────────────────────────────────────────────────────
// -- MiniGame_Countdown --

internal cc8* GetMiniGameName(u32 mg)
{
    switch (mg)
    {
        case MG_Race:
        {
            return "Race";
        } break;

        case MG_Parachute:
        {
            return "Parachute";
        } break;

        case MG_Rocket:
        {
            return "Rocket";
        } break;

        case MG_Coffee:
        {
            return "Coffee";
        } break;

        case MG_RedLight:
        {
            return "Red Light Green Light";
        } break;

        case MG_Basketball:
        {
            return "Basketball";
        } break;

        case MG_Bull:
        {
            return "Bull";
        } break;

        case MG_Movie:
        {
            return "Movie";
        } break;

        case MG_Bomb:
        {
            return "Bomb";
        } break;

        case MG_Camera:
        {
            return "Camera";
        } break;

        case EMiniGames_Size:
        {
            return "Size";
        } break;

        default:
        {
            return "";
        } break;
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Race Mini Game --
internal void RaceMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;
    FSharedStuff* shared = gameState->shared;
    FAssetsHandles* assets = &shared->assets;
    FAnimState* anim = &shared->entityTable.entities[level->racer].animState;

    local_presist f32 runningValue = 2.0f;

    switch (level->miniGameState)
    {
        case MiniGame_Init:
        {
            level->wonLastMiniGame = true;
            runningValue = 0.0f;

            shared->transforms.positions[level->racer] = { 0, 0, 0 };
            SetClip(anim, Racer_Ready);
        } break;

        case MiniGame_Countdown:
        {
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, "SPAM SPACE TO RUN!!", textPos, FColor::HotPink());
        } break;

        case MiniGame_Update:
        {
            if (Pressed(&controller->actionDown))
            {
                runningValue = Clamp(++runningValue, -1.0f, 2.0f);
            }
            else
            {
                runningValue -= 1.0f * input->deltaTime;
            }

            // Update anim
            if (runningValue >= 1.5f)
            {
                SetClip(anim, Racer_Run);
            }
            else
            {
                SetClip(anim, Racer_Walk);
            }

#if FADO_DEBUG
            c8 buffer[32];
            sprintf(buffer, "%f", runningValue);
            cc8* text = buffer;

            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, text, textPos, FColor::HotPink());
#endif

        } break;
        case MiniGame_End:
        {
            shared->transforms.positions[level->racer] = { 0, 0, -100.0f };
        }
    }

    UpdateAnimState(&shared->entityTable.entities[level->racer], &shared->spriteSheetTable.sheets[assets->hRacerSheet], input->deltaTime);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Parachute Mini Game --
internal void ParachuteMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;
    FSharedStuff* shared = gameState->shared;
    FAssetsHandles* assets = &shared->assets;
    FAnimState* anim = &shared->entityTable.entities[level->parachute].animState;

    local_presist f32 parachuteOpenPosY = 1.0f;
    local_presist b8 hasOpenedChute = false;

    switch (level->miniGameState)
    {
        case MiniGame_Init:
        {
            level->wonLastMiniGame = false;

            hasOpenedChute = false;
            parachuteOpenPosY = RandomF32InRange(-3.0f, 3.0);

            shared->transforms.positions[level->building] = { 7,0,0 };
        } break;

        case MiniGame_Countdown:
        {
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, "OPEN YOUR CHUTE AT THE RIGHT MOMENT!!", textPos, FColor::HotPink());

            shared->transforms.positions[level->parachute] = { 0, 5.0f ,0 };

            shared->transforms.positions[level->parachuteQuad].y = parachuteOpenPosY - 1.0f;

            SetClip(anim, Parachute_Fall);
        } break;

        case MiniGame_Update:
        {
            shared->transforms.positions[level->parachute].y -= 2.0f * input->deltaTime;
            f32 playerPosY = shared->transforms.positions[level->parachute].y;

            shared->transforms.positions[level->parachuteQuad].z = 0;

            if (Pressed(&controller->actionDown))
            {
                if (!hasOpenedChute)
                {
                    hasOpenedChute = true;

                    f32 min = playerPosY - 1.0f;
                    if ((playerPosY >= min) && (playerPosY <= parachuteOpenPosY))
                    {
                        level->wonLastMiniGame = true;
                        SetClip(anim, Parachute_Open);
                    }
                    else
                    {
                        level->wonLastMiniGame = false;
                    }
                }
            }

            if (hasOpenedChute)
            {
                shared->transforms.positions[level->parachute].y -= level->wonLastMiniGame ? 2.0f * input->deltaTime : 5.0f*input->deltaTime;
                if (shared->transforms.positions[level->parachute].y <= -5.0f)
                {
                    level->timer = 0.0f;
                }
            }

#if FADO_DEBUG
            c8 buffer[64];
            sprintf(buffer, "PlayerPosY: %f | Parachute: %f", playerPosY, parachuteOpenPosY);
            cc8* text = buffer;

            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, text, textPos, FColor::HotPink());
#endif

        } break;
        case MiniGame_End:
        {
            shared->transforms.positions[level->building] = { 0, 0 ,-100 };
            shared->transforms.positions[level->parachuteQuad] = { 0, 0 ,-100 };
            shared->transforms.positions[level->parachute] = { 0, 0 ,-100 };
        }
    }
    UpdateAnimState(&shared->entityTable.entities[level->parachute], &shared->spriteSheetTable.sheets[assets->hParachuteSheet], input->deltaTime);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Rocket Mini Game --
internal void RocketMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;
    FSharedStuff* shared = gameState->shared;
    FAssetsHandles* assets = &shared->assets;
    FAnimState* anim = &shared->entityTable.entities[level->coffee].animState;

    local_presist f32 tilt = 0.0f;
    local_presist f32 sign = -1.0f;

    switch (level->miniGameState)
    {
        case MiniGame_Init:
        {
            level->wonLastMiniGame = true;
            tilt = 0.0f;
            sign = -1.0f;

            shared->transforms.positions[level->rocket] = {0, -3, 0};
            shared->transforms.rotations[level->rocket] = QuatIdentity();
            SetClip(anim, 0);
        } break;

        case MiniGame_Countdown:
        {
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, "STIR THE SPACESHIP!!", textPos, FColor::HotPink());
        } break;

        case MiniGame_Update:
        {
            if (Down(&controller->dpadLeft) && !Down(&controller->dpadRight))
            {
                sign = -1.0f;
            }
            else if (Down(&controller->dpadRight) && !Down(&controller->dpadLeft))
            {
                sign = 1.0f;
            }
            tilt += sign * 100.0f * input->deltaTime;

            Rotate(&shared->transforms, level->rocket, { 0, 0, ( -tilt * input->deltaTime)});

            v3 rot = QuatToEuler(shared->transforms.rotations[level->rocket]);
            if (rot.z < 45.0f && rot.z > -45.0f)
            {
                shared->transforms.positions[level->rocket].y += 1.5f * input->deltaTime;
            }

#if FADO_DEBUG
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };

            c8 buffer[64];
            sprintf(buffer, "Rocket Tilt: %f", rot.z);
            cc8* text = buffer;
            UIPushText(&gameState->shared->uiBucket, gameState->font, text, textPos, FColor::HotPink());
#endif
        }break;

        case MiniGame_End:
        {
            v3 rot = QuatToEuler(shared->transforms.rotations[level->rocket]);
            if (rot.z >= 45.0f || rot.z <= -45.0f)
            {
                level->wonLastMiniGame = false;
            }

            shared->transforms.positions[level->rocket] = { 0, 0, -100.0f };;
            shared->transforms.rotations[level->rocket] = QuatIdentity();
        }
    }
    UpdateAnimState(&shared->entityTable.entities[level->rocket], &shared->spriteSheetTable.sheets[assets->hRocketSheet], input->deltaTime);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Coffee Mini Game --

enum EWindDir
{
    WindDir_Left,
    WindDir_Right
};

internal void CoffeeMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;
    FSharedStuff* shared = gameState->shared;
    FAssetsHandles* assets = &shared->assets;
    FAnimState* anim = &shared->entityTable.entities[level->coffee].animState;

    local_presist EWindDir wind = (EWindDir)RandomU32InRange(0, 2);
    local_presist f32 warmth = 2.0f;
    local_presist f32 windTimer = 1.0f;

    switch (level->miniGameState)
    {
        case MiniGame_Init:
        {
            warmth = 2.0f;
            windTimer = 1.0f;
            level->wonLastMiniGame = true;

            shared->transforms.positions[level->coffee] = { 0,0,0 };
            SetClip(anim, Coffee_Warm);
        } break;

        case MiniGame_Countdown:
        {
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, "KEEP THE BOSS'S COFFEE WARM!!", textPos, FColor::HotPink());
        } break;

        case MiniGame_Update:
        {
            windTimer -= input->deltaTime;
            if (windTimer <= 0)
            {
                wind = (EWindDir)RandomU32InRange(0, 1);
                windTimer = 1.0f;
            }

            if (wind == WindDir_Left)
            {
                if (!Down(&controller->dpadLeft) || Down(&controller->dpadRight))
                {
                    warmth -= 0.5f * input->deltaTime;
                }
                shared->transforms.positions[level->evilGuy] = { -8.0f , 0 ,0 };
                shared->transforms.scales[level->evilGuy].x = -4.0f;
            }
            else if (wind == WindDir_Right)
            {
                if (!Down(&controller->dpadRight) || Down(&controller->dpadLeft))
                {
                    warmth -= 0.5f * input->deltaTime;
                }
                shared->transforms.positions[level->evilGuy] = { 8.0f , 0 ,0 };
                shared->transforms.scales[level->evilGuy].x = 4.0f;
            }

            // Animations
            if(warmth > 1.0f)
            {
                SetClip(anim, Coffee_Warm); 
            }
            else if(warmth <= 1.0f && warmth > 0.25f) 
            {
                SetClip(anim, Coffee_Ok);
            }
            else if(warmth <= 0.25f)
            {
                SetClip(anim, Coffee_Cold);
            }

            // Book
            if (Down(&controller->dpadRight)) { shared->transforms.positions[level->book] = { 4.0f , 0 ,0}; }
            else if (Down(&controller->dpadLeft))  { shared->transforms.positions[level->book] = { -4.0f, 0, 0 }; }
            else { shared->transforms.positions[level->book] = { 0, 0, -100 }; }

#if FADO_DEBUG
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            cc8* windText = (wind == WindDir_Right) ? "Right" : ((wind == WindDir_Left) ? "Left" : "None");
            UIPushText(&gameState->shared->uiBucket, gameState->font, windText, textPos, FColor::HotPink());

            c8 buffer[64];
            sprintf(buffer, "Warmth: %f", warmth);
            cc8* text = buffer;
            f32 yOffset = 100.0f;
            textPos.y += yOffset;
            UIPushText(&gameState->shared->uiBucket, gameState->font, text, textPos, FColor::HotPink());
#endif

            if (warmth <= 0)
            {
                level->wonLastMiniGame = false;
                level->timer = 0.0f;
            }

        } break;
        case MiniGame_End:
        {
            shared->transforms.positions[level->book] = { 0, 0, -100.0f };
            shared->transforms.positions[level->evilGuy] = { 0, 0, -100.0f };
            shared->transforms.positions[level->coffee] = { 0, 0, -100.0f };
        }
    }
    UpdateAnimState(&shared->entityTable.entities[level->coffee], &shared->spriteSheetTable.sheets[assets->hCoffeeSheet], input->deltaTime);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Red Light Mini Game --
internal void RedLightMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    local_presist f32 redTime = RandomF32InRange(1.0, 3.0f);
    local_presist f32 goalValue = 0.0f;

    switch (level->miniGameState)
    {
        case MiniGame_Init:
        {
            level->wonLastMiniGame = true;
            redTime = RandomF32InRange(1.0, 3.0f);
            goalValue = 0.0f;
        } break;

        case MiniGame_Countdown:
        {
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, "GREEN = RUN, RED = STOP!!", textPos, FColor::HotPink());
        } break;

        case MiniGame_Update:
        {
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };

            if (level->timer <= redTime)
            {
                if (Down(&controller->actionDown) || (goalValue <= 0.0f))
                {
                    if ((redTime - level->timer) >= 0.5f)
                    {
                        level->wonLastMiniGame = false;
                        level->timer = 0.0f;
                    }
                }
                cc8* text = "RED LIGHT >:(";
                UIPushText(&gameState->shared->uiBucket, gameState->font, text, textPos, FColor::HotPink());
            }
            else
            {
                if (Pressed(&controller->actionDown))
                {
                    goalValue += 10.0f * input->deltaTime;
                }
                else
                {
                    goalValue -= 1.0f * input->deltaTime;
                }
                cc8* text = "GREEN LIGHT :)";
                UIPushText(&gameState->shared->uiBucket, gameState->font, text, textPos, FColor::HotPink());
                c8 goalBuffer[64];
                sprintf(goalBuffer, "Distance: %f", goalValue);
                cc8* goalText = goalBuffer;
                f32 yOffset = 100.0f;
                textPos.y += yOffset;
                UIPushText(&gameState->shared->uiBucket, gameState->font, goalText, textPos, FColor::HotPink());
            }
        } break;
        case MiniGame_End:
        {
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Basketball Mini Game --
internal void BasketballMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;
    FSharedStuff* shared = gameState->shared;
    FAssetsHandles* assets = &shared->assets;
    FAnimState* anim_0 = &shared->entityTable.entities[level->basketMan_0].animState;
    FAnimState* anim_1 = &shared->entityTable.entities[level->basketMan_1].animState;

    local_presist f32 jumpStrength = 0.0f;
    local_presist f32 jumpGoal = RandomF32InRange(1.0f, 2.0f);

    switch (level->miniGameState)
    {
    case MiniGame_Init:
    {
        level->wonLastMiniGame = false;
        jumpStrength = 0.0f;
        jumpGoal = RandomF32InRange(2.0f, 4.0f);

        shared->transforms.positions[level->ball] = { 0, 5, 0 };
        shared->transforms.rotations[level->ball] = QuatIdentity();

        shared->transforms.positions[level->basketMan_0] = { -3, -3, 0 };
        shared->transforms.positions[level->basketMan_1] = { 3, -3, 0 };
        SetClip(anim_0, 0);
        SetClip(anim_1, 0);
    } break;

    case MiniGame_Countdown:
    {
        Rotate(&shared->transforms, level->ball, { 0, 0, 100.0f * input->deltaTime });
        f32 w = gameState->shared->viewport.width / 4.0f;
        f32 h = gameState->shared->viewport.height / 4.0f;
        v2 textPos = { w, h };
        UIPushText(&gameState->shared->uiBucket, gameState->font, "CHARGE YOUR JUMPT TO WIN THE BALL!!", textPos, FColor::HotPink());
    } break;

    case MiniGame_Update:
    {
        Rotate(&shared->transforms, level->ball, { 0, 0, 100.0f * input->deltaTime });
        shared->transforms.positions[level->ball].y -= 0.5f * input->deltaTime;
        shared->transforms.positions[level->basketMan_0].y += 0.5f * input->deltaTime;
        shared->transforms.positions[level->basketMan_1].y += 0.5f * input->deltaTime;

        if (level->timer > 0.5f)
        {
            SetClip(anim_0, 1);
            SetClip(anim_1, 1);

            if (Pressed(&controller->actionDown))
            {
                jumpStrength += 0.25f;
            }
            else
            {
                jumpStrength -= 1.0f * input->deltaTime;
            }
            jumpStrength = Clamp(jumpStrength, 0.0f, 100.0f);

            shared->transforms.positions[level->jumpBar] = {-5, -3, 0};
            shared->transforms.scales[level->jumpBar].y = 1.0f + jumpStrength;
            if (jumpStrength <= jumpGoal)
            {
                shared->entityTable.entities[level->jumpBar].material.color = { 0,0,0,0.75f };
            }
            else if ((jumpStrength >= jumpGoal) && (jumpStrength <= (jumpGoal + 0.75f)))
            {
                shared->entityTable.entities[level->jumpBar].material.color = { 0,1,0,0.75f };
            }
            else
            {
                shared->entityTable.entities[level->jumpBar].material.color = { 1,0,0,0.75f };
            }
        }
        else
        {
            shared->transforms.positions[level->ball] = { 0, 0, -100.0f };
            shared->transforms.positions[level->basketMan_0] = { -3, -3, 0 };
            shared->transforms.positions[level->basketMan_1] = { 3, -3, 0 };
            shared->transforms.positions[level->jumpBar] = { 0, 0, -100.0f };

            if ((jumpStrength >= jumpGoal) && (jumpStrength <= (jumpGoal + 0.75f)))
            {
                SetClip(anim_0, 2);
                SetClip(anim_1, 0);

                level->wonLastMiniGame = true;
            }
            else
            {
                SetClip(anim_0, 0);
                SetClip(anim_1, 2);
            }
        }

#if FADO_DEBUG
        f32 w = gameState->shared->viewport.width / 4.0f;
        f32 h = gameState->shared->viewport.height / 4.0f;
        v2 textPos = { w, h };
        c8 buffer[64];
        sprintf(buffer, "Goal: %f", jumpGoal);
        cc8* goal = buffer;
        UIPushText(&gameState->shared->uiBucket, gameState->font, goal, textPos, FColor::HotPink());

        c8 goalBuffer[64];
        sprintf(goalBuffer, "Jump Strength: %f", jumpStrength);
        cc8* text = goalBuffer;
        f32 yOffset = 100.0f;
        textPos.y += yOffset;
        UIPushText(&gameState->shared->uiBucket, gameState->font, text, textPos, FColor::HotPink());
#endif
    } break;

    case MiniGame_End:
    {
    }
    }

    UpdateAnimState(&shared->entityTable.entities[level->basketMan_0], &shared->spriteSheetTable.sheets[assets->hBasketballSheet_0], input->deltaTime);
    UpdateAnimState(&shared->entityTable.entities[level->basketMan_1], &shared->spriteSheetTable.sheets[assets->hBasketballSheet_1], input->deltaTime);
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Bull Mini Game --

enum EBullDirection
{
    Bull_Left,
    Bull_Right,
    Bull_Front,
    Bull_Back
};

internal void BullMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    local_presist EBullDirection bullDir = (EBullDirection)RandomU32InRange(0, 3);
    local_presist f32 changeTimer = RandomF32InRange(1.0f, 2.0f);
    local_presist f32 changeElapsed = 0;
    local_presist f32 errorMargin = 0;
    local_presist b8 passed = true;

    switch (level->miniGameState)
    {
        case MiniGame_Init:
        {
            level->wonLastMiniGame = true;
            EBullDirection bullDir = (EBullDirection)RandomU32InRange(0, 3);
            f32 changeTimer = RandomF32InRange(1.0f, 2.0f);
            changeElapsed = 0;
            errorMargin = 0;
            passed = true;
        } break;

        case MiniGame_Countdown:
        {
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, "MOVE OOPOSITE THE BULL!!", textPos, FColor::HotPink());
        } break;

        case MiniGame_Update:
        {
            changeElapsed += input->deltaTime;
            errorMargin += input->deltaTime;

            if (changeElapsed >= changeTimer)
            {
                if (passed)
                {
                    if (level->timer >= 1.0f)
                    {
                        bullDir = (EBullDirection)RandomU32InRange(0, 3);
                        changeElapsed = 0.0f;
                        errorMargin = 0;
                    }
                }
                else
                {
                    level->timer = 0.0f;
                    level->wonLastMiniGame = false;
                }
            }

            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };

            switch (bullDir)
            {
                case Bull_Left:
                {
                    if (Pressed(&controller->dpadLeft) || Pressed(&controller->dpadDown) || Pressed(&controller->dpadUp))
                    {
                        passed = false;
                    }
                    else if (Pressed(&controller->dpadRight))
                    {
                        if (errorMargin <= 0.75f)
                        {
                            passed = true;
                        }
                    }
                    cc8* windText = "LEFT!";
                    UIPushText(&gameState->shared->uiBucket, gameState->font, windText, textPos, FColor::HotPink());
                } break;

                case Bull_Right:
                {
                    if (Pressed(&controller->dpadRight) || Pressed(&controller->dpadDown) || Pressed(&controller->dpadUp))
                    {
                        passed = false;
                    }
                    else if (Pressed(&controller->dpadLeft))
                    {
                        if (errorMargin <= 0.75f)
                        {
                            passed = true;
                        }
                    }
                    cc8* windText = "RIGHT!";
                    UIPushText(&gameState->shared->uiBucket, gameState->font, windText, textPos, FColor::HotPink());
                } break;

                case Bull_Front:
                {
                    if (Pressed(&controller->dpadLeft) || Pressed(&controller->dpadUp) || Pressed(&controller->dpadRight))
                    {
                        passed = false;
                    }
                    else if (Pressed(&controller->dpadDown))
                    {
                        if (errorMargin <= 0.75f)
                        {
                            passed = true;
                        }
                    }
                    cc8* windText = "FRONT!";
                    UIPushText(&gameState->shared->uiBucket, gameState->font, windText, textPos, FColor::HotPink());
                } break;

                case Bull_Back:
                {
                    if (Pressed(&controller->dpadLeft) || Pressed(&controller->dpadDown) || Pressed(&controller->dpadRight))
                    {
                        passed = false;
                    }
                    else if (Pressed(&controller->dpadUp))
                    {
                        if (errorMargin <= 0.75f)
                        {
                            passed = true;
                        }
                    }
                    cc8* windText = "BACK!";
                    UIPushText(&gameState->shared->uiBucket, gameState->font, windText, textPos, FColor::HotPink());
                } break;

            }
        } break;

        case MiniGame_End:
        {
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Movie Mini Game --

enum EMovieDirection
{
    Movie_Left,
    Movie_Right,
    Movie_Front,
    Movie_Back
};

internal void MovieMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    local_presist EMovieDirection movieDir = (EMovieDirection)RandomU32InRange(0, 3);
    local_presist f32 changeTimer = RandomF32InRange(1.0f, 2.0f);
    local_presist f32 changeElapsed = 0;
    local_presist f32 errorMargin = 0;
    local_presist b8 passed = true;

    switch (level->miniGameState)
    {
    case MiniGame_Init:
    {
        level->wonLastMiniGame = true;
        EMovieDirection MovieDir = (EMovieDirection)RandomU32InRange(0, 3);
        f32 changeTimer = RandomF32InRange(1.0f, 2.0f);
        changeElapsed = 0;
        errorMargin = 0;
        passed = true;
    } break;

    case MiniGame_Countdown:
    {
        f32 w = gameState->shared->viewport.width / 4.0f;
        f32 h = gameState->shared->viewport.height / 4.0f;
        v2 textPos = { w, h };
        UIPushText(&gameState->shared->uiBucket, gameState->font, "FOLLOW THE DIRECTOR'S ORDERS!!", textPos, FColor::HotPink());
    } break;

    case MiniGame_Update:
    {
        changeElapsed += input->deltaTime;
        errorMargin += input->deltaTime;

        if (changeElapsed >= changeTimer)
        {
            if (passed)
            {
                if (level->timer >= 1.0f)
                {
                    movieDir = (EMovieDirection)RandomU32InRange(0, 3);
                    changeElapsed = 0.0f;
                    errorMargin = 0;
                }
            }
            else
            {
                level->timer = 0.0f;
                level->wonLastMiniGame = false;
            }
        }

        f32 w = gameState->shared->viewport.width / 4.0f;
        f32 h = gameState->shared->viewport.height / 4.0f;
        v2 textPos = { w, h };

        switch (movieDir)
        {
        case Movie_Left:
        {
            if (Pressed(&controller->dpadRight) || Pressed(&controller->dpadDown) || Pressed(&controller->dpadUp))
            {
                passed = false;
            }
            else if (Pressed(&controller->dpadLeft))
            {
                if (errorMargin <= 0.75f)
                {
                    passed = true;
                }
            }
            cc8* windText = "LEFT!";
            UIPushText(&gameState->shared->uiBucket, gameState->font, windText, textPos, FColor::HotPink());
        } break;

        case Movie_Right:
        {
            if (Pressed(&controller->dpadLeft) || Pressed(&controller->dpadDown) || Pressed(&controller->dpadUp))
            {
                passed = false;
            }
            else if (Pressed(&controller->dpadRight))
            {
                if (errorMargin <= 0.75f)
                {
                    passed = true;
                }
            }
            cc8* windText = "RIGHT!";
            UIPushText(&gameState->shared->uiBucket, gameState->font, windText, textPos, FColor::HotPink());
        } break;

        case Movie_Front:
        {
            if (Pressed(&controller->dpadLeft) || Pressed(&controller->dpadDown) || Pressed(&controller->dpadRight))
            {
                passed = false;
            }
            else if (Pressed(&controller->dpadUp))
            {
                if (errorMargin <= 0.75f)
                {
                    passed = true;
                }
            }
            cc8* windText = "FRONT!";
            UIPushText(&gameState->shared->uiBucket, gameState->font, windText, textPos, FColor::HotPink());
        } break;

        case Movie_Back:
        {
            if (Pressed(&controller->dpadLeft) || Pressed(&controller->dpadUp) || Pressed(&controller->dpadRight))
            {
                passed = false;
            }
            else if (Pressed(&controller->dpadDown))
            {
                if (errorMargin <= 0.75f)
                {
                    passed = true;
                }
            }
            cc8* windText = "BACK!";
            UIPushText(&gameState->shared->uiBucket, gameState->font, windText, textPos, FColor::HotPink());
        } break;

        }
    } break;

    case MiniGame_End:
    {
    }
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Bomb Mini Game --
internal void BombMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    local_presist u32 targetNum = RandomU32InRange(0, 99);
    local_presist u32 currentNum = 0;

    switch (level->miniGameState)
    {
        case MiniGame_Init:
        {
            level->wonLastMiniGame = true;
            targetNum = RandomU32InRange(0, 99);
            currentNum = 0;
        } break;

        case MiniGame_Countdown:
        {
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, "DIFFUSE THE BOOOMB!!", textPos, FColor::HotPink());
        } break;

        case MiniGame_Update:
        {
            if (level->timer > 0.0f)
            {
                if (Pressed(&controller->dpadDown))
                {
                    currentNum--;
                }
                if (Pressed(&controller->dpadUp))
                {
                    currentNum++;
                }
                if (Pressed(&controller->dpadLeft))
                {
                    currentNum -= 10;
                }
                if (Pressed(&controller->dpadRight))
                {
                    currentNum += 10;
                }
            }
            else
            {
                if (currentNum != targetNum)
                {
                    level->wonLastMiniGame = false;
                }
            }

            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };

            c8 targetBuf[64];
            sprintf(targetBuf, "Target: %d", targetNum);
            cc8* targetText = targetBuf;
            UIPushText(&gameState->shared->uiBucket, gameState->font, targetText, textPos, FColor::HotPink());

            f32 yOffset = 100.0f;
            textPos.y += yOffset;
            c8 currentBuf[64];
            sprintf(currentBuf, "Current: %d", currentNum);
            cc8* currentText = currentBuf;
            UIPushText(&gameState->shared->uiBucket, gameState->font, currentText, textPos, FColor::HotPink());
        } break;

        case MiniGame_End:
        {
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Camera Mini Game --
internal void CameraMiniGame(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    local_presist f32 flashTime = RandomF32InRange(1.0f, level->timer);
    local_presist b8 flashed = false;

    switch (level->miniGameState)
    {
        case MiniGame_Init:
        {
            level->wonLastMiniGame = true;
            flashTime = RandomF32InRange(1.0f, level->timer);
            flashed = false;
        } break;

        case MiniGame_Countdown:
        {
            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 4.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, "SMILE ON CHEEEEEESE!!", textPos, FColor::HotPink());
        } break;

        case MiniGame_Update:
        {
            if (level->timer <= flashTime)
            {
                if (!flashed)
                {
                    flashed = true;
                }

                if (!Down(&controller->actionDown))
                {
                    f32 min = flashTime - 1.0f;
                    if (level->timer < min)
                    {
                        level->wonLastMiniGame = false;
                        level->timer = 0.0f;
                    }
                }
            }
            else
            {
                if (Down(&controller->actionDown))
                {
                    level->wonLastMiniGame = false;
                    level->timer = 0.0f;
                }
            }

            if (flashed)
            {
                f32 w = gameState->shared->viewport.width / 4.0f;
                f32 h = gameState->shared->viewport.height / 4.0f;
                v2 textPos = { w, h };
                UIPushText(&gameState->shared->uiBucket, gameState->font, "SAY CHEEEESSSEEE!!!", textPos, FColor::HotPink());
            }

            if (flashTime <= 0.0f)
            {
                level->timer = 0.0f;
            }
        } break;
        case MiniGame_End:
        {
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Game Input  --

internal void TriggerMiniGame(u32 mg, FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    switch (mg)
    {
        case MG_Race:       { RaceMiniGame(gameState, input, controller); } break;

        case MG_Parachute:  { ParachuteMiniGame(gameState, input, controller); } break;
        
        case MG_Rocket:     { RocketMiniGame(gameState, input, controller); } break;
        
        case MG_Coffee:     { CoffeeMiniGame(gameState, input, controller); } break;
        
        case MG_RedLight:   { RedLightMiniGame(gameState, input, controller); } break;
        
        case MG_Basketball: { BasketballMiniGame(gameState, input, controller); } break;
        
        case MG_Bull:       { BullMiniGame(gameState, input, controller); } break;
        
        case MG_Movie:      { MovieMiniGame(gameState, input, controller); } break;
        
        case MG_Bomb:       { BombMiniGame(gameState, input, controller); } break;
        
        case MG_Camera:     { CameraMiniGame(gameState, input, controller); } break;

        default:            { RaceMiniGame(gameState, input, controller); }break;
    }
}

internal void Level_Countdown_HandleGameInput(FGameState* gameState, FGameInput* input, FGameControllerInput* controller)
{
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    if (!gameState->paused)
    {
        input->deltaTime *= level->timeSpeedMult;
        level->timer -= input->deltaTime;
    }

    switch (level->miniGameState)
    {
        case MiniGame_Init:
        {
            if (level->mgIndex == EMiniGames_Size)
            {
                level->mgIndex = 0;
                ShuffleMiniGames(level->miniGames, EMiniGames_Size);
            }
            TriggerMiniGame(level->miniGames[level->mgIndex], gameState, input, controller);
            level->miniGameState = MiniGame_Countdown;
        } break;

        case MiniGame_Countdown:
        {
            c8 buffer[32];
            sprintf(buffer, "%d", (i32)ceil(level->timer));
            cc8* text = buffer;

            f32 w = gameState->shared->viewport.width / 8.0f;
            f32 h = gameState->shared->viewport.height / 2.0f;
            v2 textPos = { w, h };
            UIPushText(&gameState->shared->uiBucket, gameState->font, text, textPos, FColor::HotPink());

            f32 yOffset = 100.0f;
            textPos.y += yOffset;
            UIPushText(&gameState->shared->uiBucket, gameState->font, "Starting Mini Game", textPos, FColor::HotPink());
            textPos.y += yOffset;
            UIPushText(&gameState->shared->uiBucket, gameState->font, GetMiniGameName(level->miniGames[level->mgIndex]), textPos, FColor::HotPink());

            if (level->timer <= 0.0f)
            {
                level->miniGameState = MiniGame_Update;
                level->timer = 5.0f;
            }

            TriggerMiniGame(level->miniGames[level->mgIndex], gameState, input, controller);
        } break;

        case MiniGame_Update:
        {
            TriggerMiniGame(level->miniGames[level->mgIndex], gameState, input, controller);

            if (level->timer <= 0.0f)
            {
                level->miniGameState = MiniGame_End;
                level->timer = 2.0f;
            }
        } break;

        case MiniGame_End:
        {
            TriggerMiniGame(level->miniGames[level->mgIndex], gameState, input, controller);

            f32 w = gameState->shared->viewport.width / 4.0f;
            f32 h = gameState->shared->viewport.height / 2.0f;
            v2 textPos = { w, h };
            cc8* endText = level->wonLastMiniGame ? "YAAAAAY!!!" : "boooooo...";
            UIPushText(&gameState->shared->uiBucket, gameState->font, endText, textPos, FColor::HotPink());

            if (level->timer <= 0.0f)
            {
                if (!level->wonLastMiniGame)
                {
                    // Workaround: save the current score as the entity's pos.x and the high score as y :)
                    v3* score = &gameState->shared->transforms.positions[level->highScore];
                    if (score->x > score->y)
                    {
                        score->y = score->x;
                    }
                    score->x = 0.0f;

                    //SaveCurrentLevel(gameState);
                    // return to main menu
                    gameState->input->mode = Input_UI;

                    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;
                    level->state = Level_Countdown_State_MainMenu;
                    level->miniGameState = MiniGame_Init;
                    level->mgIndex = EMiniGames_Size;   // start from last so we shuffle
                    level->timer = 3.0f;
                }
                else
                {
                    // Next mini game
                    level->mgIndex++;
                    level->timer = 3.0f;
                    level->miniGameState = MiniGame_Init;

                    // Workaround: save the current score as the entity's pos.x :)
                    gameState->shared->transforms.positions[level->highScore].x += 10.0f;
                }
            }
        } break;
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Game --
internal void Level_Countdown_Game(FGameState* gameState, FGameInput* input)
{
    for (u32 controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
    {
        FGameControllerInput* controller = &input->controllers[controllerIndex];
        if (!controller->isConnected)
        {
            return;
        }

        // Pause/Unpasue
        if (Pressed(&controller->start))
        {
            SetGamePaused(gameState, !gameState->paused);
        }

        if (input->mode & Input_Game)
        {
            if (!gameState->paused)
            {
                Level_Countdown_HandleGameInput(gameState, input, controller);
            }
        }
        if (input->mode & Input_UI)
        {
            Level_Countdown_HandleUIInput(gameState, input, controller);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Update --
internal void Level_Countdown_Update(FGameState* gameState, f32 dt)
{
    FAssetsHandles* assets = &gameState->shared->assets;
    FSharedStuff* shared = gameState->shared;
    FTransforms* transforms = &shared->transforms;
    FGameInput* input = gameState->input;
    FLevel_Countdown* level = (FLevel_Countdown*)gameState->currentLevel;

    if (level->state == Level_Countdown_State_MainMenu)
    {
        Level_Countdown_MainMenu(gameState, input);
    }
    else
    {
        Level_Countdown_Game(gameState, input);
    }

    if (gameState->paused)
    {
        Level_Countdown_Pasue(gameState, input);
    }
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Make --
inline FLevel SetupLevel_Countdown()
{
    FLevel_Countdown level = {};
    level.Init = Level_Countdown_Init;
    level.Begin = Level_Countdown_Begin;
    level.Update = Level_Countdown_Update;
    level.name = "level_countdown";
    return level;
};

// ──────────────────────────────────────────────────────────────────────────────────────────

#pragma warning(pop)

#endif // LEVEL_COUNTDOWN