#include "hl1bspasset.h"

#include "hl1bsptypes.h"
#include "stb_rect_pack.h"
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stb_image.h>

namespace fs = std::filesystem;
using namespace valve::hl1;

BspFile::BspFile(
    byte *data)
    : _entityData(LoadLump<byte, HL1_BSP_ENTITYLUMP>(data)),
      _planes(LoadLump<tBSPPlane, HL1_BSP_PLANELUMP>(data)),
      _textureData(LoadLump<byte, HL1_BSP_TEXTURELUMP>(data)),
      _verticesData(LoadLump<tBSPVertex, HL1_BSP_VERTEXLUMP>(data)),
      _visData(LoadLump<byte, HL1_BSP_VISIBILITYLUMP>(data)),
      _nodeData(LoadLump<tBSPNode, HL1_BSP_NODELUMP>(data)),
      _texinfoData(LoadLump<tBSPTexInfo, HL1_BSP_TEXINFOLUMP>(data)),
      _faceData(LoadLump<tBSPFace, HL1_BSP_FACELUMP>(data)),
      _lightingData(LoadLump<byte, HL1_BSP_LIGHTINGLUMP>(data)),
      _clipnodeData(LoadLump<tBSPClipNode, HL1_BSP_CLIPNODELUMP>(data)),
      _leafData(LoadLump<tBSPLeaf, HL1_BSP_LEAFLUMP>(data)),
      _marksurfaceData(LoadLump<unsigned short, HL1_BSP_MARKSURFACELUMP>(data)),
      _edgeData(LoadLump<tBSPEdge, HL1_BSP_EDGELUMP>(data)),
      _surfedgeData(LoadLump<int, HL1_BSP_SURFEDGELUMP>(data)),
      _modelData(LoadLump<tBSPModel, HL1_BSP_MODELLUMP>(data))
{}

BspAsset::BspAsset(
    IFileSystem *fs)
    : Asset(fs)
{}

BspAsset::~BspAsset()
{}

