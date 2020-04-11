#include "hl1bspasset.h"

#include <iostream>
#include <sstream>
#include "hl1bsptypes.h"
#include "stb_rect_pack.h"

using namespace valve::hl1;

BspAsset::BspAsset(IFileSystem *fs)
    : Asset(fs)
{ }

BspAsset::~BspAsset()
{ }

bool BspAsset::Load(const std::string &filename)
{
    _fs->LoadFile(filename, data);
    _header = (tBSPHeader*)data.data;

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
        _entities = BspAsset::LoadEntities(entityData);

    Array<byte> visibilityData;
    if (LoadLump(data, visibilityData, HL1_BSP_VISIBILITYLUMP))
        _visLeafs = BspAsset::LoadVisLeafs(visibilityData, _leafData, _modelData);

    std::vector<WadAsset*> wads;
    tBSPEntity* worldspawn = FindEntityByClassname("worldspawn");
    if (worldspawn != nullptr)
    {
        wads = WadAsset::LoadWads(worldspawn->keyvalues["wad"], filename, _fs);
    }
    LoadTextures(_textures, wads);
    WadAsset::UnloadWads(wads);

    LoadFacesWithLightmaps(_faces, _lightMaps, _vertices);

    LoadModels();

    return true;
}

std::vector<sBSPEntity> BspAsset::LoadEntities(const Array<byte>& entityData)
{
    const byte* itr = entityData.data;
    const byte* end = entityData.data + entityData.count;

    std::string key, value;
    std::vector<tBSPEntity> entities;
    while (itr[0] == '{')
    {
        tBSPEntity entity;
        itr++; // skip the bracket
        while (itr[0] <= ' ' && itr != end) itr++; // skip all space characters
        while (itr[0] != '}')
        {
            key = "";
            value = "";

            itr++; // skip the quote
            while (itr[0] != '\"' && itr != end) key += (*itr++);

            itr++; // skip the quote
            while (itr[0] <= ' ' && itr != end) itr++; // skip all space characters

            itr++; // skip the quote
            while (itr[0] != '\"' && itr != end) value += (*itr++);

            if (key == "classname") entity.classname = value;
            entity.keyvalues.insert(std::make_pair(key, value));

            itr++; // skip the quote
            while (itr[0] <= ' ' && itr != end) itr++; // skip all space characters
        }
        entities.push_back(entity);
        while (itr[0] != '{' && itr != end) itr++; // skip to the next entity
    }
    return entities;
}

tBSPEntity* BspAsset::FindEntityByClassname(const std::string& classname)
{
    for (std::vector<tBSPEntity>::iterator i = _entities.begin(); i != _entities.end(); ++i)
    {
        tBSPEntity* e = &(*i);
        if (e->classname == classname)
            return e;
    }

    return nullptr;
}

