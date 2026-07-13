// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_ASSETS_H
#define FADO_ASSETS_H

#include "fado_types.h"

// Fat list of all the assets we load in the engine
struct FAssetsHandles
{
    // folayfila
    HTexture hFolayfilaTex;
    HSpriteSheet hFolayfilaSheet;

    // Mesh handles
    HMesh hQuadMesh;        // Created manually once when we load the assets and used across all sprites.
    HMesh hGroundQuad;      // Created manually once when we load the assets and used across all assets with shadows.
    HEntity hPlaneMesh;
    HMesh hSkyBoxMesh;
    HMesh hCubeMesh;
    HMesh hSphereMesh;

    // Texture handles
    HTexture hWhiteTexture;
    HTexture hShadowTexture;
    HTexture hGridTexture;
    HTexture hSkyBoxTexture;
    HTexture hMosaicTexture;
    HTexture hGraniteTexture;

    // Sound
    HSound hMusic;
    HSound hCollideSFX;
    HSound hUIClickSFX;
    HSound hFireSFX;
};

#endif // FADO_ASSETS_H