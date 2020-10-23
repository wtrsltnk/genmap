/*
 * 
 */

#define EXAMPLE_NAME "genmap"

#define APPLICATION_IMPLEMENTATION
#include "include/application.h"

#include "hl1bspasset.h"
#include "include/glshader.h"

#include "include/glbuffer.h"
#include "include/glmath.h"
#include <iostream>

static struct
{
    std::string filename;
    valve::hl1::BspAsset *bspAsset = nullptr;
    glm::mat4 matrix;
    glm::vec3 position;
    ShaderType shader;
    BufferType vertexBuffer;
} State;

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
        std::cout << "File not found:\n"
                  << "\t" << filename << "\n";
        return false;
    }

    auto count = file.tellg();

    data.Allocate(count);
    file.seekg(0, std::ios::beg);
    file.read((char *)data.data, data.count);

    file.close();

    return true;
}

bool Startup()
{
    glClearColor(0.0f, 0.8f, 1.0f, 1.0f);

    State.shader.compileDefaultShader();

    FileSystem fs;
    State.bspAsset = new valve::hl1::BspAsset(&fs);
    if (!State.bspAsset->Load(State.filename))
    {
        delete State.bspAsset;
        State.bspAsset = nullptr;

        State.vertexBuffer
            .color(glm::vec4(0.0f, 1.0f, 1.0f, 1.0f))
            .vertex(glm::vec3(-10.0f, -10.0f, 0.0f)) // mint
            .color(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f))
            .vertex(glm::vec3(-10.0f, 10.0f, 0.0f)) // geel
            .color(glm::vec4(1.0f, 0.0f, 1.0f, 1.0f))
            .vertex(glm::vec3(10.0f, 10.0f, 0.0f)) // paars
            .color(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
            .vertex(glm::vec3(10.0f, -10.0f, 0.0f)); // wit

        State.vertexBuffer
            .setup(GL_TRIANGLE_FAN, State.shader);
    }
    else
    {
        for (auto face : State.bspAsset->_faces)
        {
            for (int v = face.firstVertex; v < face.firstVertex + face.vertexCount; v++)
            {
                auto &vertex = State.bspAsset->_vertices[v];
                State.vertexBuffer
                    .color(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
                    .vertex(glm::vec3(vertex.position));
            }

            State.vertexBuffer
                .addFace(face.firstVertex, face.vertexCount);
        }

        State.vertexBuffer
            .setup(GL_TRIANGLE_FAN, State.shader);
    }

    return true;
}

void Resize(
    int width,
    int height)
{
    glViewport(0, 0, width, height);

    // Calculate the projection and view matrix
    State.matrix = glm::perspective(glm::radians(90.0f), float(width) / float(height), 0.1f, 4096.0f) *
                   glm::lookAt(State.position + glm::vec3(12.0f, 0.0f, 0.0f), State.position, glm::vec3(0.0f, 0.0f, 1.0f));
}

void Destroy()
{
    if (State.bspAsset != nullptr)
    {
        delete State.bspAsset;
        State.bspAsset = nullptr;
    }
}

bool Tick()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Select shader
    State.shader.use();

    // Upload projection and view matrix into shader
    State.shader.setupMatrices(State.matrix);

    // Render vertex buffer with selected shader
    State.vertexBuffer.render();

    return true; // to keep running
}

int main(
    int argc,
    char *argv[])
{
    if (argc > 1)
    {
        State.filename = argv[1];
    }

    auto app = Application::Create(Startup, Resize, Destroy);

    return app->Run(Tick);
}
