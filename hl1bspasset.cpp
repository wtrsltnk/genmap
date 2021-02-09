#include "hl1bspasset.h"

#include "hl1bsptypes.h"
#include "stb_rect_pack.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stb_image.h>

namespace fs = std::filesystem;
using namespace valve::hl1;

BspAsset::BspAsset(
    IFileSystem *fs)
    : Asset(fs),
      _header(nullptr)
{}

BspAsset::~BspAsset()
{}

bool BspAsset::Load(
    const std::string &filename)
{
    if (!_fs->LoadFile(filename, data))
    {
        return false;
    }

    _header = (tBSPHeader *)data.data;

    LoadLump(data, _planeData, HL1_BSP_PLANELUMP);
    LoadLump(data, _textureData, HL1_BSP_TEXTURELUMP);
    LoadLump(data, _verticesData, HL1_BSP_VERTEXLUMP);
    LoadLump(data, _nodeData, HL1_BSP_NODELUMP);
    LoadLump(data, _texinfoData, HL1_BSP_TEXINFOLUMP);
    LoadLump(data, _faceData, HL1_BSP_FACELUMP);
    LoadLump(data, _lightingData, HL1_BSP_LIGHTINGLUMP);
    LoadLump(data, _clipnodeData, HL1_BSP_CLIPNODELUMP);
    LoadLump(data, _leafData, HL1_BSP_LEAFLUMP);
    LoadLump(data, _marksurfaceData, HL1_BSP_MARKSURFACELUMP);
    LoadLump(data, _edgeData, HL1_BSP_EDGELUMP);
    LoadLump(data, _surfedgeData, HL1_BSP_SURFEDGELUMP);
    LoadLump(data, _modelData, HL1_BSP_MODELLUMP);

    Array<byte> entityData;
    if (LoadLump(data, entityData, HL1_BSP_ENTITYLUMP))
    {
        _entities = BspAsset::LoadEntities(entityData);
    }

    Array<byte> visibilityData;
    if (LoadLump(data, visibilityData, HL1_BSP_VISIBILITYLUMP))
    {
        _visLeafs = BspAsset::LoadVisLeafs(visibilityData, _leafData, _modelData);
    }

    std::vector<WadAsset *> wads;
    tBSPEntity *worldspawn = FindEntityByClassname("worldspawn");
    if (worldspawn != nullptr)
    {
        wads = WadAsset::LoadWads(worldspawn->keyvalues["wad"], filename, _fs);
    }

    LoadTextures(_textures, wads);
    WadAsset::UnloadWads(wads);

    LoadFacesWithLightmaps(_faces, _lightMaps, _vertices);

    LoadModels();

    LoadSkyTextures();

    return true;
}

void SkipAllSpaceCharacters(
    const valve::byte *&itr,
    const valve::byte *end)
{
    while (itr[0] <= ' ' && itr != end)
    {
        itr++; // skip all space characters
    }
}

void GetAllWithinQuotes(
    std::string &value,
    const valve::byte *&itr,
    const valve::byte *end)
{
    itr++; // skip the quote
    while (itr[0] != '\"' && itr != end)
    {
        value += (*itr++);
    }

    itr++; // skip the quote

    SkipAllSpaceCharacters(itr, end);
}

void MoveToNextEntity(
    const valve::byte *&itr,
    const valve::byte *end)
{
    while (itr[0] != '{' && itr != end)
    {
        itr++; // skip to the next entity
    }
}

