#ifndef RENDERAPI_H
#define RENDERAPI_H

#include <glad/glad.h>

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <memory>
#include <vector>

#define GLSL(src) "#version 150\n" #src

template <int Type>
class GlShader
{
public:
    GlShader(const char *source)
    {
        _index = glCreateShader(Type);

        glShaderSource(_index, 1, &source, NULL);
        glCompileShader(_index);

        GLint result = GL_FALSE;
        GLint logLength;

        glGetShaderiv(_index, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE)
        {
            glGetShaderiv(_index, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> error(static_cast<size_t>((logLength > 1) ? logLength : 1));
            glGetShaderInfoLog(_index, logLength, NULL, &error[0]);

            std::cerr << "tried compiling shader\n\t" << source << "\n\n"
                      << "got error\n\t" << error.data() << std::endl;

            glDeleteShader(_index);

            _index = 0;
        }
    }

    ~GlShader()
    {
        if (is_good())
        {
            glDeleteShader(_index);

            _index = 0;
        }
    }

    bool is_good() const { return _index > 0; }

private:
    GLuint _index = 0;

    friend class GlProgram;
};

class GlProgram
{
public:
    GlProgram()
    {
        _index = glCreateProgram();
    }

    ~GlProgram()
    {
        if (is_good())
        {
            glDeleteProgram(_index);
            _index = 0;
        }
    }

    template <int Type>
    void attach(const GlShader<Type> &shader)
    {
        glAttachShader(_index, shader._index);
    }

    void link()
    {
        glLinkProgram(_index);

        GLint result = GL_FALSE;
        GLint logLength;

        glGetProgramiv(_index, GL_LINK_STATUS, &result);
        if (result == GL_FALSE)
        {
            glGetProgramiv(_index, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> error(static_cast<size_t>((logLength > 1) ? logLength : 1));
            glGetProgramInfoLog(_index, logLength, NULL, &error[0]);

            std::cerr << "tried linking program, got error:\n"
                      << error.data();
        }
    }

    GLint getAttribLocation(const char *name) const { return glGetAttribLocation(_index, name); }
    GLint getUniformLocation(const char *name) const { return glGetUniformLocation(_index, name); }

    void use() const { glUseProgram(_index); }

    bool is_good() const { return _index > 0; }

private:
    GLuint _index = 0;
};

template <typename TPosition, typename TColor, typename TUv>
class RenderApi
{
    struct Vertex
    {
        TPosition pos;
        TColor color;
        TUv uv;
    };

    struct Face
    {
        size_t firstVertex;
        size_t vertexCount;
        bool fan = true;
    };

    struct Mesh
    {
        unsigned int textureIndex;
        size_t firstFace;
        size_t faceCount;
    };

public:
    void Setup()
    {
        glGenBuffers(1, &_vbo);

        GlShader<GL_VERTEX_SHADER> vs(GLSL(
            in vec3 a_position;
            in vec4 a_color;
            in vec2 a_uv;

            uniform mat4 u_matrix;

            out vec2 f_uv;

            void main() {
                gl_Position = u_matrix * vec4(a_position.xyz, 1.0);
                f_uv = a_uv;
            }));

        GlShader<GL_FRAGMENT_SHADER> fs(GLSL(
            uniform sampler2D u_tex0;

            in vec2 f_uv;

            out vec4 color;

            void main() {
                vec4 texel0;
                texel0 = texture2D(u_tex0, f_uv);
                color = vec4(texel0.rgb, 1.0);
            }));

        _program = std::unique_ptr<GlProgram>(new GlProgram());

        _program->attach(vs);
        _program->attach(fs);
        _program->link();

        _positionAttrib = _program->getAttribLocation("a_position");
        _colorAttrib = _program->getAttribLocation("a_color");
        _uvAttrib = _program->getAttribLocation("a_uv");
        _matrixUniform = _program->getUniformLocation("u_matrix");
        _textureUniform = _program->getUniformLocation("u_tex0");

        glUniform1i(_textureUniform, 0);
    }

