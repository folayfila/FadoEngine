// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#pragma warning(push)
#pragma warning(disable : 6262)

#ifndef FADO_GLB_H
#define FADO_GLB_H

#include <stdio.h>

/*
 * fado_glb.h  —  GLB 2.0 parser, no external dependencies
 *
 * A very simple .glb loader that extracts only the meshes data, no materials, textures or whatsoever.
 * It Covers:
 *   - GLB binary container parsing (header + JSON chunk + BIN chunk)
 *   - Minimal hand-rolled JSON parser (enough for glTF)
 *   - Buffer / BufferView / Accessor resolution
 *   - Mesh primitive extraction: POSITION, NORMAL, TEXCOORD_0, indices
 *   - Node hierarchy (flat array, parent index)
 *
 *
 * Note:
 * The JSON parser is based on a really good Clause prompt, the .glb loading code was however simplified to our needs only.
 */


 // Copied so we don't include fado_types.h
 // ─────────────────────────────────────────────
typedef signed char i8;
typedef short		i16;
typedef int			i32;
typedef long long	i64;

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long  u64;

typedef float f32;
typedef double f64;

typedef bool b8;
typedef i32 b32;

typedef char c8;
typedef const char cc8;
typedef wchar_t wchar;

struct GLB_V2
{
    f32 x, y;
};

struct GLB_V3
{
    f32 x, y, z;
};
// ─────────────────────────────────────────────

#define GLB_MAX_MESHES      64
#define GLB_MAX_PRIMITIVES  16      // per mesh
#define GLB_MAX_ACCESSORS   256
#define GLB_MAX_BUFFERVIEWS 256
#define GLB_MAX_NAME        128
#define GLB_INVALID         -1
// ─────────────────────────────────────────────

/*
//> Important: Copy this from fado_d3d.h main texture vertex layout
Current:
struct FTextureVertex
{
    DXFloat3 position;
    DXFloat3 normal;
    DXFloat2 texture;
};
*/
struct GLB_FLitTextureVertex
{
    GLB_V3 position;
    GLB_V3 normal;
    GLB_V2 texture;
};

 /* -----------------------------------------------------------------------
    Vertex — interleaved, DX11-ready
    ----------------------------------------------------------------------- */
struct FGLBVertex
{
    f32 px, py, pz;       // POSITION
    f32 nx, ny, nz;       // NORMAL   (0 if absent in file)
    f32 u, v;             // TEXCOORD_0 (0 if absent)
};

/* -----------------------------------------------------------------------
   Primitive — one draw call's worth of data, fully flattened
   Primitive = a batch of vertices + indices drawn in one call.
   Example:
   A character model is a mesh with 3 Primitives: body, clothes, eyes.
   ----------------------------------------------------------------------- */
struct FGLBPrimitive
{
    FGLBVertex* vertices;
    u32*        indices;
    u32         vertexCount;
    u32         indexCount;
};

/* -----------------------------------------------------------------------
   Mesh — collection of primitives
   ----------------------------------------------------------------------- */
struct FGLBMesh
{
    c8              name[GLB_MAX_NAME];
    FGLBPrimitive   primitives[GLB_MAX_PRIMITIVES];
    u32             primitiveCount;
};

/* -----------------------------------------------------------------------
   Top-level asset
   ----------------------------------------------------------------------- */
struct FGLBAsset
{
    FGLBMesh      meshes[GLB_MAX_MESHES];
    u32           meshCount;
};

/* =======================================================================
   SECTION 1 — GLB BINARY CONTAINER
   =======================================================================
   A .glb file has exactly this layout:

   [12-byte header]
       magic    : u32  = 0x46546C67  ("glTF")
       version  : u32  = 2
       length   : u32  = total file size in bytes

   [Chunk 0 — JSON]
       chunkLength : u32
       chunkType   : u32 = 0x4E4F534A  ("JSON")
       chunkData   : chunkLength bytes of UTF-8 JSON
       padding     : 0x20 bytes to align to 4 bytes

   [Chunk 1 — BIN]
       chunkLength : u32
       chunkType   : u32 = 0x004E4942  ("BIN\0")
       chunkData   : chunkLength bytes of raw binary
       padding     : 0x00 bytes to align to 4 bytes
   ======================================================================= */

