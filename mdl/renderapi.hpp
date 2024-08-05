#ifndef RENDERAPI_H
#define RENDERAPI_H

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <vector>

#define GLSL(src) "#version 150\n" #src

class GlShader
{
public:
    GlShader(int type, const char *source);

    ~GlShader();

    bool is_good() const;

private:
    GLuint _index = 0;

    friend class GlProgram;
};

class GlProgram
{
public:
    GlProgram();

    ~GlProgram();

    void attach(const GlShader &shader);

    void attach(GLuint index);

    void link();

    GLint getAttribLocation(const char *name) const;

    GLint getUniformLocation(const char *name) const;

    GLint getUniformBlockIndex(const char *name) const;

    void uniformBlockBinding(GLuint index, GLuint binding);

    void use() const;

    bool is_good() const;

//private:
    GLuint _index = 0;
};

class RenderApi
{
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec2 uv;
        int bone = 0;
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
    void Setup();

    void SetupBones(const float m[128][4][4], int count);

    void Render(const glm::mat4 &m);

    void Texture(unsigned int index);

    void BeginMesh();

    void EndMesh();

    void BeginFace(bool fan);

    void EndFace();

    void Position(const glm::vec3 &pos);

    void Uv(const glm::vec2 &uv);

    void Bone(int bone);

private:
    unsigned int _vbo;
    std::unique_ptr<GlProgram> _program;
    int _positionAttrib;
    int _colorAttrib;
    int _uvAttrib;
    int _boneAttrib;
    int _matrixUniform;
    int _textureUniform;
    int _bonesBlockUniform;
    unsigned int _bonesBuffer;

    unsigned int VertexSize() const { return sizeof(Vertex); }

    glm::vec2 _nextUv;
    int _nextBone;
    std::unique_ptr<Face> _currentFace = nullptr;
    std::unique_ptr<Mesh> _currentMesh = nullptr;

    std::vector<Vertex> _vertices;
    std::vector<std::unique_ptr<Face>> _faces;
    std::vector<std::unique_ptr<Mesh>> _meshes;
};

#endif // RENDERAPI_H
