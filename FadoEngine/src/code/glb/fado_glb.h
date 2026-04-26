#ifndef FADO_GLB_H
#define FADO_GLB_H

/*
 * fado_glb.h  —  GLB 2.0 parser, no external dependencies
 *
 * WHAT THIS COVERS:
 *   - GLB binary container parsing (header + JSON chunk + BIN chunk)
 *   - Minimal hand-rolled JSON parser (enough for glTF, not a full RFC 8259 impl)
 *   - Buffer / BufferView / Accessor resolution
 *   - Mesh primitive extraction: POSITION, NORMAL, TEXCOORD_0, indices
 *   - Node hierarchy (flat array, parent index)
 *   - Scene root nodes
 *   - Material base color texture index
 *
 *
 * USAGE:
 *   FGLBAsset asset = {};
 *   if (GLB_Load("model.glb", &asset))
 *   {
 *       // access asset.meshes[], asset.nodes[], etc.
 *       GLB_Free(&asset);
 *   }
 *
 * OUTPUT VERTEX FORMAT:
 *   Each mesh primitive is flattened to interleaved FGLBVertex[] + uint32 indices[].
 *   Ready to drop straight into UploadMesh().
 */

#include "../fado_types.h"

#define GLB_MAX_MESHES      256
#define GLB_MAX_PRIMITIVES  16      // per mesh
#define GLB_MAX_NODES       1024
#define GLB_MAX_MATERIALS   256
#define GLB_MAX_TEXTURES    256
#define GLB_MAX_IMAGES      256
#define GLB_MAX_ACCESSORS   1024
#define GLB_MAX_BUFFERVIEWS 1024
#define GLB_MAX_NAME        128
#define GLB_INVALID         0xFFFFFFFF

/* -----------------------------------------------------------------------
   Vertex — interleaved, DX11-ready
   ----------------------------------------------------------------------- */
struct FGLBVertex
{
    f32 px, py, pz;       // POSITION
    f32 nx, ny, nz;       // NORMAL   (0 if absent in file)
    f32 u,  v;            // TEXCOORD (0 if absent)
};

/* -----------------------------------------------------------------------
   Primitive — one draw call's worth of data, fully flattened
   ----------------------------------------------------------------------- */
struct FGLBPrimitive
{
    FGLBVertex* vertices;
    u32*        indices;
    u32         vertexCount;
    u32         indexCount;
    i32         materialIndex;   // -1 = none
};

/* -----------------------------------------------------------------------
   Mesh — collection of primitives
   ----------------------------------------------------------------------- */
struct FGLBMesh
{
    char            name[GLB_MAX_NAME];
    FGLBPrimitive   primitives[GLB_MAX_PRIMITIVES];
    u32             primitiveCount;
};

/* -----------------------------------------------------------------------
   Node
   ----------------------------------------------------------------------- */
struct FGLBNode
{
    char  name[GLB_MAX_NAME];
    i32   meshIndex;        // -1 = none
    i32   parentIndex;      // -1 = root
    f32   translation[3];   // local transform components
    f32   rotation[4];      // quaternion xyzw
    f32   scale[3];
    bool32  hasTRS;           // true if any TRS values were present
};

/* -----------------------------------------------------------------------
   Material (minimal — just base color texture for now)
   ----------------------------------------------------------------------- */
struct FGLBMaterial
{
    char  name[GLB_MAX_NAME];
    i32   baseColorTextureIndex;   // index into asset.textures[], -1 = none
    f32   baseColorFactor[4];      // RGBA, default 1,1,1,1
};

/* -----------------------------------------------------------------------
   Texture / Image
   ----------------------------------------------------------------------- */
struct FGLBTexture
{
    i32 imageIndex;    // index into asset.images[]
    i32 samplerIndex;  // -1 if none
};

struct FGLBImage
{
    // If the image is embedded in the BIN chunk, data/byteLength are valid.
    // mimeType is "image/png" or "image/jpeg".
    const u8* data;
    u32       byteLength;
    char      mimeType[32];
    char      name[GLB_MAX_NAME];
};

/* -----------------------------------------------------------------------
   Top-level asset
   ----------------------------------------------------------------------- */
struct FGLBAsset
{
    FGLBMesh      meshes[GLB_MAX_MESHES];
    u32           meshCount;

    FGLBNode      nodes[GLB_MAX_NODES];
    u32           nodeCount;

    FGLBMaterial  materials[GLB_MAX_MATERIALS];
    u32           materialCount;

    FGLBTexture   textures[GLB_MAX_TEXTURES];
    u32           textureCount;

    FGLBImage     images[GLB_MAX_IMAGES];
    u32           imageCount;

    i32           sceneRootNodes[GLB_MAX_NODES];
    u32           sceneRootCount;

    // Internal — the raw file bytes, kept alive while asset is live.
    u8*  _fileData;
    u32  _fileSize;
};

/* -----------------------------------------------------------------------
   Public API
   ----------------------------------------------------------------------- */

// Load a .glb file. Returns true on success.
// Call GLB_Free when done.
bool32 GLB_Load(const char* filename, FGLBAsset* out);

// Free all heap memory owned by the asset.
void GLB_Free(FGLBAsset* asset);

#endif // FADO_GLB_H