bool valve::hl1::BspAsset::LoadSkyTextures()
{
    const char *shortNames[] = {"bk", "dn", "ft", "lf", "rt", "up"};
    std::string sky = "dusk";

    tBSPEntity *worldspawn = FindEntityByClassname("worldspawn");
    if (worldspawn != nullptr)
    {
        auto skyname = worldspawn->keyvalues.find("skyname");
        if (skyname != worldspawn->keyvalues.end())
        {
            sky = skyname->second;
        }
    }

    spdlog::info("loading sky {}", sky);

    for (int i = 0; i < 6; i++)
    {
        valve::Array<valve::byte> buffer;
        auto filenametga = fmt::format("gfx/env/{}{}.tga", sky, shortNames[i]);
        auto location = _fs->LocateFile(filenametga);

        spdlog::debug("loading {} at index {}", shortNames[i], i);

        fs::path fullPath;
        if (!location.empty())
        {
            fullPath = fs::path(location) / fs::path(filenametga);
        }
        else
        {
            auto filenamebmp = fmt::format("gfx/env/{}{}.bmp", sky, shortNames[i]);
            location = _fs->LocateFile(filenamebmp);

            if (!location.empty())
            {
                fullPath = fs::path(location) / fs::path(filenamebmp);
            }
            else
            {
                spdlog::error("unable to find sky texture {} in either tga and bmp", shortNames[i]);
                return false;
            }
        }

        if (!_fs->LoadFile(fullPath.generic_string(), buffer))
        {
            spdlog::error("failed to load sky texture");
            return false;
        }

        int x, y, n;
        unsigned char *data = stbi_load_from_memory(buffer.data, buffer.count, &x, &y, &n, 0);
        if (data != nullptr)
        {
            _skytextures[i] = new valve::Texture();
            _skytextures[i]->SetName(fs::relative(fullPath, _fs->Root() / fs::path(_fs->Mod())).generic_string());
            _skytextures[i]->SetData(x, y, n, data, false);
        }
        else
        {
            spdlog::error("unable to load tga or bmp from file data");

            return false;
        }
    }

    return true;
}

std::vector<sBSPEntity> BspAsset::LoadEntities(
    const Array<byte> &entityData)
{
    const byte *itr = entityData.data;
    const byte *end = entityData.data + entityData.count;

    std::string key, value;
    std::vector<tBSPEntity> entities;
    while (itr[0] == '{')
    {
        tBSPEntity entity;
        itr++; // skip the bracket

        SkipAllSpaceCharacters(itr, end);

        while (itr[0] != '}')
        {
            key = "";
            value = "";

            GetAllWithinQuotes(key, itr, end);

            GetAllWithinQuotes(value, itr, end);

            entity.keyvalues.insert(std::make_pair(key, value));

            if (key == "classname")
            {
                entity.classname = value;
            }
        }

        entities.push_back(entity);

        MoveToNextEntity(itr, end);
    }
    return entities;
}

tBSPEntity *BspAsset::FindEntityByClassname(
    const std::string &classname)
{
    for (std::vector<tBSPEntity>::iterator i = _entities.begin(); i != _entities.end(); ++i)
    {
        tBSPEntity *e = &(*i);
        if (e->classname == classname)
        {
            return e;
        }
    }

    return nullptr;
}

std::vector<tBSPVisLeaf> BspAsset::LoadVisLeafs(
    const Array<byte> &visdata,
    const Array<tBSPLeaf> &leafs,
    const Array<tBSPModel> &models)
{
    std::vector<tBSPVisLeaf> visLeafs = std::vector<tBSPVisLeaf>(leafs.count);

    for (int i = 0; i < leafs.count; i++)
    {
        visLeafs[i].leafs = 0;
        visLeafs[i].leafCount = 0;
        int visOffset = leafs[i].visofs;

        for (int j = 1; j < models[0].visLeafs; visOffset++)
        {
            if (visdata[visOffset] == 0)
            {
                visOffset++;
                j += (visdata[visOffset] << 3);
            }
            else
            {
                for (byte bit = 1; bit; bit <<= 1, j++)
                {
                    if (visdata[visOffset] & bit)
                        visLeafs[i].leafCount++;
                }
            }
        }

        if (visLeafs[i].leafCount > 0)
        {
            visLeafs[i].leafs = new int[visLeafs[i].leafCount];
            if (visLeafs[i].leafs == 0)
                return visLeafs;
        }
    }

    for (int i = 0; i < leafs.count; i++)
    {
        int visOffset = leafs[i].visofs;
        int index = 0;
        for (int j = 1; j < models[0].visLeafs; visOffset++)
        {
            if (visdata[visOffset] == 0)
            {
                visOffset++;
                j += (visdata[visOffset] << 3);
            }
            else
            {
                for (byte bit = 1; bit; bit <<= 1, j++)
                {
                    if (visdata[visOffset] & bit)
                    {
                        visLeafs[i].leafs[index++] = j;
                    }
                }
            }
        }
    }

    return visLeafs;
}