bool BspAsset::Load(
    const std::string &filename)
{
    auto location = _fs->LocateFile(filename);

    if (location.empty())
    {
        spdlog::error("Unable to find {}", filename);

        return false;
    }

    auto fullpath = std::filesystem::path(location) / filename;

    std::vector<byte> data;

    if (!_fs->LoadFile(fullpath.string(), data))
    {
        return false;
    }

    _bspFile = std::make_unique<BspFile>(data.data());

    _entities = BspAsset::LoadEntities(_bspFile);

    //    _visLeafs = BspAsset::LoadVisLeafs(_bspFile);

    std::vector<WadAsset *> wads;
    _worldspawn = _entities.front();
    if (_worldspawn.classname != "worldspawn")
    {
        _worldspawn = *FindEntityByClassname("worldspawn");
    }

    wads = WadAsset::LoadWads(_worldspawn.keyvalues["wad"], _fs);

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
        std::vector<valve::byte> buffer;
        auto filenametga = fmt::format("gfx/env/{}{}.tga", sky, shortNames[i]);
        auto location = _fs->LocateFile(filenametga);

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
                spdlog::error("unable to find sky texture {} as either tga and bmp", shortNames[i]);
                return false;
            }
        }

        if (!_fs->LoadFile(fullPath.generic_string(), buffer))
        {
            spdlog::error("failed to load sky texture");
            return false;
        }

        int x, y, n;
        unsigned char *data = stbi_load_from_memory(buffer.data(), buffer.size(), &x, &y, &n, 0);
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
    std::unique_ptr<BspFile> &bspFile)
{
    const byte *itr = bspFile->_entityData.data();
    const byte *end = bspFile->_entityData.data() + bspFile->_entityData.size();

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
    std::unique_ptr<BspFile> &bspFile)
{
    std::vector<tBSPVisLeaf> visLeafs = std::vector<tBSPVisLeaf>(bspFile->_leafData.size());

    for (unsigned int i = 0; i < bspFile->_leafData.size(); i++)
    {
        visLeafs[i].leafs = 0;
        visLeafs[i].leafCount = 0;
        int visOffset = bspFile->_leafData[i].visofs;

        for (int j = 1; j < bspFile->_modelData[0].visLeafs; visOffset++)
        {
            if (bspFile->_visData[visOffset] == 0)
            {
                visOffset++;
                j += (bspFile->_visData[visOffset] << 3);
            }
            else
            {
                for (byte bit = 1; bit; bit <<= 1, j++)
                {
                    if (bspFile->_visData[visOffset] & bit)
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

    for (unsigned int i = 0; i < bspFile->_leafData.size(); i++)
    {
        int visOffset = bspFile->_leafData[i].visofs;
        int index = 0;
        for (int j = 1; j < bspFile->_modelData[0].visLeafs; visOffset++)
        {
            if (bspFile->_visData[visOffset] == 0)
            {
                visOffset++;
                j += (bspFile->_visData[visOffset] << 3);
            }
            else
            {
                for (byte bit = 1; bit; bit <<= 1, j++)
                {
                    if (bspFile->_visData[visOffset] & bit)
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
    tempLightmaps.resize(_bspFile->_faceData.size());

    Texture whiteTexture;
    unsigned char data[8 * 8 * 3];
    memset(data, 255, 8 * 8 * 3);
    whiteTexture.SetData(8, 8, 3, data);

    for (unsigned int f = 0; f < _bspFile->_faceData.size(); f++)
    {
        tBSPFace &in = _bspFile->_faceData[f];
        tBSPMipTexHeader *mip = GetMiptex(_bspFile->_texinfoData[in.texinfo].miptexIndex);
        tFace out;

        out.firstVertex = vertices.size();
        out.vertexCount = in.edgeCount;
        out.flags = _bspFile->_texinfoData[in.texinfo].flags;
        out.texture = _bspFile->_texinfoData[in.texinfo].miptexIndex;
        out.lightmap = f;
        out.plane = glm::vec4(
            _bspFile->_planes[in.planeIndex].normal[0],
            _bspFile->_planes[in.planeIndex].normal[1],
            _bspFile->_planes[in.planeIndex].normal[2],
            _bspFile->_planes[in.planeIndex].distance);

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
            int ei = _bspFile->_surfedgeData[in.firstEdge + e];
            // Determine the vertex based on the edge index
            v.position = _bspFile->_verticesData[_bspFile->_edgeData[ei < 0 ? -ei : ei].vertex[ei < 0 ? 1 : 0]].point;

            // Copy the normal from the plane
            v.normal = glm::vec3(out.plane);

            // Reset the bone so its not used
            v.bone = -1;

            tBSPTexInfo &ti = _bspFile->_texinfoData[in.texinfo];
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
    auto count = int(*_bspFile->_textureData.data());
    auto offsetPtr = (int *)(_bspFile->_textureData.data() + sizeof(int));
    std::vector<int> textureTable(offsetPtr, offsetPtr + count);

    for (int t = 0; t < int(*_bspFile->_textureData.data()); t++)
    {
        const unsigned char *textureData = _bspFile->_textureData.data() + textureTable[t];

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
    tBSPMipTexOffsetTable *bspMiptexTable = (tBSPMipTexOffsetTable *)_bspFile->_textureData.data();

    if (index >= 0 && bspMiptexTable->miptexCount > index)
    {
        return (tBSPMipTexHeader *)(_bspFile->_textureData.data() + bspMiptexTable->offsets[index]);
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

float dist(
    const tBSPPlane &plane,
    const glm::vec3 &point)
{
    //    if (plane.type < 3)
    //    {
    //        return point[plane.type] - plane.distance;
    //    }
    //    else
    {
        return glm::dot(plane.normal, point) - plane.distance;
    }
}

#define DIST_EPSILON (1.0f / 32.0f)
#define VectorLerp(v1, lerp, v2, c) ((c)[0] = (v1)[0] + (lerp) * ((v2)[0] - (v1)[0]), (c)[1] = (v1)[1] + (lerp) * ((v2)[1] - (v1)[1]), (c)[2] = (v1)[2] + (lerp) * ((v2)[2] - (v1)[2]))
glm::vec3 BspAsset::Trace(
    const glm::vec3 &from,
    const glm::vec3 &to,
    int clipNodeIndex)
{
    if (clipNodeIndex < 0)
    {
        clipNodeIndex = _bspFile->_modelData[0].headnode[0];
    }

    auto plane = _bspFile->_planes[_bspFile->_clipnodeData[clipNodeIndex].planeIndex];

    auto fromInFront = dist(plane, from);
    auto toInFront = dist(plane, to);

    auto nextClipNode = _bspFile->_clipnodeData[clipNodeIndex].children[(toInFront >= 0.0f) ? 0 : 1];
    if (nextClipNode < -1)
    {
        spdlog::debug("we are in the solid! {}", nextClipNode);
        float frac = fromInFront < 0.0f ? (fromInFront + DIST_EPSILON) / (fromInFront - toInFront) : (fromInFront - DIST_EPSILON) / (fromInFront - toInFront);

        glm::vec3 mid;
        VectorLerp(from, frac, to, mid);

        spdlog::debug("{} {} {}", glm::to_string(from), glm::to_string(mid), glm::to_string(to));
        return mid;
    }

    if (nextClipNode < 0)
    {
        return to;
    }

    return Trace(from, to, nextClipNode);
}

bool BspAsset::IsInContents(
    const glm::vec3 &from,
    const glm::vec3 &to,
    glm::vec3 &target,
    int clipNodeIndex)
{
    restartCount++;

    if (clipNodeIndex == CONTENTS_EMPTY)
    {
        target = to;

        return false;
    }

    spdlog::debug("    clipNode     : {}", clipNodeIndex);

    auto plane = _bspFile->_planes[_bspFile->_clipnodeData[clipNodeIndex].planeIndex];

    auto toInFront = dist(plane, to);
    auto fromInFront = dist(plane, from);

    if (toInFront >= 0 && fromInFront >= 0 && _bspFile->_clipnodeData[clipNodeIndex].children[0] >= CONTENTS_EMPTY)
    {
        return IsInContents(from, to, target, _bspFile->_clipnodeData[clipNodeIndex].children[0]);
    }

    if (toInFront < 0 && fromInFront < 0 && _bspFile->_clipnodeData[clipNodeIndex].children[1] >= CONTENTS_EMPTY)
    {
        return IsInContents(from, to, target, _bspFile->_clipnodeData[clipNodeIndex].children[1]);
    }

    auto nextClipNode = _bspFile->_clipnodeData[clipNodeIndex].children[(toInFront >= 0.0f) ? 0 : 1];

    if (nextClipNode == CONTENTS_EMPTY)
    {
        target = to;

        return false;
    }

    if (nextClipNode == CONTENTS_SOLID)
    {
        auto dir = toInFront;
        if (dir < 0)
            dir -= DIST_EPSILON;
        else
            dir += DIST_EPSILON;

        auto newTo = to + (plane.normal * -dir);

        if (newTo == to)
        {
            target = to;

            spdlog::debug("    {}, {}, {}", target.x, target.y, target.z);

            return false;
        }

        target = newTo;

        spdlog::debug("    jump {}", restartCount);
        spdlog::debug("    new to       : {}, {}, {}", newTo.x, newTo.y, newTo.z);
        spdlog::debug("    plane        : {}", _bspFile->_clipnodeData[clipNodeIndex].planeIndex);
        spdlog::debug("    plane normal : {}, {}, {} [{}]", plane.normal.x, plane.normal.y, plane.normal.z, plane.distance);
        spdlog::debug("    toInFront    : {}", toInFront);
        spdlog::debug("    fromInFront  : {}", fromInFront);

        return IsInContents(from, newTo, target, _bspFile->_modelData[0].headnode[0]);
    }

    return IsInContents(from, to, target, nextClipNode);
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
    const tBSPTexInfo *t = &_bspFile->_texinfoData[in.texinfo];

    min[0] = min[1] = 999999;
    max[0] = max[1] = -999999;

    for (int i = 0; i < in.edgeCount; i++)
    {
        const tBSPVertex *v;
        int e = _bspFile->_surfedgeData[in.firstEdge + i];

        if (e >= 0)
        {
            v = &_bspFile->_verticesData[_bspFile->_edgeData[e].vertex[0]];
        }
        else
        {
            v = &_bspFile->_verticesData[_bspFile->_edgeData[-e].vertex[1]];
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

    out.SetData(size[0] + 1, size[1] + 1, 3, _bspFile->_lightingData.data() + in.lightOffset, false);

    return true;
}

bool BspAsset::LoadModels()
{
    for (unsigned int m = 0; m < _bspFile->_modelData.size(); m++)
    {
        tModel model;

        model.position = _bspFile->_modelData[m].origin;
        model.firstFace = _bspFile->_modelData[m].firstFace;
        model.faceCount = _bspFile->_modelData[m].faceCount;

        _models.push_back(model);
    }

    return true;
}