#define GLB_MAGIC       0x46546C67u
#define GLB_CHUNK_JSON  0x4E4F534Au
#define GLB_CHUNK_BIN   0x004E4942u

struct FGLBHeader
{
    u32 magic;
    u32 version;
    u32 length;
};

struct FGLBChunkHeader
{
    u32 chunkLength;
    u32 chunkType;
};

/* =======================================================================
   SECTION 2 — MINIMAL JSON PARSER
   =======================================================================
   glTF JSON is well-structured and machine-generated, so we can get away
   with a very simple recursive-descent parser. We don't build a full tree;
   instead we parse into a flat FJSONValue tagged union and provide
   helpers to navigate it.

   Supported types: null, bool, number (f64), string, array, object.
   Objects and arrays store their children in a flat FJSONValue pool
   as a linked list, using firstChild and nextSibling indices.
   ======================================================================= */

#define JSON_POOL_MAX  1024  // max total nodes across entire document

enum FJSONType : u8
{
    JSON_NULL = 0,
    JSON_BOOL = 1,
    JSON_NUMBER = 2,
    JSON_STRING = 3,
    JSON_ARRAY = 4,
    JSON_OBJECT = 5,
};

struct FJSONValue
{
    FJSONType type;
    union
    {
        b32  boolVal;
        f64 numVal;
        c8* strVal;
        i32 firstChild;  // ARRAY / OBJECT: index of first child, GLB_INVALID if empty
    };
    c8* key;             // OBJECT / children only: key is a pointer into JSON text buffer
    i32 nextSibling;     // index of next sibling in parent, GLB_INVALID if last
};

struct FJSONDoc
{
    FJSONValue pool[JSON_POOL_MAX];
    u32        poolUsed;
    c8* text;           // mutable copy of the JSON chunk (we null-terminate strings)
    u32        textLen;
};

// -----------------------------------------------------------------------
// Parser state
// -----------------------------------------------------------------------
struct FJSONParser
{
    c8* cur;
    c8* end;
    FJSONDoc* doc;
    b32 error;
};

internal void json_skip_whitespace(FJSONParser* p)
{
    while ((p->cur < p->end) &&
          ((*p->cur == ' ') || (*p->cur == '\t') ||
          (*p->cur == '\n') || (*p->cur == '\r')))
    {
        p->cur++;
    }
}

internal b8 json_expect(FJSONParser* p, c8 c)
{
    json_skip_whitespace(p);
    if ((p->cur >= p->end) || (*p->cur != c))
    {
        p->error = true;
        return false;
    }
    p->cur++;
    return true;
}

internal i32 json_alloc_node(FJSONParser* p)
{
    if (p->doc->poolUsed >= JSON_POOL_MAX)
    {
        p->error = true;
        return GLB_INVALID;
    }
    i32 idx = p->doc->poolUsed++;
    p->doc->pool[idx] = {};
    return idx;
}

internal i32 json_parse_value(FJSONParser* p);  // forward

internal i32 json_parse_string(FJSONParser* p)
{
    // Caller has already confirmed *cur == '"'
    p->cur++; // skip opening "
    c8* start = p->cur;
    while ((p->cur < p->end) && (*p->cur != '"'))
    {
        if (*p->cur == '\\')
        {
            p->cur++; // skip escaped char
        }
        p->cur++;
    }
    if (p->cur >= p->end)
    {
        p->error = true;
        return GLB_INVALID;
    }
    *p->cur = '\0'; // null-terminate in place
    p->cur++;       // skip closing "

    i32 idx = json_alloc_node(p);
    p->doc->pool[idx].type = JSON_STRING;
    p->doc->pool[idx].strVal = start;
    return idx;
}

internal i32 json_parse_number(FJSONParser* p)
{
    c8* start = p->cur;
    if (*p->cur == '-')
    {
        p->cur++;
    }
    while ((p->cur < p->end) && (*p->cur >= '0' && *p->cur <= '9'))
    {
        p->cur++;
    }
    if ((p->cur < p->end) && (*p->cur == '.'))
    {
        p->cur++;
        while ((p->cur < p->end) && ((*p->cur >= '0') && (*p->cur <= '9')))
        {
            p->cur++;
        }
    }
    if ((p->cur < p->end) && (*p->cur == 'e' || *p->cur == 'E'))
    {
        p->cur++;
        if ((p->cur < p->end) && (*p->cur == '+' || *p->cur == '-'))
        {
            p->cur++;
        }
        while ((p->cur < p->end) && (*p->cur >= '0' && *p->cur <= '9'))
        {
            p->cur++;
        }
    }
    i32 idx = json_alloc_node(p);
    if (idx == GLB_INVALID)
    {
        return GLB_INVALID;
    }
    p->doc->pool[idx].type = JSON_NUMBER;
    p->doc->pool[idx].numVal = strtod(start, nullptr);
    return idx;
}