bool BspAsset::LoadFacesWithLightmaps(
    std::vector<tFace> &faces,
    std::vector<Texture *> &tempLightmaps,
    std::vector<tVertex> &vertices)
{
    // Allocate the arrays for faces and lightmaps
    tempLightmaps.resize(_faceData.count);

    Texture whiteTexture;
    unsigned char data[8 * 8 * 3];
    memset(data, 255, 8 * 8 * 3);
    whiteTexture.SetData(8, 8, 3, data);

    for (int f = 0; f < _faceData.count; f++)
    {
        tBSPFace &in = _faceData[f];
        tBSPMipTexHeader *mip = GetMiptex(_texinfoData[in.texinfo].miptexIndex);
        tFace out;

        out.firstVertex = vertices.size();
        out.vertexCount = in.edgeCount;
        out.flags = _texinfoData[in.texinfo].flags;
        out.texture = _texinfoData[in.texinfo].miptexIndex;
        out.lightmap = f;
        out.plane = glm::vec4(
            _planeData[in.planeIndex].normal[0],
            _planeData[in.planeIndex].normal[1],
            _planeData[in.planeIndex].normal[2],
            _planeData[in.planeIndex].distance);

        // Flip face normal when side == 1
        if (in.side == 1)
        {
            out.plane[0] = -out.plane[0];
            out.plane[1] = -out.plane[1];
            out.plane[2] = -out.plane[2];
            out.plane[3] = -out.plane[3];
        }

        // Calculate and grab the lightmap buffer
        float min[2], max[2];
        CalculateSurfaceExtents(in, min, max);

        tempLightmaps[f] = new Texture();
        tempLightmaps[f]->SetRepeat(false);

        // Skip the lightmaps for faces with special flags
        if (out.flags == 0)
        {
            if (!LoadLightmap(in, *tempLightmaps[f], min, max))
            {
                spdlog::error("failed to load lightmap {}", f);
            }
        }
        else
        {
            tempLightmaps[f]->CopyFrom(whiteTexture);
        }

        float lw = float(tempLightmaps[f]->Width());
        float lh = float(tempLightmaps[f]->Height());
        float halfsizew = (min[0] + max[0]) / 2.0f;
        float halfsizeh = (min[1] + max[1]) / 2.0f;

        // Create a vertex list for this face
        for (int e = 0; e < in.edgeCount; e++)
        {
            tVertex v;

            // Get the edge index
            int ei = _surfedgeData[in.firstEdge + e];
            // Determine the vertex based on the edge index
            v.position = _verticesData[_edgeData[ei < 0 ? -ei : ei].vertex[ei < 0 ? 1 : 0]].point;

            // Copy the normal from the plane
            v.normal = glm::vec3(out.plane);

            // Reset the bone so its not used
            v.bone = -1;

            tBSPTexInfo &ti = _texinfoData[in.texinfo];
            float s = glm::dot(v.position, glm::vec3(ti.vecs[0][0], ti.vecs[0][1], ti.vecs[0][2])) + ti.vecs[0][3];
            float t = glm::dot(v.position, glm::vec3(ti.vecs[1][0], ti.vecs[1][1], ti.vecs[1][2])) + ti.vecs[1][3];

            // Calculate the texture texcoords
            v.texcoords[0] = glm::vec2(s / float(mip->width), t / float(mip->height));

            // Calculate the lightmap texcoords
            v.texcoords[1] = glm::vec2(((lw / 2.0f) + (s - halfsizew) / 16.0f) / lw, ((lh / 2.0f) + (t - halfsizeh) / 16.0f) / lh);

            vertices.push_back(v);
        }
        faces.push_back(out);
    }

    return true;
}

