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
    glm::vec4 col;
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

    void setDrawMode(
        GLenum mode)
    {
        _drawMode = mode;
    }

    void addFace(
        int start,
        int count)
    {
        _faces.insert(std::make_pair(start, count));
    }

    int vertexCount() const
    {
        return _vertexCount;
    }

    int faceCount() const
    {
        return static_cast<int>(_faces.size());
    }

    BufferType &vertex(
        glm::vec3 const &position)
    {
        _verts.push_back(VertexType({position, _nextColor}));

        _vertexCount = static_cast<GLsizei>(_verts.size());

        return *this;
    }

    BufferType &color(
        glm::vec4 const &color)
    {
        _nextColor = color;

        return *this;
    }

    bool setup(
        ShaderType &shader)
    {
        return setup(_drawMode, shader);
    }

    bool setup(
        GLenum mode,
        ShaderType &shader)
    {
        _drawMode = mode;
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

        shader.setupAttributes();

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        _verts.clear();

        return true;
    }

    void render()
    {
        glBindVertexArray(_vertexArrayId);
        if (_faces.empty())
        {
            glDrawArrays(_drawMode, 0, _vertexCount);
        }
        else
        {
            for (auto pair : _faces)
            {
                glDrawArrays(_drawMode, pair.first, pair.second);
            }
        }
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
    glm::vec4 _nextColor;
    unsigned int _vertexArrayId = 0;
    unsigned int _vertexBufferId = 0;
    GLenum _drawMode = GL_TRIANGLES;
    std::map<int, int> _faces;
};

#endif // GLBUFFER_H