internal i32 json_parse_array(FJSONParser* p)
{
    p->cur++; // skip '['
    i32 arrIdx = json_alloc_node(p);
    if (arrIdx == GLB_INVALID)
    {
        return GLB_INVALID;
    }
    p->doc->pool[arrIdx].type = JSON_ARRAY;
    p->doc->pool[arrIdx].firstChild = GLB_INVALID;
    p->doc->pool[arrIdx].nextSibling = GLB_INVALID;

    json_skip_whitespace(p);
    if ((p->cur < p->end) && (*p->cur == ']'))
    {
        p->cur++;
        return arrIdx;
    }

    i32 lastChild = GLB_INVALID;
    while ((p->cur < p->end) && !p->error)
    {
        i32 valIdx = json_parse_value(p);
        if (valIdx == GLB_INVALID)
        {
            break;
        }
        p->doc->pool[arrIdx].nextSibling = GLB_INVALID;

        if (lastChild == GLB_INVALID)
        {
            p->doc->pool[arrIdx].firstChild = valIdx;
        }
        else
        {
            p->doc->pool[lastChild].nextSibling = valIdx;
        }
        lastChild = valIdx;

        json_skip_whitespace(p);
        if (p->cur >= p->end)
        {
            break;
        }
        if (*p->cur == ']')
        {
            p->cur++;
            break;
        }
        if (*p->cur == ',')
        {
            p->cur++;
            continue;
        }
        p->error = true;
        break;
    }
    return arrIdx;
}

internal i32 json_parse_object(FJSONParser* p)
{
    p->cur++; // skip '{'
    i32 objIdx = json_alloc_node(p);
    if (objIdx == GLB_INVALID)
    {
        return GLB_INVALID;
    }
    p->doc->pool[objIdx].type = JSON_OBJECT;
    p->doc->pool[objIdx].firstChild = GLB_INVALID;
    p->doc->pool[objIdx].nextSibling = GLB_INVALID;

    json_skip_whitespace(p);
    if ((p->cur < p->end) && (*p->cur == '}'))
    {
        p->cur++;
        return objIdx;
    }

    i32 lastChild = GLB_INVALID;
    while ((p->cur < p->end) && !p->error)
    {
        json_skip_whitespace(p);
        if (*p->cur != '"')
        {
            p->error = true;
            break;
        }

        // parse key
        p->cur++;
        c8* key = p->cur;
        while ((p->cur < p->end) && (*p->cur != '"'))
        {
            if (*p->cur == '\\')
            {
                p->cur++;
            }
            p->cur++;
        }
        if (p->cur >= p->end)
        {
            p->error = true;
            break;
        }
        *p->cur = '\0';
        p->cur++;

        if (!json_expect(p, ':'))
        {
            break;
        }

        i32 valIdx = json_parse_value(p);
        if (valIdx == GLB_INVALID)
        {
            break;
        }
        p->doc->pool[valIdx].key = key;
        p->doc->pool[objIdx].nextSibling = GLB_INVALID;

        if (lastChild == GLB_INVALID)
        {
            p->doc->pool[objIdx].firstChild = valIdx;
        }
        else
        {
            p->doc->pool[lastChild].nextSibling = valIdx;
        }
        lastChild = valIdx;

        json_skip_whitespace(p);
        if (p->cur >= p->end)
        {
            break;
        }
        if (*p->cur == '}')
        {
            p->cur++;
            break;
        }
        if (*p->cur == ',')
        {
            p->cur++;
            continue;
        }
        p->error = true;
        break;
    }
    return objIdx;
}