bool BspAsset::LoadTextures(
    std::vector<Texture *> &textures,
    const std::vector<WadAsset *> &wads)
{
    Array<int> textureTable(int(*_textureData.data), (int *)(_textureData.data + sizeof(int)));

    for (int t = 0; t < int(*_textureData.data); t++)
    {
        const unsigned char *textureData = _textureData.data + textureTable[t];

        tBSPMipTexHeader *miptex = (tBSPMipTexHeader *)textureData;

        auto tex = new Texture(miptex->name);

        if (miptex->offsets[0] == 0)
        {
            for (std::vector<WadAsset *>::const_iterator i = wads.cbegin(); i != wads.cend(); ++i)
            {
                WadAsset *wad = *i;

                textureData = wad->LumpData(wad->IndexOf(miptex->name));

                if (textureData != nullptr)
                {
                    break;
                }
            }
        }

        if (textureData != nullptr)
        {
            miptex = (tBSPMipTexHeader *)textureData;
            int s = miptex->width * miptex->height;
            int bpp = 4;
            int paletteOffset = miptex->offsets[0] + s + (s / 4) + (s / 16) + (s / 64) + sizeof(short);

            // Get the miptex data and palette
            const unsigned char *source0 = textureData + miptex->offsets[0];
            const unsigned char *palette = textureData + paletteOffset;

            unsigned char *destination = new unsigned char[s * bpp];

            for (int i = 0; i < s; i++)
            {
                unsigned r = palette[source0[i] * 3];
                unsigned g = palette[source0[i] * 3 + 1];
                unsigned b = palette[source0[i] * 3 + 2];
                unsigned a = 255;

                // Do we need a transparent pixel
                if (tex->Name()[0] == '{' && b >= 255)
                {
                    r = g = b = a = 0;
                }

                destination[i * 4 + 0] = r;
                destination[i * 4 + 1] = g;
                destination[i * 4 + 2] = b;
                destination[i * 4 + 3] = a;
            }

            tex->SetData(miptex->width, miptex->height, bpp, destination);

            delete[] destination;
        }
        else
        {
            spdlog::error("Texture \"{0}\" not found, using default texture", miptex->name);

            tex->DefaultTexture();
        }

        textures.push_back(tex);
    }

    return true;
}

tBSPMipTexHeader *BspAsset::GetMiptex(
    int index)
{
    tBSPMipTexOffsetTable *bspMiptexTable = (tBSPMipTexOffsetTable *)_textureData.data;

    if (index >= 0 && bspMiptexTable->miptexCount > index)
    {
        return (tBSPMipTexHeader *)(_textureData.data + bspMiptexTable->offsets[index]);
    }

    return 0;
}

int BspAsset::FaceFlags(
    size_t index)
{
    if (_faces.size() > index)
    {
        return _faces[index].flags;
    }

    return -1;
}

//
// the following computations are based on:
// PolyEngine (c) Alexey Goloshubin and Quake I source by id Software
//

void BspAsset::CalculateSurfaceExtents(
    const tBSPFace &in,
    float min[2],
    float max[2]) const
{
    const tBSPTexInfo *t = &_texinfoData[in.texinfo];

    min[0] = min[1] = 999999;
    max[0] = max[1] = -999999;

    for (int i = 0; i < in.edgeCount; i++)
    {
        const tBSPVertex *v;
        int e = _surfedgeData[in.firstEdge + i];

        if (e >= 0)
        {
            v = &_verticesData[_edgeData[e].vertex[0]];
        }
        else
        {
            v = &_verticesData[_edgeData[-e].vertex[1]];
        }

        for (int j = 0; j < 2; j++)
        {
            auto val = (v->point[0] * t->vecs[j][0]) + (v->point[1] * t->vecs[j][1]) + (v->point[2] * t->vecs[j][2]) + t->vecs[j][3];

            if (val < min[j])
            {
                min[j] = val;
            }

            if (val > max[j])
            {
                max[j] = val;
            }
        }
    }
}

bool BspAsset::LoadLightmap(
    const tBSPFace &in,
    Texture &out,
    float min[2],
    float max[2])
{
    // compute lightmap size
    int size[2];

    for (int c = 0; c < 2; c++)
    {
        float tmin = floorf(min[c] / 16.0f);
        float tmax = ceilf(max[c] / 16.0f);

        size[c] = (int)(tmax - tmin);
    }

    out.SetData(size[0] + 1, size[1] + 1, 3, _lightingData.data + in.lightOffset, false);

    return true;
}

bool BspAsset::LoadModels()
{
    for (int m = 0; m < _modelData.count; m++)
    {
        tModel model;

        model.position = _modelData[m].origin;
        model.firstFace = _modelData[m].firstFace;
        model.faceCount = _modelData[m].faceCount;

        _models.push_back(model);
    }

    return true;
}
