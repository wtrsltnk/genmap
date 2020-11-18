
#define APPLICATION_IMPLEMENTATION
#include "include/application.h"

#include "hl1bspasset.h"
#include "include/glshader.h"

#include "include/glbuffer.h"

//#define GLMATH_IMPLEMENTATION
//#include <glmath.h>

#include "camera.h"
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <spdlog/spdlog.h>

class FileSystem :
    public valve::IFileSystem
{
public:
    virtual std::string LocateFile(
        const std::string &relativeFilename);

    virtual bool LoadFile(
        const std::string &filename,
        valve::Array<valve::byte> &data);
};

std::string FileSystem::LocateFile(
    const std::string &relativeFilename)
{
    return relativeFilename;
}

bool FileSystem::LoadFile(
    const std::string &filename,
    valve::Array<valve::byte> &data)
{
    std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);

    if (!file.is_open())
    {
        spdlog::error("File not found: {0}", filename);

        return false;
    }

    auto count = file.tellg();

    data.Allocate(count);
    file.seekg(0, std::ios::beg);
    file.read((char *)data.data, data.count);

    file.close();

    return true;
}

unsigned int UploadToGl(
    valve::Texture *texture)
{
    GLuint format = GL_RGB;
    GLuint glIndex = 0;

    switch (texture->Bpp())
    {
        case 3:
            format = GL_RGB;
            break;
        case 4:
            format = GL_RGBA;
            break;
    }

    glGenTextures(1, &glIndex);
    glBindTexture(GL_TEXTURE_2D, glIndex);

    if (texture->Repeat())
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    //    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D, 0, format, texture->Width(), texture->Height(), 0, format, GL_UNSIGNED_BYTE, texture->Data());
    glGenerateMipmap(GL_TEXTURE_2D);

    return glIndex;
}

void OpenGLMessageCallback(
    unsigned source,
    unsigned type,
    unsigned id,
    unsigned severity,
    int length,
    const char *message,
    const void *userParam)
{
    (void)source;
    (void)type;
    (void)id;
    (void)length;
    (void)userParam;

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
            spdlog::critical("{} - {}", source, message);
            return;
        case GL_DEBUG_SEVERITY_MEDIUM:
            spdlog::error("{} - {}", source, message);
            return;
        case GL_DEBUG_SEVERITY_LOW:
            spdlog::warn("{} - {}", source, message);
            return;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            spdlog::trace("{} - {}", source, message);
            return;
    }

    spdlog::debug("Unknown severity level!");
    spdlog::debug(message);
}

class FaceType
{
public:
    GLuint firstVertex;
    GLuint vertexCount;
    GLuint textureIndex;
    GLuint lightmapIndex;
};

class Test
{
public:
    bool Startup()
    {
        spdlog::debug("Startup()");

        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OpenGLMessageCallback, nullptr);

        glDebugMessageControl(
            GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION,
            0,
            NULL,
            GL_FALSE);

        glClearColor(0.0f, 0.8f, 1.0f, 1.0f);

        shader.compileDefaultShader();

        FileSystem fs;
        bspAsset = new valve::hl1::BspAsset(&fs);
        if (!bspAsset->Load(filename))
        {
            delete bspAsset;
            bspAsset = nullptr;

            vertexBuffer
                .vertex(glm::vec3(-10.0f, -10.0f, 0.0f)) // mint
                .vertex(glm::vec3(-10.0f, 10.0f, 0.0f))  // geel
                .vertex(glm::vec3(10.0f, 10.0f, 0.0f))   // paars
                .vertex(glm::vec3(10.0f, -10.0f, 0.0f)); // wit

            vertexBuffer
                .setup(shader);
        }
        else
        {
            _lightmapIndices = std::vector<GLuint>(bspAsset->_lightMaps.size());
            glActiveTexture(GL_TEXTURE1);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            for (size_t i = 0; i < bspAsset->_lightMaps.size(); i++)
            {
                _lightmapIndices[i] = UploadToGl(bspAsset->_lightMaps[i]);
            }

            _textureIndices = std::vector<GLuint>(bspAsset->_textures.size());
            glActiveTexture(GL_TEXTURE0);
            for (size_t i = 0; i < bspAsset->_textures.size(); i++)
            {
                _textureIndices[i] = UploadToGl(bspAsset->_textures[i]);
            }

            for (auto &face : bspAsset->_faces)
            {
                for (int v = face.firstVertex; v < face.firstVertex + face.vertexCount; v++)
                {
                    auto &vertex = bspAsset->_vertices[v];

                    vertexBuffer
                        .uvs(glm::vec4(vertex.texcoords[1].x, vertex.texcoords[1].y, vertex.texcoords[0].x, vertex.texcoords[0].y))
                        .vertex(glm::vec3(vertex.position));
                }

                FaceType ft;
                ft.firstVertex = face.firstVertex;
                ft.vertexCount = face.vertexCount;
                ft.lightmapIndex = face.lightmap;
                ft.textureIndex = face.texture;
                _faces.push_back(ft);
            }

            vertexBuffer
                .setup(shader);
        }

