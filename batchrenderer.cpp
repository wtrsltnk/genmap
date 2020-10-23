#include "batchrenderer.h"

#include <cstring>
#include <glad/glad.h>

using namespace genmap::util;

AbstractBatchRenderer::AbstractBatchRenderer(
    int maxVertexCount,
    int vertexSize)
    : _maxVertexCount(maxVertexCount),
      _vertexSize(vertexSize),
      _maxTextureCount(32),
      _data(nullptr),
      _cursor(nullptr),
      _vao(0),
      _vbo(0)
{
    auto realVertexSize =
        _vertexSize    // original size
        + sizeof(int)  // texture index
        + sizeof(int); // lightmap index

    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);

    glGenBuffers(1, &_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, realVertexSize * maxVertexCount, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);
}

AbstractBatchRenderer::~AbstractBatchRenderer() = default;

void AbstractBatchRenderer::CheckFlush(
    unsigned int size,
    unsigned int texture,
    unsigned int lightmap)
{
    (void)texture;
    (void)lightmap;

    if ((_cursor - _data) + size > (_vertexSize * _maxVertexCount))
    {
        Flush();
        return;
    }
}

void AbstractBatchRenderer::Begin(
    const glm::mat4 &projection)
{
    (void)projection;

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    _data = (unsigned char *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    _cursor = _data;
}

void AbstractBatchRenderer::AddTriangle(
    const unsigned char *data,
    unsigned int size,
    unsigned int texture,
    unsigned int lightmap)
{
    if (size % _vertexSize != 0)
    {
        return;
    }

    CheckFlush(size, texture, lightmap);

    int textureIndex = 0;
    int lightmapIndex = 0;

    auto inCursor = data;
    while (size > 0)
    {
        std::memcpy(_cursor, inCursor, _vertexSize);
        _cursor += size;
        size -= _vertexSize;
        inCursor += _vertexSize;

        std::memcpy(_cursor, reinterpret_cast<unsigned char *>(&textureIndex), sizeof(int));
        _cursor += sizeof(int);
        std::memcpy(_cursor, reinterpret_cast<unsigned char *>(&lightmapIndex), sizeof(int));
        _cursor += sizeof(int);
    }
}

void AbstractBatchRenderer::Flush()
{
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(_vao);

    glDrawArrays(GL_TRIANGLES, 0, _maxVertexCount / 3);

    glBindVertexArray(0);

    _cursor = nullptr;
    _usedTextures.clear();
}