    void Render(const glm::mat4 &m)
    {
        if (_vertices.empty())
        {
            return;
        }

        _program->use();

        glBindBuffer(GL_ARRAY_BUFFER, _vbo);

        glBufferData(
            GL_ARRAY_BUFFER,
            GLsizeiptr(_vertices.size() * VertexSize()),
            0,
            GL_STATIC_DRAW);

        glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            GLsizeiptr(_vertices.size() * VertexSize()),
            reinterpret_cast<const GLvoid *>(&_vertices[0]));

        glVertexAttribPointer(_positionAttrib, 3, GL_FLOAT, GL_FALSE, VertexSize(), (void *)0);
        glEnableVertexAttribArray(_positionAttrib);

        glVertexAttribPointer(_colorAttrib, 4, GL_FLOAT, GL_FALSE, VertexSize(), (void *)sizeof(TPosition));
        glEnableVertexAttribArray(_colorAttrib);

        glVertexAttribPointer(_uvAttrib, 2, GL_FLOAT, GL_FALSE, VertexSize(), (void *)(sizeof(TPosition) + sizeof(TColor)));
        glEnableVertexAttribArray(_uvAttrib);

        glUniformMatrix4fv(_matrixUniform, 1, false, glm::value_ptr(m));

        for (auto &mesh : _meshes)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh->textureIndex);

            for (auto &face : _faces)
            {
                if (face->fan)
                {
                    glDrawArrays(GL_TRIANGLE_FAN, face->firstVertex, face->vertexCount);
                }
                else
                {
                    glDrawArrays(GL_TRIANGLE_STRIP, face->firstVertex, face->vertexCount);
                }
            }
        }

        _vertices.clear();
        _meshes.clear();
        _faces.clear();
    }

    void Texture(unsigned int index)
    {
        if (_currentMesh == nullptr)
        {
            return;
        }

        _currentMesh->textureIndex = index;
    }

    void BeginMesh()
    {
        if (_currentMesh != nullptr)
        {
            return;
        }

        _currentMesh = std::unique_ptr<Mesh>(new Mesh());
    }

    void EndMesh()
    {
        if (_currentMesh == nullptr)
        {
            return;
        }

        _currentMesh->faceCount = _faces.size() - _currentMesh->firstFace;

        _meshes.push_back(std::move(_currentMesh));
        _currentMesh = nullptr;
    }

    void BeginFace(bool fan)
    {
        if (_currentFace != nullptr)
        {
            return;
        }

        _currentFace = std::unique_ptr<Face>(new Face());
        _currentFace->firstVertex = _vertices.size();
        _currentFace->fan = fan;
    }

    void EndFace()
    {
        if (_currentFace == nullptr)
        {
            return;
        }

        _currentFace->vertexCount = _vertices.size() - _currentFace->firstVertex;

        _faces.push_back(std::move(_currentFace));
        _currentFace = nullptr;
    }

    void Position(const TPosition &pos)
    {
        Vertex v = {pos, _nextColor, _nextUv};

        _vertices.push_back(v);
    };

    void Color(const TColor &color)
    {
        _nextColor = color;
    };

    void Uv(const TUv &uv)
    {
        _nextUv = uv;
    };

private:
    unsigned int _vbo;
    std::unique_ptr<GlProgram> _program;
    int _positionAttrib;
    int _colorAttrib;
    int _uvAttrib;
    int _matrixUniform;
    int _textureUniform;

    unsigned int VertexSize() const { return sizeof(Vertex); }

    TColor _nextColor;
    TUv _nextUv;
    std::unique_ptr<Face> _currentFace = nullptr;
    std::unique_ptr<Mesh> _currentMesh = nullptr;

    std::vector<Vertex> _vertices;
    std::vector<std::unique_ptr<Face>> _faces;
    std::vector<std::unique_ptr<Mesh>> _meshes;
};

#endif // RENDERAPI_H