        spdlog::debug("loaded {0} vertices", vertexBuffer.vertexCount());

        return true;
    }

    void Resize(
        int width,
        int height)
    {
        glViewport(0, 0, width, height);

        // Calculate the projection and view matrix
        matrix = glm::perspective(glm::radians(90.0f), float(width) / float(height), 0.1f, 4096.0f);

        spdlog::debug("recalculated matrx: {0}", glm::to_string(matrix));
    }

    void Destroy()
    {
        if (bspAsset != nullptr)
        {
            delete bspAsset;
            bspAsset = nullptr;
        }
    }

    std::chrono::milliseconds::rep _lastTime;

    bool Tick(
        std::chrono::milliseconds::rep time,
        const struct InputState &inputState)
    {
        const float speed = 0.25f;
        float diff = float(time - _lastTime);

        if (inputState.KeyboardButtonStates[KeyboardButtons::KeyLeft] || inputState.KeyboardButtonStates[KeyboardButtons::KeyA])
        {
            _cam.MoveLeft(speed * diff);
        }
        else if (inputState.KeyboardButtonStates[KeyboardButtons::KeyRight] || inputState.KeyboardButtonStates[KeyboardButtons::KeyD])
        {
            _cam.MoveLeft(-speed * diff);
        }

        if (inputState.KeyboardButtonStates[KeyboardButtons::KeyUp] || inputState.KeyboardButtonStates[KeyboardButtons::KeyW])
        {
            _cam.MoveForward(speed * diff);
        }
        else if (inputState.KeyboardButtonStates[KeyboardButtons::KeyDown] || inputState.KeyboardButtonStates[KeyboardButtons::KeyS])
        {
            _cam.MoveForward(-speed * diff);
        }

        static int lastPointerX = inputState.MousePointerPosition[0];
        static int lastPointerY = inputState.MousePointerPosition[1];

        int diffX = -(inputState.MousePointerPosition[0] - lastPointerX);
        int diffY = -(inputState.MousePointerPosition[1] - lastPointerY);

        lastPointerX = inputState.MousePointerPosition[0];
        lastPointerY = inputState.MousePointerPosition[1];

        if (inputState.MouseButtonStates[MouseButtons::LeftButton])
        {
            _cam.RotateZ(glm::radians(float(diffX) * 0.1f));
            _cam.RotateX(glm::radians(float(diffY) * 0.1f));
        }

        _lastTime = time;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        // Select shader
        shader.use();

        // Upload projection and view matrix into shader
        shader.setupMatrices(matrix * _cam.GetViewMatrix());

        for (size_t i = 0; i < _faces.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _textureIndices[_faces[i].textureIndex]);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _lightmapIndices[i]);
            vertexBuffer.bind();

            glDrawArrays(GL_TRIANGLE_FAN, _faces[i].firstVertex, _faces[i].vertexCount);
        }

        vertexBuffer.unbind();

        return true; // to keep running
    }

    void SetFilename(const char *fn)
    {
        filename = fn;
    }

private:
    std::string filename;
    valve::hl1::BspAsset *bspAsset = nullptr;
    glm::mat4 matrix = glm::mat4(1.0f);
    ShaderType shader;
    BufferType vertexBuffer;
    std::vector<GLuint> _textureIndices;
    std::vector<GLuint> _lightmapIndices;
    std::vector<FaceType> _faces;
    std::map<GLuint, FaceType> _facesByLightmapAtlas;
    Camera _cam;
};

int main(
    int argc,
    char *argv[])
{
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug

    Test t;

    if (argc > 1)
    {
        spdlog::debug("loading map {0}", argv[1]);
        t.SetFilename(argv[1]);
    }

    return Application::Run<Test>(t);
}
