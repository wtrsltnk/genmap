
#define APPLICATION_IMPLEMENTATION
#include "include/application.h"

#include "hl1bspasset.h"
#include "include/glshader.h"

#include "include/glbuffer.h"

//#define GLMATH_IMPLEMENTATION
//#include <glmath.h>

#include "camera.h"
#include "entitycomponents.h"
#include <entt/entt.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stb_image.h>

namespace fs = std::filesystem;

template <class T>
inline std::istream &operator>>(
    std::istream &str, T &v)
{
    unsigned int tmp = 0;

    if (str >> tmp)
    {
        v = static_cast<T>(tmp);
    }

    return str;
}

class FileSystem :
    public valve::IFileSystem
{
public:
    void FindRootFromFilePath(
        const std::string &filePath);

    virtual std::string LocateFile(
        const std::string &relativeFilename);

    virtual bool LoadFile(
        const std::string &filename,
        valve::Array<valve::byte> &data);

    const fs::path &Root() const;
    const std::string &Mod() const;

private:
    fs::path _root;
    std::string _mod;
};

const fs::path &FileSystem::Root() const
{
    return _root;
}

const std::string &FileSystem::Mod() const
{
    return _mod;
}

void FileSystem::FindRootFromFilePath(
    const std::string &filePath)
{
    auto path = fs::path(filePath);

    if (!path.has_parent_path())
    {
        spdlog::error("given path ({}) has no parent path", filePath);

        return;
    }

    path = path.parent_path();

    auto fn = path.filename();
    if (path.has_parent_path() && (fn == "maps" || fn == "models" || fn == "sprites" || fn == "sound" || fn == "gfx" || fn == "env"))
    {
        path = path.parent_path();
    }

    auto lastDirectory = path.filename().generic_string();

    do
    {
        for (auto &p : fs::directory_iterator(path))
        {
            if (p.is_directory())
            {
                continue;
            }

            if (p.path().filename() == "hl.exe" && p.path().has_parent_path())
            {
                _root = p.path().parent_path();
                _mod = lastDirectory;
                return;
            }
        }

        lastDirectory = path.filename().generic_string();
        path = path.parent_path();

    } while (path.has_parent_path());
}

std::string FileSystem::LocateFile(
    const std::string &relativeFilename)
{
    auto modDir = _root / fs::path(_mod);

    if (fs::exists(modDir / fs::path(relativeFilename)))
    {
        return modDir.generic_string();
    }

    auto valveDir = _root / fs::path("valve");

    if (fs::exists(valveDir / fs::path(relativeFilename)))
    {
        return valveDir.generic_string();
    }

    return "";
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
    int flags;
};

const std::string solidBlendingVertexShader(
    "#version 150\n"

    "in vec3 a_vertex;"
    "in vec4 a_texcoords;"

    "uniform mat4 u_matrix;"

    "out vec2 f_uv_tex;"
    "out vec2 f_uv_light;"

    "void main()"
    "{"
    "    gl_Position = u_matrix * vec4(a_vertex.xyz, 1.0);"
    "    f_uv_light = a_texcoords.xy;"
    "    f_uv_tex = a_texcoords.zw;"
    "}");

const std::string solidBlendingFragmentShader(
    "#version 150\n"

    "uniform sampler2D u_tex0;"
    "uniform sampler2D u_tex1;"

    "in vec2 f_uv_tex;"
    "in vec2 f_uv_light;"

    "out vec4 color;"

    "void main()"
    "{"
    "    vec4 texel0, texel1;"
    "    texel0 = texture2D(u_tex0, f_uv_tex);"
    "    texel1 = texture2D(u_tex1, f_uv_light);"
    "    vec4 tempcolor = texel0 * texel1;"
    "    if (texel0.a < 0.2) discard;"
    "    else tempcolor = vec4(texel0.rgb, 1.0) * vec4(texel1.rgb, 1.0);"
    "    color = tempcolor;"
    "}");

const std::string skyVertexShader(
    "#version 150\n"

    "in vec3 a_vertex;"
    "in vec4 a_texcoords;"

    "uniform mat4 u_matrix;"

    "out vec2 texCoord;"

    "void main()"
    "{"
    "   gl_Position = u_matrix * vec4(a_vertex, 1.0);"
    "	texCoord = a_texcoords.xy;"
    "}");

const std::string skyFragmentShader(
    "#version 150\n"

    "in vec2 texCoord;"

    "uniform sampler2D tex;"

    "out vec4 color;"

    "void main()"
    "{"
    "    color = texture(tex, texCoord);"
    "}");

GLuint sky_textures[6] = {0, 0, 0, 0, 0, 0};

