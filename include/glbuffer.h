#ifndef GLBUFFER_H
#define GLBUFFER_H

#include "glad/glad.h"
#include "glshader.h"
#include <glm/glm.hpp>
#include <map>
#include <vector>

class VertexType
{
public:
    glm::vec3 pos;
    glm::vec4 uvs;
};

class BufferType
{
public:
    BufferType() = default;

    virtual ~BufferType() = default;

    std::vector<VertexType> &verts()
    {
        return _verts;
    }

    BufferType &operator<<(
        VertexType const &vertex)
    {
        _verts.push_back(vertex);
        _vertexCount = static_cast<GLsizei>(_verts.size());

        return *this;
    }

    int vertexCount() const
    {
        return _vertexCount;
    }

    BufferType &vertex(
        glm::vec3 const &position)
    {
        VertexType v;
        v.pos = position;
        v.uvs = _nextUvs;
        _verts.push_back(v);

        _vertexCount = static_cast<GLsizei>(_verts.size());

        return *this;
    }

    BufferType &uvs(
        glm::vec4 const &uvs)
    {
        _nextUvs = uvs;

        return *this;
    }

    bool setup(
        ShaderType &shader)
    {
        _vertexCount = static_cast<GLsizei>(_verts.size());

        glGenVertexArrays(1, &_vertexArrayId);
        glGenBuffers(1, &_vertexBufferId);

        glBindVertexArray(_vertexArrayId);
        glBindBuffer(GL_ARRAY_BUFFER, _vertexBufferId);

        glBufferData(
            GL_ARRAY_BUFFER,
            GLsizeiptr(_verts.size() * sizeof(VertexType)),
            0,
            GL_STATIC_DRAW);

        glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            GLsizeiptr(_verts.size() * sizeof(VertexType)),
            reinterpret_cast<const GLvoid *>(&_verts[0]));

        shader.setupAttributes(sizeof(VertexType));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        _verts.clear();

        return true;
    }

    void bind()
    {
        glBindVertexArray(_vertexArrayId);
    }

    void unbind()
    {
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void cleanup()
    {
        if (_vertexBufferId != 0)
        {
            glDeleteBuffers(1, &_vertexBufferId);
            _vertexBufferId = 0;
        }
        if (_vertexArrayId != 0)
        {
            glDeleteVertexArrays(1, &_vertexArrayId);
            _vertexArrayId = 0;
        }
    }

private:
    int _vertexCount = 0;
    std::vector<VertexType> _verts;
    glm::vec4 _nextUvs;
    unsigned int _vertexArrayId = 0;
    unsigned int _vertexBufferId = 0;
};

#endif // GLBUFFER_H
