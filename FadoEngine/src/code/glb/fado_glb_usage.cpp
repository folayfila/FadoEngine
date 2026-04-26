/*
 * fado_glb_usage_example.cpp
 *
 * Shows how to take a parsed FGLBAsset and get it into FRenderWorld
 * via your existing LoadMesh() / LoadTexture() functions.
 *
 * Drop this code into Initialize() or a dedicated LoadGLB() function.
 */

#include "../fado_d3d.h"       // for FRenderWorld, HMesh, HTexture, LoadMesh()
#include "fado_glb.h"

// -------------------------------------------------------------------
// LoadGLBIntoWorld
//
// Parses a .glb file and uploads all meshes into world->meshes[].
// Returns the number of HMesh handles written into outHandles[].
//
// outHandles should be at least GLB_MAX_MESHES * GLB_MAX_PRIMITIVES large.
// -------------------------------------------------------------------
u32 LoadGLBIntoWorld(FRenderWorld* world, const char* filename,
                     HMesh* outHandles, u32 maxHandles)
{
    FGLBAsset asset = {};
    if (!GLB_Load(filename, &asset))
        return 0;

    u32 handleCount = 0;

    // Walk every mesh -> every primitive
    for (u32 mi = 0; mi < asset.meshCount; mi++)
    {
        FGLBMesh* mesh = &asset.meshes[mi];

        for (u32 pi = 0; pi < mesh->primitiveCount; pi++)
        {
            FGLBPrimitive* prim = &mesh->primitives[pi];
            if (!prim->vertices || !prim->indices || prim->vertexCount == 0)
            {
                continue;
            }

            if (handleCount >= maxHandles)
            {
                break;
            }

            // LoadMesh() is your existing function that calls UploadMesh()
            // and returns an HMesh handle (uint32 index into world->meshes[])
            //
            // NOTE: FGLBVertex and FTextureVertex need to match in layout.
            // FGLBVertex has position(xyz), normal(xyz), uv(uv).
            // If your FTextureVertex only has position+uv, either:
            //   a) cast FGLBVertex* and adjust stride  — simple but skips normals
            //   b) convert to FTextureVertex[] first   — shown below
            //   c) update FTextureVertex to include normals — recommended

            // Option (b): convert to FTextureVertex (pos + uv only, normals dropped)
            FTextureVertex* converted = (FTextureVertex*)malloc(prim->vertexCount * sizeof(FTextureVertex));
            if (!converted) continue;

            for (u32 v = 0; v < prim->vertexCount; v++)
            {
                converted[v].position = DirectX::XMFLOAT3(
                    prim->vertices[v].px,
                    prim->vertices[v].py,
                    prim->vertices[v].pz);
                converted[v].texture = DirectX::XMFLOAT2(
                    prim->vertices[v].u,
                    prim->vertices[v].v);
            }

            HMesh handle = 1;// LoadMesh(world, world->d3d.device, converted, prim->vertexCount, prim->indices, prim->indexCount);
            free(converted);

            outHandles[handleCount++] = handle;
        }
    }

    GLB_Free(&asset);
    return handleCount;
}

// -------------------------------------------------------------------
// Example: using it in Initialize()
// -------------------------------------------------------------------
#if 0
bool32 Initialize(FRenderWorld* world, ...)
{
    InitializeFD3D(&world->d3d, ...);
    world->camera.position = { 0.0f, 1.0f, -4.0f };

    // Load a GLB model — up to 64 primitives
    HMesh meshHandles[64] = {};
    u32   meshCount = LoadGLBIntoWorld(world, "assets\\suzanne.glb",
                                       meshHandles, 64);
    if (meshCount == 0)
    {
        MessageBox(window, L"Could not load suzanne.glb", L"Error", MB_OK);
        return false;
    }

    // Store first handle for rendering
    world->quadMesh = meshHandles[0];

    // Load a texture normally
    world->quadTexture = LoadTexture(world, world->d3d.device,
                                     world->d3d.deviceContext,
                                     "assets\\suzanne_diffuse.tga");

    InitializeTextureShader(&world->textureShader, world->d3d.device, window);
    return true;
}
#endif

// -------------------------------------------------------------------
// Example: walking the node hierarchy from a loaded asset
// (useful once you add transforms / scene graph)
// -------------------------------------------------------------------
#if 0
void PrintNodeHierarchy(FGLBAsset* asset, i32 nodeIndex, i32 depth)
{
    if (nodeIndex < 0 || (u32)nodeIndex >= asset->nodeCount) return;
    FGLBNode* node = &asset->nodes[nodeIndex];

    for (i32 d = 0; d < depth; d++) printf("  ");
    printf("Node '%s' mesh=%d\n", node->name, node->meshIndex);

    // Recurse into children (nodes that list this node as parent)
    for (u32 i = 0; i < asset->nodeCount; i++)
        if (asset->nodes[i].parentIndex == nodeIndex)
            PrintNodeHierarchy(asset, (i32)i, depth + 1);
}

void PrintAssetInfo(FGLBAsset* asset)
{
    printf("Meshes: %u  Nodes: %u  Materials: %u  Textures: %u  Images: %u\n",
           asset->meshCount, asset->nodeCount,
           asset->materialCount, asset->textureCount, asset->imageCount);

    printf("Scene roots:\n");
    for (u32 i = 0; i < asset->sceneRootCount; i++)
        PrintNodeHierarchy(asset, asset->sceneRootNodes[i], 1);
}
#endif
