#include "renderapi.hpp"

GlShader::GlShader(int type, const char *source)
    : _index(glCreateShader(type))
{
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

GlShader::~GlShader()
{
    if (is_good())
    {
        glDeleteShader(_index);

        _index = 0;
    }
}

bool GlShader::is_good() const
{
    return _index > 0;
}

GlProgram::GlProgram()
    : _index(glCreateProgram())
{}

GlProgram::~GlProgram()
{
    if (is_good())
    {
        glDeleteProgram(_index);
        _index = 0;
    }
}

void GlProgram::attach(const GlShader &shader)
{
    glAttachShader(_index, shader._index);
}

void GlProgram::attach(GLuint index)
{
    glAttachShader(_index, index);
}

void GlProgram::link()
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

GLint GlProgram::getAttribLocation(const char *name) const
{
    return glGetAttribLocation(_index, name);
}

GLint GlProgram::getUniformLocation(const char *name) const
{
    return glGetUniformLocation(_index, name);
}

GLint GlProgram::getUniformBlockIndex(const char *name) const
{
    return glGetUniformBlockIndex(_index, name);
}

void GlProgram::uniformBlockBinding(GLuint index, GLuint binding)
{
    glUniformBlockBinding(_index, index, binding);
}

void GlProgram::use() const
{
    glUseProgram(_index);
}

bool GlProgram::is_good() const
{
    return _index > 0;
}

void RenderApi::Setup()
{
    glGenBuffers(1, &_vbo);

    GlShader vs = GlShader(
        GL_VERTEX_SHADER,
        GLSL(
            in vec3 a_position;
            in vec2 a_uv;
            in uint a_bone;

            uniform mat4 u_matrix;

            layout(std140) uniform BonesBlock {
                mat4 u_bones[16];
            };

            out vec2 f_uv;

            void main() {
                mat4 m = u_matrix;
                gl_Position = m * vec4(a_position.xyz, 1.0);
                f_uv = a_uv;
            }));

    GlShader fs(
        GL_FRAGMENT_SHADER,
        GLSL(
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
    _uvAttrib = _program->getAttribLocation("a_uv");
    _boneAttrib = _program->getAttribLocation("a_bone");
    _matrixUniform = _program->getUniformLocation("u_matrix");
    _textureUniform = _program->getUniformLocation("u_tex0");

    int _u_bones = 0;
    GLint uniform_block_index = glGetUniformBlockIndex(_program->_index, "u_bones");
    std::cout << _program->_index << " " << uniform_block_index << " " << _u_bones << std::endl;
    glUniformBlockBinding(_program->_index, uniform_block_index, _u_bones);
    glGenBuffers(1, &_bonesBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, _bonesBuffer);
    glBufferData(GL_UNIFORM_BUFFER, 32 * sizeof(glm::mat4), 0, GL_STREAM_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static glm::mat4 mats[32];

void copy_to_mat4(glm::mat4 &m, const float matrix[3][4])
{
    for (int i = 0; i < 4; i++)
    {
        m[i] = glm::vec4(matrix[i][1], matrix[i][2], matrix[i][3], 0.0f);
    }
}

void RenderApi::SetupBones(const float m[128][4][4], int count)
{
    for (int i = 0; i < count && i < 32; i++)
    {
        copy_to_mat4(mats[i], m[i]);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, _bonesBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, count * sizeof(glm::mat4), glm::value_ptr(mats[0]));
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, _bonesBuffer, 0, count * sizeof(glm::mat4));
}

void RenderApi::Render(const glm::mat4 &m)
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

    glVertexAttribPointer(_uvAttrib, 2, GL_FLOAT, GL_FALSE, VertexSize(), (void *)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(_uvAttrib);

    glVertexAttribPointer(_boneAttrib, 1, GL_INT, GL_FALSE, VertexSize(), (void *)(sizeof(glm::vec3) + sizeof(glm::vec2)));
    glEnableVertexAttribArray(_boneAttrib);

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

void RenderApi::Texture(unsigned int index)
{
    if (_currentMesh == nullptr)
    {
        return;
    }

    _currentMesh->textureIndex = index;
}

void RenderApi::BeginMesh()
{
    if (_currentMesh != nullptr)
    {
        return;
    }

    _currentMesh = std::unique_ptr<Mesh>(new Mesh());
}

void RenderApi::EndMesh()
{
    if (_currentMesh == nullptr)
    {
        return;
    }

    _currentMesh->faceCount = _faces.size() - _currentMesh->firstFace;

    _meshes.push_back(std::move(_currentMesh));
    _currentMesh = nullptr;
}

void RenderApi::BeginFace(bool fan)
{
    if (_currentFace != nullptr)
    {
        return;
    }

    _currentFace = std::unique_ptr<Face>(new Face());
    _currentFace->firstVertex = _vertices.size();
    _currentFace->fan = fan;
}

void RenderApi::EndFace()
{
    if (_currentFace == nullptr)
    {
        return;
    }

    _currentFace->vertexCount = _vertices.size() - _currentFace->firstVertex;

    _faces.push_back(std::move(_currentFace));
    _currentFace = nullptr;
}

void RenderApi::Position(const glm::vec3 &pos)
{
    Vertex v = {pos, _nextUv, _nextBone};

    _vertices.push_back(v);
}

void RenderApi::Uv(const glm::vec2 &uv)
{
    _nextUv = uv;
}

void RenderApi::Bone(int bone)
{
    _nextBone = bone;
}