internal i32 json_parse_value(FJSONParser* p)
{
    i32 result = GLB_INVALID;
    json_skip_whitespace(p);
    if (p->cur >= p->end || p->error)
    {
        return result;
    }

    c8 c = *p->cur;
    if (c == '"')
    {
        result = json_parse_string(p);
        return result;
    }
    if (c == '{')
    {
        result = json_parse_object(p);
        return result;
    }
    if (c == '[')
    {
        result = json_parse_array(p);
        return result;
    }
    if (c == 't')
    {
        p->cur += 4;
        i32 i = json_alloc_node(p);
        p->doc->pool[i].type = JSON_BOOL;
        p->doc->pool[i].boolVal = true;
        return i;
    }
    if (c == 'f')
    {
        p->cur += 5;
        i32 i = json_alloc_node(p);
        p->doc->pool[i].type = JSON_BOOL;
        p->doc->pool[i].boolVal = false;
        return i;
    }
    if (c == 'n')
    {
        p->cur += 4;
        i32 i = json_alloc_node(p);
        p->doc->pool[i].type = JSON_NULL;
        return i;
    }
    if (c == '-' || (c >= '0' && c <= '9'))
    {
        result = json_parse_number(p);
        return result;
    }

    p->error = true;
    return GLB_INVALID;
}

internal b8 json_parse(FJSONDoc* doc, c8* text, u32 textLen)
{
    doc->text = text;
    doc->textLen = textLen;
    doc->poolUsed = 0;

    FJSONParser p = {};
    p.cur = text;
    p.end = text + textLen;
    p.doc = doc;

    json_parse_value(&p);
    return !(p.error);
}

// -----------------------------------------------------------------------
// Navigation helpers
// -----------------------------------------------------------------------

// Find a child by key in an OBJECT node. Returns nullptr if not found.
internal FJSONValue* json_obj_get(FJSONDoc* doc, i32 objIdx, cc8* key)
{
    if (objIdx < 0)
    {
        return nullptr;
    }

    FJSONValue* obj = &doc->pool[objIdx];
    if (obj->type != JSON_OBJECT)
    {
        return nullptr;
    }

    i32 ci = obj->firstChild;
    while (ci != GLB_INVALID)
    {
        FJSONValue* child = &doc->pool[ci];
        if (child->key && strcmp(child->key, key) == 0)
        {
            return child;
        }
        ci = child->nextSibling;
    }
    return nullptr;
}

// Get array element by index. Returns nullptr if out of range.
internal FJSONValue* json_arr_get(FJSONDoc* doc, i32 arrIdx, u32 index)
{
    if (arrIdx < 0)
    {
        return nullptr;
    }

    FJSONValue* arr = &doc->pool[arrIdx];
    if (arr->type != JSON_ARRAY)
    {
        return nullptr;
    }

    i32 ci = arr->firstChild;
    u32 i = 0;
    while (ci != GLB_INVALID)
    {
        if (i == index)
        {
            return &doc->pool[ci];
        }
        ci = doc->pool[ci].nextSibling;
        ++i;
    }
    return nullptr;
}

// Shorthand: get a numeric value from an object key, with a default.
internal f64 json_num(FJSONDoc* doc, u32 objIdx, cc8* key, f64 def)
{
    FJSONValue* v = json_obj_get(doc, objIdx, key);
    if (!v || (v->type != JSON_NUMBER))
    {
        return def;
    }
    return v->numVal;
}

internal i32 json_int(FJSONDoc* doc, u32 objIdx, cc8* key, i32 def)
{
    i32 result = (i32)json_num(doc, objIdx, key, (f64)def);
    return result;
}

internal cc8* json_str(FJSONDoc* doc, u32 objIdx, cc8* key, cc8* def = "")
{
    FJSONValue* v = json_obj_get(doc, objIdx, key);
    if (!v || (v->type != JSON_STRING))
    {
        return def;
    }
    return v->strVal;
}

// Index of a child node (for passing to further navigation). Returns -1 if not found.
internal i32 json_child_idx(FJSONDoc* doc, i32 parentIdx, cc8* key)
{
    FJSONValue* v = json_obj_get(doc, parentIdx, key);
    if (!v)
    {
        return GLB_INVALID;
    }
    return (i32)(v - doc->pool);
}

/* =======================================================================
   SECTION 3 — INTERNAL ACCESSOR STRUCTS
   We keep raw accessor/bufferView metadata so we can resolve data ptrs.
   ======================================================================= */

   // glTF componentType constants
