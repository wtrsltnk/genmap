#ifndef _HL1BSPASSET_H_
#define _HL1BSPASSET_H_

#include "hl1bsptypes.h"
#include "hl1wadasset.h"
#include "hltexture.h"

#include <string>

namespace valve
{

    namespace hl1
    {

        class BspFile
        {
        public:
            BspFile(byte *data);

            std::vector<byte> _entityData;
            std::vector<tBSPPlane> _planes;
            std::vector<unsigned char> _textureData;
            std::vector<tBSPVertex> _verticesData;
            std::vector<byte> _visData;
            std::vector<tBSPNode> _nodeData;
            std::vector<tBSPTexInfo> _texinfoData;
            std::vector<tBSPFace> _faceData;
            std::vector<byte> _lightingData;
            std::vector<tBSPClipNode> _clipnodeData;
            std::vector<tBSPLeaf> _leafData;
            std::vector<unsigned short> _marksurfaceData;
            std::vector<tBSPEdge> _edgeData;
            std::vector<int> _surfedgeData;
            std::vector<tBSPModel> _modelData;

        private:
            template <class T, int L>
            std::vector<T> LoadLump(byte *data)
            {
                auto header = reinterpret_cast<tBSPHeader *>(data);

                auto offsetPtr = data + header->lumps[L].offset;
                auto typePtr = reinterpret_cast<T *>(offsetPtr);

                return std::vector<T>(typePtr, typePtr + (header->lumps[L].size / sizeof(T)));
            }
        };

        class BspAsset :
            public Asset
        {
        public:
            typedef struct sModel
            {
                glm::vec3 position;
                int firstFace;
                int faceCount;

                int rendermode;        // "Render Mode" [ 0: "Normal" 1: "Color" 2: "Texture" 3: "Glow" 4: "Solid" 5: "Additive" ]
                char renderamt;        // "FX Amount (1 - 255)"
                glm::vec4 rendercolor; // "FX Color (R G B)"

            } tModel;

        public:
            BspAsset(
                IFileSystem *fs);
            virtual ~BspAsset();

            virtual bool Load(
                const std::string &filename);

            tBSPEntity *FindEntityByClassname(
                const std::string &classname);

            tBSPMipTexHeader *GetMiptex(
                int index);

            int FaceFlags(
                size_t index);

            glm::vec3 Trace(
                const glm::vec3 &from,
                const glm::vec3 &to,
                int clipNodeIndex = -1);

            int restartCount = 0;
            bool IsInContents(
                const glm::vec3 &from,
                const glm::vec3 &to,
                glm::vec3 &target,
                int clipNodeIndex);

            // These are mapped from the input file data
            std::unique_ptr<BspFile> _bspFile;
            tBSPEntity _worldspawn;

            // These are parsed from the mapped data
            std::vector<tBSPEntity> _entities;
            std::vector<tBSPVisLeaf> _visLeafs;
            std::vector<tModel> _models;
            std::vector<Texture *> _textures;
            std::vector<Texture *> _lightMaps;
            std::vector<tVertex> _vertices;
            std::vector<tFace> _faces;
            valve::Texture *_skytextures[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

        private:
            void CalculateSurfaceExtents(
                const tBSPFace &in,
                float min[2],
                float max[2]) const;

            bool LoadLightmap(
                const tBSPFace &in,
                Texture &out,
                float min[2],
                float max[2]);

            bool LoadFacesWithLightmaps(
                std::vector<tFace> &faces,
                std::vector<Texture *> &lightmaps,
                std::vector<tVertex> &vertices);

            bool LoadSkyTextures();

            bool LoadTextures(
                std::vector<Texture *> &textures,
                const std::vector<WadAsset *> &wads);

            bool LoadModels();

            static std::vector<sBSPEntity> LoadEntities(
                std::unique_ptr<BspFile> &bspFile);

            static std::vector<tBSPVisLeaf> LoadVisLeafs(
                std::unique_ptr<BspFile> &bspFile);
        };

    } // namespace hl1

} // namespace valve

#endif /* _HL1BSPASSET_H_ */
