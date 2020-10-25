
#define APPLICATION_IMPLEMENTATION
#include "include/application.h"

#include "hl1bspasset.h"
#include "include/glshader.h"

#include "include/glbuffer.h"

//#define GLMATH_IMPLEMENTATION
//#include <glmath.h>

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
            spdlog::critical(message);
            return;
        case GL_DEBUG_SEVERITY_MEDIUM:
            spdlog::error(message);
            return;
        case GL_DEBUG_SEVERITY_LOW:
            spdlog::warn(message);
            return;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            spdlog::trace(message);
            return;
    }

    spdlog::debug("Unknown severity level!");
    spdlog::debug(message);
}

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
                .color(glm::vec4(0.0f, 1.0f, 1.0f, 1.0f))
                .vertex(glm::vec3(-10.0f, -10.0f, 0.0f)) // mint
                .color(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f))
                .vertex(glm::vec3(-10.0f, 10.0f, 0.0f)) // geel
                .color(glm::vec4(1.0f, 0.0f, 1.0f, 1.0f))
                .vertex(glm::vec3(10.0f, 10.0f, 0.0f)) // paars
                .color(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
                .vertex(glm::vec3(10.0f, -10.0f, 0.0f)); // wit

            vertexBuffer
                .setup(GL_TRIANGLE_FAN, shader);
        }
        else
        {
            for (auto face : bspAsset->_faces)
            {
                for (int v = face.firstVertex; v < face.firstVertex + face.vertexCount; v++)
                {
                    auto &vertex = bspAsset->_vertices[v];
                    vertexBuffer
                        .color(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
                        .vertex(glm::vec3(vertex.position));
                }

                vertexBuffer
                    .addFace(face.firstVertex, face.vertexCount);
            }

            vertexBuffer
                .setup(GL_TRIANGLE_FAN, shader);
        }

        spdlog::debug("loaded {0} faces", vertexBuffer.faceCount());
        spdlog::debug("loaded {0} vertices", vertexBuffer.vertexCount());

        return true;
    }

    void Resize(
        int width,
        int height)
    {
        glViewport(0, 0, width, height);

        // Calculate the projection and view matrix
        matrix = glm::perspective(glm::radians(120.0f), float(width) / float(height), 0.1f, 4096.0f);

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
        const float speed = 1.0f;
        float diff = float(time - _lastTime);

        if (inputState.KeyboardButtonStates[KeyboardButtons::KeyLeft])
        {
            position.y -= (speed * diff);
        }
        else if (inputState.KeyboardButtonStates[KeyboardButtons::KeyRight])
        {
            position.y += (speed * diff);
        }
        else if (inputState.KeyboardButtonStates[KeyboardButtons::KeyUp])
        {
            position.x -= (speed * diff);
        }
        else if (inputState.KeyboardButtonStates[KeyboardButtons::KeyDown])
        {
            position.x += (speed * diff);
        }

        _lastTime = time;

        glClear(GL_COLOR_BUFFER_BIT);

        // Select shader
        shader.use();

        auto m = matrix * glm::lookAt(position + glm::vec3(12.0f, 0.0f, 0.0f), position, glm::vec3(0.0f, 0.0f, 1.0f));

        // Upload projection and view matrix into shader
        shader.setupMatrices(m);

        // Render vertex buffer with selected shader
        vertexBuffer.render();

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
    glm::vec3 position = glm::vec3(0.0f);
    ShaderType shader;
    BufferType vertexBuffer;
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