#define GLTF_BYTE           5120
#define GLTF_UNSIGNED_BYTE  5121
#define GLTF_SHORT          5122
#define GLTF_UNSIGNED_SHORT 5123
#define GLTF_UNSIGNED_INT   5125
#define GLTF_FLOAT          5126

struct FGLBBufferView
{
    u32 byteOffset;
    u32 byteLength;
    u32 byteStride;   // 0 = tightly packed
    i32 bufferIndex;
};

struct FGLBAccessor
{
    i32 bufferViewIndex;    // -1 = no buffer view (sparse, not supported here)
    u32 byteOffset;
    u32 componentType;      // GLTF_FLOAT etc.
    u32 count;
    u32 numComponents;      // 1=SCALAR, 2=VEC2, 3=VEC3, 4=VEC4, 9=MAT3, 16=MAT4
};

internal u32 gltf_type_components(cc8* type)
{
    if (!type) return 0;
    if (strcmp(type, "SCALAR") == 0) return 1;
    if (strcmp(type, "VEC2") == 0) return 2;
    if (strcmp(type, "VEC3") == 0) return 3;
    if (strcmp(type, "VEC4") == 0) return 4;
    if (strcmp(type, "MAT2") == 0) return 4;
    if (strcmp(type, "MAT3") == 0) return 9;
    if (strcmp(type, "MAT4") == 0) return 16;
    return 0;
}

internal u32 gltf_component_size(u32 componentType)
{
    switch (componentType)
    {
    case GLTF_BYTE:
    case GLTF_UNSIGNED_BYTE:  return 1;
    case GLTF_SHORT:
    case GLTF_UNSIGNED_SHORT: return 2;
    case GLTF_UNSIGNED_INT:
    case GLTF_FLOAT:          return 4;
    default:                  return 0;
    }
}

/* =======================================================================
   SECTION 4 — DATA ACCESS HELPERS
   Given the BIN chunk pointer and resolved accessor/bufferView, read
   individual elements cleanly.
   ======================================================================= */

   // Returns a byte pointer to element [i] of an accessor.
internal const u8* gltf_accessor_element(
    const u8* binData,
    const FGLBAccessor* acc,
    const FGLBBufferView* bv,
    u32                    elementIndex)
{
    u32 compSize = gltf_component_size(acc->componentType);
    u32 elemBytes = compSize * acc->numComponents;
    u32 stride = (bv->byteStride != 0) ? bv->byteStride : elemBytes;

    const u8* base = binData + bv->byteOffset + acc->byteOffset;
    return base + (u64)elementIndex * stride;
}

// Read a float from a byte pointer, converting from the accessor's component type.
internal f32 gltf_read_float(const u8* ptr, u32 componentType)
{
    switch (componentType)
    {
    case GLTF_FLOAT:          return *(const f32*)ptr;
    case GLTF_UNSIGNED_BYTE:  return (f32)*ptr / 255.0f;
    case GLTF_UNSIGNED_SHORT: return (f32) * (const u16*)ptr / 65535.0f;
    case GLTF_SHORT:          return (f32) * (const i16*)ptr / 32767.0f;
    default:                  return 0.0f;
    }
}

// Read a u32 index (supports UNSIGNED_BYTE, UNSIGNED_SHORT, UNSIGNED_INT).
internal u32 gltf_read_index(const u8* ptr, u32 componentType)
{
    switch (componentType)
    {
    case GLTF_UNSIGNED_BYTE:  return (u32)*ptr;
    case GLTF_UNSIGNED_SHORT: return (u32) * (const u16*)ptr;
    case GLTF_UNSIGNED_INT:   return *(const u32*)ptr;
    default:                  return 0;
    }
}

/* =======================================================================
   SECTION 5 — MAIN PARSER
   ======================================================================= */