class GenMap
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

        glClearColor(0.0f, 0.45f, 0.7f, 1.0f);

        normalBlendingShader.compileDefaultShader();
        solidBlendingShader.compile(solidBlendingVertexShader, solidBlendingFragmentShader);
        skyShader.compile(skyVertexShader, skyFragmentShader);

        GenerateSkyVbo();
        skyVertexBuffer.setup(skyShader);

        FileSystem fs;

        fs.FindRootFromFilePath(filename);

        spdlog::info("{} @ {}", fs.Mod(), fs.Root().generic_string());

        bspAsset = new valve::hl1::BspAsset(&fs);
        if (bspAsset->Load(filename))
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

            for (size_t f = 0; f < bspAsset->_faces.size(); f++)
            {
                auto &face = bspAsset->_faces[f];

                FaceType ft;
                ft.flags = face.flags;
                ft.firstVertex = 0;
                ft.vertexCount = 0;
                ft.lightmapIndex = 0;
                ft.textureIndex = 0;

                if (face.flags == 0)
                {
                    ft.firstVertex = vertexBuffer.vertexCount();
                    ft.vertexCount = face.vertexCount;
                    ft.lightmapIndex = f;
                    ft.textureIndex = face.texture;

                    for (int v = face.firstVertex; v < face.firstVertex + face.vertexCount; v++)
                    {
                        auto &vertex = bspAsset->_vertices[v];

                        vertexBuffer
                            .uvs(glm::vec4(vertex.texcoords[1].x, vertex.texcoords[1].y, vertex.texcoords[0].x, vertex.texcoords[0].y))
                            .vertex(glm::vec3(vertex.position));
                    }
                }

                _faces.push_back(ft);
            }

            vertexBuffer
                .setup(normalBlendingShader);

            for (auto &bspEntity : bspAsset->_entities)
            {
                const auto entity = _registry.create();

                //                spdlog::info("Entity {}", bspEntity.classname);
                //                for (auto kvp : bspEntity.keyvalues)
                //                {
                //                    spdlog::info("    {} = {}", kvp.first, kvp.second);
                //                }
                //                spdlog::info(" ");

                if (bspEntity.classname == "worldspawn")
                {
                    _registry.assign<ModelComponent>(entity, 0);
                }

                auto model = bspEntity.keyvalues.find("model");
                if (model != bspEntity.keyvalues.end())
                {
                    ModelComponent mc = {0};
                    char astrix;

                    std::istringstream(model->second) >> astrix >> mc.Model;

                    if (mc.Model != 0)
                    {
                        _registry.assign<ModelComponent>(entity, mc);
                    }
                    else
                    {
                        // todo, thi sprobably is a mdl or spr file
                    }
                }

                RenderComponent rc = {0, {255, 255, 255}, RenderModes::NormalBlending};

                auto renderamt = bspEntity.keyvalues.find("renderamt");
                if (renderamt != bspEntity.keyvalues.end())
                {
                    std::istringstream(renderamt->second) >> (rc.Amount);
                }

                auto rendercolor = bspEntity.keyvalues.find("rendercolor");
                if (rendercolor != bspEntity.keyvalues.end())
                {
                    std::istringstream(rendercolor->second) >> (rc.Color[0]) >> (rc.Color[1]) >> (rc.Color[2]);
                }

                auto rendermode = bspEntity.keyvalues.find("rendermode");
                if (rendermode != bspEntity.keyvalues.end())
                {
                    std::istringstream(rendermode->second) >> (rc.Mode);
                }

                _registry.assign<RenderComponent>(entity, rc);

                auto origin = bspEntity.keyvalues.find("origin");
                if (origin != bspEntity.keyvalues.end())
                {
                    glm::vec3 originPosition;

                    std::istringstream(origin->second) >> originPosition.x >> originPosition.y >> originPosition.z;

                    _registry.assign<OriginComponent>(entity, originPosition);
                }
                else
                {
                    _registry.assign<OriginComponent>(entity, glm::vec3(0.0f));
                }
            }
        }

        _registry.sort<RenderComponent>([](const RenderComponent &lhs, const RenderComponent &rhs) {
            return lhs.Mode < rhs.Mode;
        });

        for (int i = 0; i < 6; i++)
        {
            sky_textures[i] = UploadToGl(bspAsset->_skytextures[i]);
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
        _projectionMatrix = glm::perspective(glm::radians(90.0f), float(width) / float(height), 0.1f, 4096.0f);
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
        const float speed = 0.85f;
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
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        skyShader.use();

        skyShader.setupMatrices(_projectionMatrix * (_cam.GetViewMatrix() * glm::rotate(glm::translate(glm::mat4(1.0f), _cam.Position()), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))));

        skyVertexBuffer.bind();
        glActiveTexture(GL_TEXTURE0);

        for (int i = 0; i < SkyTextures::Count; i++)
        {
            glBindTexture(GL_TEXTURE_2D, sky_textures[i]);

            glDrawArrays(GL_QUADS, i * 4, 4);
        }

        skyVertexBuffer.unbind();

        vertexBuffer.bind();

        glEnable(GL_DEPTH_TEST);

        glDisable(GL_BLEND);
        RenderModelsByRenderMode(RenderModes::NormalBlending, normalBlendingShader, _projectionMatrix * _cam.GetViewMatrix());

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_DST_ALPHA);
        RenderModelsByRenderMode(RenderModes::TextureBlending, normalBlendingShader, _projectionMatrix * _cam.GetViewMatrix());

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderModelsByRenderMode(RenderModes::SolidBlending, solidBlendingShader, _projectionMatrix * _cam.GetViewMatrix());

        vertexBuffer.unbind();

        return true; // to keep running
    }

    void RenderModelsByRenderMode(
        RenderModes mode,
        ShaderType &shader,
        const glm::mat4 &matrix)
    {
        shader.use();

        auto view = _registry.view<RenderComponent, ModelComponent, OriginComponent>();

        if (mode == RenderModes::NormalBlending)
        {
            shader.setupColor(glm::vec4(
                1.0f,
                1.0f,
                1.0f,
                1.0f));
        }

        for (auto entity : view)
        {
            auto renderComponent = _registry.get<RenderComponent>(entity);

            if (renderComponent.Mode != mode)
            {
                continue;
            }

            if (mode == RenderModes::TextureBlending || mode == RenderModes::SolidBlending)
            {
                shader.setupColor(glm::vec4(
                    1.0f,
                    1.0f,
                    1.0f,
                    float(renderComponent.Amount) / 255.0f));
            }

            auto modelComponent = _registry.get<ModelComponent>(entity);
            auto originComponent = _registry.get<OriginComponent>(entity);

            shader.setupMatrices(glm::translate(matrix, originComponent.Origin));

            auto model = bspAsset->_models[modelComponent.Model];

            for (int i = model.firstFace; i < model.firstFace + model.faceCount; i++)
            {
                if (_faces[i].flags > 0)
                {
                    continue;
                }

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, _textureIndices[_faces[i].textureIndex]);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, _lightmapIndices[_faces[i].lightmapIndex]);

                glDrawArrays(GL_TRIANGLE_FAN, _faces[i].firstVertex, _faces[i].vertexCount);
            }
        }
    }

    void SetFilename(
        const char *fn)
    {
        filename = fn;
    }

    enum SkyTextures
    {
        Back = 0,
        Down = 1,
        Front = 2,
        Left = 3,
        Right = 4,
        up = 5,
        Count,
    };

    void GenerateSkyVbo()
    {
        // here we make up for the half of pixel to get the sky textures really stitched together because clamping is not enough
        const float uv_1 = 255.0f / 256.0f;
        const float uv_0 = 1.0f - uv_1;
        const float size = 1.0f;

        std::vector<float> buffer;

        //if (renderFlag & SKY_BACK)
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, size, size));
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, -size, size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, -size, size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, size, size));

        //if (renderFlag & SKY_DOWN)
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, -size, size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, -size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, -size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, -size, size));

        //if (renderFlag & SKY_FRONT)
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, -size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, -size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, size, -size));

        // glBindTextureif (renderFlag & SKY_LEFT)
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, -size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, -size, size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, size, size));

        //if (renderFlag & SKY_RIGHT)
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, -size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, size, size));
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, -size, size));

        //if (renderFlag & SKY_UP)
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, size, -size));
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_0, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(-size, size, size));
        skyVertexBuffer.uvs(glm::vec4(uv_0, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, size, size));
        skyVertexBuffer.uvs(glm::vec4(uv_1, uv_1, 0, 0));
        skyVertexBuffer.vertex(glm::vec3(size, size, -size));
    }

private:
    std::string filename;
    valve::hl1::BspAsset *bspAsset = nullptr;
    glm::mat4 _projectionMatrix = glm::mat4(1.0f);
    ShaderType skyShader;
    BufferType skyVertexBuffer;
    GLuint skyCubeMap = 0;
    ShaderType normalBlendingShader;
    ShaderType solidBlendingShader;
    BufferType vertexBuffer;
    std::vector<GLuint> _textureIndices;
    std::vector<GLuint> _lightmapIndices;
    std::vector<FaceType> _faces;
    std::map<GLuint, FaceType> _facesByLightmapAtlas;
    Camera _cam;
    entt::registry _registry;
};

int main(
    int argc,
    char *argv[])
{
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug

    GenMap t;

    if (argc > 1)
    {
        spdlog::debug("loading map {0}", argv[1]);
        t.SetFilename(argv[1]);
    }

    return Application::Run<GenMap>(t);
}