std::vector<tBSPVisLeaf> BspAsset::LoadVisLeafs(const Array<byte>& visdata, const Array<tBSPLeaf>& leafs, const Array<tBSPModel>& models)
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
                j += (visdata[visOffset]<<3);
            }
            else
            {
                for (byte bit = 1; bit; bit<<=1, j++)
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
                j += (visdata[visOffset]<<3);
            }
            else
            {
                for (byte bit = 1; bit; bit<<=1, j++)
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

bool BspAsset::LoadFacesWithLightmaps(std::vector<tFace>& faces, std::vector<Texture*>& lightmaps, std::vector<tVertex>& vertices)
{
    // Temporary lightmap array for each face, these will be packed into an atlas later
    Array<Texture> lightMaps;

    // Allocate the arrays for faces and lightmaps
    lightMaps.Allocate(_faceData.count);

    std::vector<stbrp_rect> rects;
    for (int f = 0; f < _faceData.count; f++)
    {
        tBSPFace& in = _faceData[f];
        tBSPMipTexHeader* mip = GetMiptex(_texinfoData[in.texinfo].miptexIndex);
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
                    _planeData[in.planeIndex].distance
                );

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

        // Skip the lightmaps for faces with special flags
        if (out.flags == 0)
        {
            if (LoadLightmap(in, lightMaps[f], min, max))
            {
                stbrp_rect rect;
                rect.id = f;
                rect.w = lightMaps[f].Width() + 2;
                rect.h = lightMaps[f].Height() + 2;
                rects.push_back(rect);
            }
        }

        float lw = float(lightMaps[f].Width());
        float lh = float(lightMaps[f].Height());
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
            v.normal = glm::to_vec3(out.plane);

            // Reset the bone so its not used
            v.bone = -1;

            tBSPTexInfo& ti = _texinfoData[in.texinfo];
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

    // Pack the lightmaps into an atlas
    while (rects.size() > 0)
    {
        // Setup one atlas texture (for now)
        Texture* atlas = new Texture();
        atlas->SetDimentions(512, 512, 3);
        atlas->Fill(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

        // pack the lightmap rects into one atlas
        stbrp_context context = { 0 };
        Array<stbrp_node> nodes(rects.size());
        stbrp_init_target(&context, atlas->Width(), atlas->Height(), nodes, rects.size());
        stbrp_pack_rects(&context, (stbrp_rect*)&rects[0], rects.size());

        std::vector<stbrp_rect> nextrects;
        for (auto rect = rects.begin(); rect != rects.end(); rect++)
        {
            // a reference to the loaded lightmapfrom the rect
            Texture& lm = lightMaps[faces[(*rect).id].lightmap];
            if ((*rect).was_packed)
            {
                // Copy the lightmap texture into the atlas
                atlas->FillAtPosition(lm, glm::vec2((*rect).x + 1, (*rect).y + 1), true);
                for (int vertexIndex = 0; vertexIndex < faces[(*rect).id].vertexCount; vertexIndex++)
                {
                    // Recalculate the lightmap texcoords for the atlas
                    glm::vec2& vec = vertices[faces[(*rect).id].firstVertex + vertexIndex].texcoords[1];
                    vec.s = float((*rect).x + 1 + (float(lm.Width()) * vec.s)) / atlas->Width();
                    vec.t = float((*rect).y + 1 + (float(lm.Height()) * vec.t)) / atlas->Height();
                }

                // Make sure we will use the (correct) atlas when we render
                faces[(*rect).id].lightmap = lightmaps.size();
            }
            else
            {
                // When the lightmap was not packed into the atlas, we try for a next atlas
                nextrects.push_back(*rect);
            }
        }

        // upload the atlas with all its lightmap textures
        lightmaps.push_back(atlas);
        rects = nextrects;
    }

    // cleanup the temporary lightmap array
    lightMaps.Delete();

    return true;
}

bool BspAsset::LoadTextures(std::vector<Texture*>& textures, const std::vector<WadAsset*>& wads)
{
    Array<int> textureTable(int(*_textureData.data), (int*)(_textureData.data + sizeof(int)));

    for (int t = 0; t < int(*_textureData.data); t++)
    {
        const unsigned char* textureData = _textureData.data + textureTable[t];
        tBSPMipTexHeader* miptex = (tBSPMipTexHeader*)textureData;
        Texture* tex = new Texture();
        tex->SetName(miptex->name);

        if (miptex->offsets[0] == 0)
        {
            for (std::vector<WadAsset*>::const_iterator i = wads.cbegin(); i != wads.cend(); ++i)
            {
                WadAsset* wad = *i;
                textureData = wad->LumpData(wad->IndexOf(miptex->name));
                if (textureData != nullptr)
                    break;
            }
        }

        if (textureData != nullptr)
        {
            miptex = (tBSPMipTexHeader*)textureData;
            int s = miptex->width * miptex->height;
            int bpp = 4;
            int paletteOffset = miptex->offsets[0] + s + (s/4) + (s/16) + (s/64) + sizeof(short);

            // Get the miptex data and palette
            const unsigned char* source0 = textureData + miptex->offsets[0];
            const unsigned char* palette = textureData + paletteOffset;

            unsigned char* destination = new unsigned char[s * bpp];

            unsigned char r, g, b, a;
            for (int i = 0; i < s; i++)
            {
                r = palette[source0[i]*3];
                g = palette[source0[i]*3+1];
                b = palette[source0[i]*3+2];
                a = 255;

                // Do we need a transparent pixel
                if (tex->Name()[0] == '{' && source0[i] == 255)
                    r = g = b = a = 0;

                destination[i*4 + 0] = r;
                destination[i*4 + 1] = g;
                destination[i*4 + 2] = b;
                destination[i*4 + 3] = a;
            }

            tex->SetData(miptex->width, miptex->height, bpp, destination);

            delete []destination;
        }
        else
        {
            std::cout << "Texture \"" << miptex->name << "\" not found" << std::endl;
            tex->DefaultTexture();
        }
        textures.push_back(tex);
    }

    return true;
}

tBSPMipTexHeader* BspAsset::GetMiptex(int index)
{
    tBSPMipTexOffsetTable* bspMiptexTable = (tBSPMipTexOffsetTable*)_textureData.data;

    if (index >= 0 && bspMiptexTable->miptexCount > index)
        return (tBSPMipTexHeader*)(_textureData.data + bspMiptexTable->offsets[index]);

    return 0;
}

int BspAsset::FaceFlags(int index)
{
    if (index >= 0 && _faces.size() > index)
        return _faces[index].flags;

    return -1;
}

//
// the following computations are based on:
// PolyEngine (c) Alexey Goloshubin and Quake I source by id Software
//

void BspAsset::CalculateSurfaceExtents(const tBSPFace& in, float min[2], float max[2]) const
{
    const tBSPTexInfo* t = &_texinfoData[in.texinfo];

    min[0] = min[1] =  999999;
    max[0] = max[1] = -999999;

    for (int i = 0; i < in.edgeCount; i++)
    {
        const tBSPVertex* v;
        int e = _surfedgeData[in.firstEdge + i];
        if (e >= 0)
            v = &_verticesData[_edgeData[e].vertex[0]];
        else
            v = &_verticesData[_edgeData[-e].vertex[1]];

        for (int j = 0; j < 2; j++)
        {
            int val = v->point[0] * t->vecs[j][0] + v->point[1] * t->vecs[j][1] + v->point[2] * t->vecs[j][2] +t->vecs[j][3];
            if (val < min[j]) min[j] = val;
            if (val > max[j]) max[j] = val;
        }
    }
}

bool BspAsset::LoadLightmap(const tBSPFace& in, Texture& out, float min[2], float max[2])
{
    // compute lightmap size
    int size[2];
    for (int c = 0; c < 2; c++)
    {
        float tmin = floorf(min[c] / 16.0f);
        float tmax = ceilf(max[c] / 16.0f);

        size[c] = (int) (tmax - tmin);
    }

    out.SetData(size[0] + 1, size[1] + 1, 3, _lightingData.data + in.lightOffset, false);

    return true;
}

bool BspAsset::LoadModels()
{
    _models.Allocate(_modelData.count);

    for (int m = 0; m < _modelData.count; m++)
    {
        _models[m].position = _modelData[m].origin;
        _models[m].firstFace = _modelData[m].firstFace;
        _models[m].faceCount = _modelData[m].faceCount;
    }

    return true;
}