internal b8 GLB_Load(cc8* filename, FGLBAsset* out)
{
    // ----------------------------------------------------------------
    // 5.1  Read entire file into memory
    // ----------------------------------------------------------------
    FILE* f = nullptr;
    fopen_s(&f, filename, "rb");
    if (!f)
    {
        fprintf(stderr, "GLB_Load: cannot open '%s'\n", filename);
        return false;
    }
    fseek(f, 0, SEEK_END);
    u32 fileSize = (u32)ftell(f);
    fseek(f, 0, SEEK_SET);

    u8* fileData = (u8*)malloc(fileSize + 1); // +1 for safety null term
    if (!fileData)
    {
        fclose(f);
        return false;
    }
    fread(fileData, 1, fileSize, f);
    fclose(f);
    fileData[fileSize] = '\0';

    // ----------------------------------------------------------------
    // 5.2  Validate GLB header
    // ----------------------------------------------------------------
    if (fileSize < 12)
    {
        fprintf(stderr, "GLB_Load: file too small\n");
        return false;
    }

    FGLBHeader* header = (FGLBHeader*)fileData;
    if (header->magic != GLB_MAGIC)
    {
        fprintf(stderr, "GLB_Load: bad magic (not a .glb file)\n");
        return false;
    }
    if (header->version != 2)
    {
        fprintf(stderr, "GLB_Load: unsupported GLB version %u\n", header->version);
        return false;
    }

    // ----------------------------------------------------------------
    // 5.3  Locate JSON chunk
    // ----------------------------------------------------------------
    u8* cursor = fileData + 12;
    u8* fileEnd = fileData + fileSize;

    FGLBChunkHeader* jsonChunkHdr = (FGLBChunkHeader*)cursor;
    if (((cursor + 8) > fileEnd) || (jsonChunkHdr->chunkType != GLB_CHUNK_JSON))
    {
        fprintf(stderr, "GLB_Load: first chunk is not JSON\n");
        return false;
    }
    c8* jsonText = (c8*)(cursor + 8);
    u32   jsonLen = jsonChunkHdr->chunkLength;
    cursor += 8 + ((jsonLen + 3) & ~3u); // advance past chunk (padded to 4)

    // ----------------------------------------------------------------
    // 5.4  Locate BIN chunk
    // ----------------------------------------------------------------
    const u8* binData = nullptr;
    u32       binLength = 0;

    if ((cursor + 8) <= fileEnd)
    {
        FGLBChunkHeader* binChunkHdr = (FGLBChunkHeader*)cursor;
        if (binChunkHdr->chunkType == GLB_CHUNK_BIN)
        {
            binData = cursor + 8;
            binLength = binChunkHdr->chunkLength;
        }
    }

    // ----------------------------------------------------------------
    // 5.5  Parse JSON
    // ----------------------------------------------------------------
    // json_parse modifies jsonText in place (null-terminates strings),
    // which is fine because we own fileData.
    FJSONDoc doc = {};

    if (!json_parse(&doc, jsonText, jsonLen))
    {
        fprintf(stderr, "GLB_Load: JSON parse failed\n");
        return false;
    }

    // Root object is always pool[0]
    i32 rootIdx = 0;

    // ----------------------------------------------------------------
    // 5.6  Parse bufferViews
    // ----------------------------------------------------------------
    FGLBBufferView bufferViews[GLB_MAX_BUFFERVIEWS] = {};

    u32 bufferViewCount = 0;

    i32 bvArrayIdx = json_child_idx(&doc, rootIdx, "bufferViews");
    if (bvArrayIdx != GLB_INVALID)
    {
        i32 ci = doc.pool[bvArrayIdx].firstChild;
        while ((ci != GLB_INVALID) && (bufferViewCount < GLB_MAX_BUFFERVIEWS))
        {
            FJSONValue* bvNode = &doc.pool[ci];
            if (!bvNode || (bvNode->type != JSON_OBJECT))
            {
                ci = bvNode->nextSibling;
                continue;
            }
            i32 bvIdx = ci;
            u32 i = bufferViewCount;

            bufferViews[i].byteOffset = (u32)json_num(&doc, bvIdx, "byteOffset", 0);
            bufferViews[i].byteLength = (u32)json_num(&doc, bvIdx, "byteLength", 0);
            bufferViews[i].byteStride = (u32)json_num(&doc, bvIdx, "byteStride", 0);
            bufferViews[i].bufferIndex = json_int(&doc, bvIdx, "buffer", 0);

            bufferViewCount++;
            ci = bvNode->nextSibling;
        }
    }

    // ----------------------------------------------------------------
    // 5.7  Parse accessors
    // ----------------------------------------------------------------
    FGLBAccessor accessors[GLB_MAX_ACCESSORS] = {};

    u32 accessorCount = 0;

    i32 accArrayIdx = json_child_idx(&doc, rootIdx, "accessors");
    if (accArrayIdx != GLB_INVALID)
    {
        i32 ci = doc.pool[accArrayIdx].firstChild;
        while ((ci != GLB_INVALID) && (accessorCount < GLB_MAX_ACCESSORS))
        {
            FJSONValue* accNode = &doc.pool[ci];

            if (!accNode || (accNode->type != JSON_OBJECT))
            {
                ci = accNode->nextSibling;
                continue;
            }
            i32 aIdx = ci;
            u32 i = accessorCount;

            accessors[i].bufferViewIndex = json_int(&doc, aIdx, "bufferView", -1);
            accessors[i].byteOffset = (u32)json_num(&doc, aIdx, "byteOffset", 0);
            accessors[i].componentType = (u32)json_num(&doc, aIdx, "componentType", 0);
            accessors[i].count = (u32)json_num(&doc, aIdx, "count", 0);

            cc8* typeStr = json_str(&doc, aIdx, "type", "SCALAR");
            accessors[i].numComponents = gltf_type_components(typeStr);

            accessorCount++;
            ci = accNode->nextSibling;
        }
    }

    // ----------------------------------------------------------------
    // 5.8  Parse meshes and their primitives
    // ----------------------------------------------------------------
    i32 meshArrayIdx = json_child_idx(&doc, rootIdx, "meshes");
    if (meshArrayIdx != GLB_INVALID)
    {
        i32 mci = doc.pool[meshArrayIdx].firstChild;
        while ((mci != GLB_INVALID) && (out->meshCount < GLB_MAX_MESHES))
        {
            FJSONValue* meshNode = &doc.pool[mci];
            if (!meshNode || (meshNode->type != JSON_OBJECT))
            {
                mci = meshNode->nextSibling;
                continue;
            }

            i32 meshNodeIdx = mci;
            u32 mi = out->meshCount;

            cc8* meshName = json_str(&doc, meshNodeIdx, "name", "");
            strncpy_s(out->meshes[mi].name, meshName, GLB_MAX_NAME - 1);

            i32 primArrayIdx = json_child_idx(&doc, meshNodeIdx, "primitives");
            if (primArrayIdx == GLB_INVALID)
            {
                mci = meshNode->nextSibling;
                continue;
            }

            i32 pci = doc.pool[primArrayIdx].firstChild;
            while ((pci != GLB_INVALID) && (out->meshes[mi].primitiveCount < GLB_MAX_PRIMITIVES))
            {
                FJSONValue* primNode = &doc.pool[pci];
                if (!primNode || (primNode->type != JSON_OBJECT))
                {
                    pci = primNode->nextSibling;
                    continue;
                }
                i32 primNodeIdx = pci;
                u32 pi = out->meshes[mi].primitiveCount;

                FGLBPrimitive* prim = &out->meshes[mi].primitives[pi];

                // ---- Attributes ----
                i32 attribIdx = json_child_idx(&doc, primNodeIdx, "attributes");
                if (attribIdx == GLB_INVALID)
                {
                    pci = primNode->nextSibling;
                    continue;
                }

                i32 posAccIdx = json_int(&doc, attribIdx, "POSITION", -1);
                i32 normAccIdx = json_int(&doc, attribIdx, "NORMAL", -1);
                i32 uvAccIdx = json_int(&doc, attribIdx, "TEXCOORD_0", -1);
                i32 idxAccIdx = json_int(&doc, primNodeIdx, "indices", -1);

                if (posAccIdx < 0 || (u32)posAccIdx >= accessorCount)
                {
                    pci = primNode->nextSibling;
                    continue;
                }

                FGLBAccessor* posAcc = &accessors[posAccIdx];
                prim->vertexCount = posAcc->count;

                // Allocate output vertex array
                prim->vertices = (FGLBVertex*)calloc(prim->vertexCount, sizeof(FGLBVertex));
                if (!prim->vertices)
                {
                    pci = primNode->nextSibling;
                    continue;
                }

                // ---- Copy POSITION ----
                if (posAcc->bufferViewIndex >= 0 && binData)
                {
                    FGLBBufferView* bv = &bufferViews[posAcc->bufferViewIndex];
                    for (u32 v = 0; v < prim->vertexCount; v++)
                    {
                        const u8* elem = gltf_accessor_element(binData, posAcc, bv, v);
                        prim->vertices[v].px = gltf_read_float(elem + 0, posAcc->componentType);
                        prim->vertices[v].py = gltf_read_float(elem + 4, posAcc->componentType);
                        prim->vertices[v].pz = gltf_read_float(elem + 8, posAcc->componentType);
                    }
                }

                // ---- Copy NORMAL ----
                if (normAccIdx >= 0 && (u32)normAccIdx < accessorCount && binData)
                {
                    FGLBAccessor* normAcc = &accessors[normAccIdx];
                    if (normAcc->bufferViewIndex >= 0)
                    {
                        FGLBBufferView* bv = &bufferViews[normAcc->bufferViewIndex];
                        u32 count = (normAcc->count < prim->vertexCount) ? normAcc->count : prim->vertexCount;
                        for (u32 v = 0; v < count; v++)
                        {
                            const u8* elem = gltf_accessor_element(binData, normAcc, bv, v);
                            prim->vertices[v].nx = gltf_read_float(elem + 0, normAcc->componentType);
                            prim->vertices[v].ny = gltf_read_float(elem + 4, normAcc->componentType);
                            prim->vertices[v].nz = gltf_read_float(elem + 8, normAcc->componentType);
                        }
                    }
                }

                // ---- Copy TEXCOORD_0 ----
                if (uvAccIdx >= 0 && (u32)uvAccIdx < accessorCount && binData)
                {
                    FGLBAccessor* uvAcc = &accessors[uvAccIdx];
                    if (uvAcc->bufferViewIndex >= 0)
                    {
                        FGLBBufferView* bv = &bufferViews[uvAcc->bufferViewIndex];
                        u32 compSize = gltf_component_size(uvAcc->componentType);
                        u32 count = (uvAcc->count < prim->vertexCount) ? uvAcc->count : prim->vertexCount;
                        for (u32 v = 0; v < count; v++)
                        {
                            const u8* elem = gltf_accessor_element(binData, uvAcc, bv, v);
                            prim->vertices[v].u = gltf_read_float(elem + 0, uvAcc->componentType);
                            prim->vertices[v].v = gltf_read_float(elem + compSize, uvAcc->componentType);
                        }
                    }
                }

                // ---- Copy indices ----
                if (idxAccIdx >= 0 && (u32)idxAccIdx < accessorCount && binData)
                {
                    FGLBAccessor* idxAcc = &accessors[idxAccIdx];
                    if (idxAcc->bufferViewIndex >= 0)
                    {
                        FGLBBufferView* bv = &bufferViews[idxAcc->bufferViewIndex];
                        prim->indexCount = idxAcc->count;
                        prim->indices = (u32*)malloc(prim->indexCount * sizeof(u32));
                        if (prim->indices)
                        {
                            u32 compSize = gltf_component_size(idxAcc->componentType);
                            for (u32 ii = 0; ii < prim->indexCount; ii++)
                            {
                                const u8* elem = gltf_accessor_element(binData, idxAcc, bv, ii);
                                prim->indices[ii] = gltf_read_index(elem, idxAcc->componentType);
                            }
                        }
                    }
                }

                // If the mesh has no index buffer, generate sequential indices
                if (!prim->indices && prim->vertexCount > 0)
                {
                    prim->indexCount = prim->vertexCount;
                    prim->indices = (u32*)malloc(prim->indexCount * sizeof(u32));
                    if (prim->indices)
                        for (u32 ii = 0; ii < prim->indexCount; ii++)
                            prim->indices[ii] = ii;
                }

                pci = primNode->nextSibling;
                out->meshes[mi].primitiveCount++;
            }

            mci = meshNode->nextSibling;
            out->meshCount++;
        }
    }

    return true;
}

internal void GLB_Free(FGLBAsset* asset)
{
    if (!asset) return;

    // Free all primitive vertex/index arrays
    for (u32 mi = 0; mi < asset->meshCount; mi++)
    {
        for (u32 pi = 0; pi < asset->meshes[mi].primitiveCount; pi++)
        {
            FGLBPrimitive* prim = &asset->meshes[mi].primitives[pi];
            free(prim->vertices);
            free(prim->indices);
            prim->vertices = nullptr;
            prim->indices = nullptr;
        }
    }
}

#endif // FADO_GLB_H

#pragma warning(pop)