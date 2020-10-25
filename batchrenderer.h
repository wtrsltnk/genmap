#ifndef _BATCHRENDERER_H_
#define _BATCHRENDERER_H_

#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace genmap
{

    namespace util
    {
        class AbstractBatchRenderer
        {
        protected:
            unsigned int _maxVertexCount;
            unsigned int _vertexSize;
            unsigned int _maxTextureCount;
            unsigned char *_data;
            unsigned char *_cursor;
            std::map<unsigned int, int> _usedTextures;
            unsigned int _vao;
            unsigned int _vbo;

            void CheckFlush(
                unsigned int size,
                unsigned int texture,
                unsigned int lightmap);

        public:
            AbstractBatchRenderer(
                int maxVertexCount,
                int vertexSize);

            virtual ~AbstractBatchRenderer();

            inline int MaxVertexCount() const { return _maxVertexCount; }

            void Begin(
                const glm::mat4 &projection);

            void AddTriangle(
                const unsigned char *data,
                unsigned int size,
                unsigned int texture,
                unsigned int lightmap);

            void Flush();
        };

        template <typename TVertex>
        class BatchRenderer :
            public AbstractBatchRenderer
        {
        public:
            BatchRenderer()
                : AbstractBatchRenderer(1000, sizeof(TVertex))
            {}

            BatchRenderer(
                int maxVertexCount)
                : AbstractBatchRenderer(maxVertexCount, sizeof(TVertex))
            {}

            virtual ~BatchRenderer() = default;

            void AddTriangle(
                const std::vector<TVertex> &vertices,
                unsigned int texture,
                unsigned int lightmap)
            {
                AddTriangle(
                    reinterpret_cast<const void *>(vertices.data()),
                    _vertexSize * vertices.size(),
                    texture,
                    lightmap);
            }

            void AddTriangleFan(
                const std::vector<TVertex> &vertices,
                unsigned int texture,
                unsigned int lightmap)
            {
                for (unsigned int i = 2; i < vertices.size(); i++)
                {
                    std::vector<TVertex> triangle(3);

                    triangle.push_back(vertices[0]);
                    triangle.push_back(vertices[i - 1]);
                    triangle.push_back(vertices[i]);

                    AddTriangle(triangle, texture, lightmap);
                }
            }
        };

    } // namespace util

} // namespace genmap

#endif // _BATCHRENDERER_H_
