// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_ASSETS_H
#define FADO_ASSETS_H

#include "fado_types.h"

// ────────────────────────────────────────────────────────────────────────

// Fat list of all the assets we load in the engine
struct FAssetsHandles
{
    // GMTK Jam Assets

    HTexture hNoiseTexture;
    HTexture hLinesBGTexture;

    HTexture hRacerTex;
    HSpriteSheet hRacerSheet;

    HTexture hBookTex;
    HTexture hEvilGuyTex;
    HTexture hCoffeeTex;
    HSpriteSheet hCoffeeSheet;

    HTexture hBuildingTex;
    HTexture hParachuteTex;
    HSpriteSheet hParachuteSheet;

    HTexture hRocketTex;
    HSpriteSheet hRocketSheet;

    HTexture hBallTex;
    HTexture hBasketballTex;
    HSpriteSheet hBasketballSheet_0;
    HSpriteSheet hBasketballSheet_1;

    HTexture hBombTex;

    HTexture hRedLightTex;
    HSpriteSheet hRedLightSheet;

    HTexture hCameraTex;
    HSpriteSheet hCameraSheet;

    HTexture hMovieTex;
    HSpriteSheet hMovieSheet;

    HTexture hBullTex;
    HSpriteSheet hBullSheet;

    HTexture hInputTex;
    HSpriteSheet hInputSheet;

    // Sounds
    HSound hMusic;
    HSound hYaySFX;
    HSound hBooSFX;
    HSound hCountdownSFX;
    HSound hRunningSFX;
    HSound hSoldierHornSFX;
    HSound hParachuteSFX;
    HSound rocketSFX;
    HSound hEvilLaughSFX;
    HSound hGreenLightSFX;
    HSound hRedLightSFX;
    HSound sportWhistleSFX;
    HSound dearBasketballSFX;
    HSound yehawSFX;
    HSound movieSFX;
    HSound bombStartSFX;
    HSound bombEndSFX;
    HSound flashSFX;

    ///////////////////////////////////////////////////////////////////////

    // Mesh handles
    HMesh hQuadMesh;        // Created manually once when we load the assets and used across all sprites.
    HMesh hGroundQuad;      // Created manually once when we load the assets and used across all assets with shadows.

    // Texture handles
    HTexture hWhiteTexture;
    HTexture hBlobTexture;

    HSound hUIClickSFX;
};

// ────────────────────────────────────────────────────────────────────────

#endif // FADO_ASSETS_H