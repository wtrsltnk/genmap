
#define APPLICATION_IMPLEMENTATION
#include "include/application.h"

#include "hl1bspasset.h"
#include "hl1filesystem.h"
#include "include/glshader.h"

#include "include/glbuffer.h"

#include "camera.h"
#include "entitycomponents.h"
#include <entt/entt.hpp>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stb_image.h>

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

class GenMap
{
public:
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

    void SetupSky()
    {
        // here we make up for the half of pixel to get the sky textures really stitched together because clamping is not enough
        const float uv_1 = 255.0f / 256.0f;
        const float uv_0 = 1.0f - uv_1;
        const float size = 1.0f;

        //if (renderFlag & SKY_BACK)
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_0, 0, 0))
            .vertex(glm::vec3(-size, size, size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_1, 0, 0))
            .vertex(glm::vec3(-size, -size, size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_1, 0, 0))
            .vertex(glm::vec3(size, -size, size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_0, 0, 0))
            .vertex(glm::vec3(size, size, size));

        //if (renderFlag & SKY_DOWN)
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_1, 0, 0))
            .vertex(glm::vec3(-size, -size, size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_1, 0, 0))
            .vertex(glm::vec3(-size, -size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_0, 0, 0))
            .vertex(glm::vec3(size, -size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_0, 0, 0))
            .vertex(glm::vec3(size, -size, size));

        //if (renderFlag & SKY_FRONT)
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_0, 0, 0))
            .vertex(glm::vec3(size, size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_1, 0, 0))
            .vertex(glm::vec3(size, -size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_1, 0, 0))
            .vertex(glm::vec3(-size, -size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_0, 0, 0))
            .vertex(glm::vec3(-size, size, -size));

        // glBindTextureif (renderFlag & SKY_LEFT)
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_0, 0, 0))
            .vertex(glm::vec3(-size, size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_1, 0, 0))
            .vertex(glm::vec3(-size, -size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_1, 0, 0))
            .vertex(glm::vec3(-size, -size, size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_0, 0, 0))
            .vertex(glm::vec3(-size, size, size));

        //if (renderFlag & SKY_RIGHT)
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_1, 0, 0))
            .vertex(glm::vec3(size, -size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_0, 0, 0))
            .vertex(glm::vec3(size, size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_0, 0, 0))
            .vertex(glm::vec3(size, size, size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_1, 0, 0))
            .vertex(glm::vec3(size, -size, size));

        //if (renderFlag & SKY_UP)
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_0, 0, 0))
            .vertex(glm::vec3(-size, size, -size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_0, 0, 0))
            .vertex(glm::vec3(-size, size, size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_0, uv_1, 0, 0))
            .vertex(glm::vec3(size, size, size));
        _skyVertexBuffer
            .uvs(glm::vec4(uv_1, uv_1, 0, 0))
            .vertex(glm::vec3(size, size, -size));

        _skyShader.compile(skyVertexShader, skyFragmentShader);
        _skyVertexBuffer.setup(_skyShader);
    }

    void SetupBsp()
    {
        _normalBlendingShader.compileDefaultShader();
        _solidBlendingShader.compile(solidBlendingVertexShader, solidBlendingFragmentShader);

        _lightmapIndices = std::vector<GLuint>(_bspAsset->_lightMaps.size());
        glActiveTexture(GL_TEXTURE1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (size_t i = 0; i < _bspAsset->_lightMaps.size(); i++)
        {
            _lightmapIndices[i] = UploadToGl(_bspAsset->_lightMaps[i]);
        }

        _textureIndices = std::vector<GLuint>(_bspAsset->_textures.size());
        glActiveTexture(GL_TEXTURE0);
        for (size_t i = 0; i < _bspAsset->_textures.size(); i++)
        {
            _textureIndices[i] = UploadToGl(_bspAsset->_textures[i]);
        }

        for (size_t f = 0; f < _bspAsset->_faces.size(); f++)
        {
            auto &face = _bspAsset->_faces[f];

            FaceType ft;
            ft.flags = face.flags;
            ft.firstVertex = 0;
            ft.vertexCount = 0;
            ft.lightmapIndex = 0;
            ft.textureIndex = 0;

            if (face.flags == 0)
            {
                ft.firstVertex = _vertexBuffer.vertexCount();
                ft.vertexCount = face.vertexCount;
                ft.lightmapIndex = f;
                ft.textureIndex = face.texture;

                for (int v = face.firstVertex; v < face.firstVertex + face.vertexCount; v++)
                {
                    auto &vertex = _bspAsset->_vertices[v];

                    _vertexBuffer
                        .uvs(glm::vec4(vertex.texcoords[1].x, vertex.texcoords[1].y, vertex.texcoords[0].x, vertex.texcoords[0].y))
                        .vertex(glm::vec3(vertex.position));
                }
            }

            _faces.push_back(ft);
        }

        _vertexBuffer
            .setup(_normalBlendingShader);

        for (auto &bspEntity : _bspAsset->_entities)
        {
            const auto entity = _registry.create();

            // spdlog::info("Entity {}", bspEntity.classname);
            // for (auto kvp : bspEntity.keyvalues)
            // {
            //     spdlog::info("    {} = {}", kvp.first, kvp.second);
            // }
            // spdlog::info(" ");

            if (bspEntity.classname == "worldspawn")
            {
                _registry.assign<ModelComponent>(entity, 0);
            }

            if (bspEntity.keyvalues.count("model") != 0)
            {
                ModelComponent mc = {0};
                char astrix;

                std::istringstream(bspEntity.keyvalues["model"]) >> astrix >> mc.Model;

                if (mc.Model != 0)
                {
                    _registry.assign<ModelComponent>(entity, mc);
                }
                else
                {
                    // todo, this probably is a mdl or spr file
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

        for (int i = 0; i < 6; i++)
        {
            _skyTextureIndices[i] = UploadToGl(_bspAsset->_skytextures[i]);
        }

        spdlog::debug("loaded {0} vertices", _vertexBuffer.vertexCount());
    }

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

        spdlog::info("{} @ {}", _fs.Mod(), _fs.Root().generic_string());

        _bspAsset = new valve::hl1::BspAsset(&_fs);
        if (_bspAsset->Load(_map))
        {
            SetupBsp();
        }

        SetupSky();

        _registry.sort<RenderComponent>([](const RenderComponent &lhs, const RenderComponent &rhs) {
            return lhs.Mode < rhs.Mode;
        });

        return true;
    }

    void Resize(
        int width,
        int height)
    {
        glViewport(0, 0, width, height);

        _projectionMatrix = glm::perspective(glm::radians(90.0f), float(width) / float(height), 0.1f, 4096.0f);
    }

    void Destroy()
    {
        if (_bspAsset != nullptr)
        {
            delete _bspAsset;
            _bspAsset = nullptr;
        }
    }

    std::chrono::milliseconds::rep _lastTime;

    bool Tick(
        std::chrono::milliseconds::rep time,
        const struct InputState &inputState)
    {
        const float speed = 0.85f;
        float timeStep = float(time - _lastTime);
        _lastTime = time;

        if (inputState.KeyboardButtonStates[KeyboardButtons::KeyLeft] || inputState.KeyboardButtonStates[KeyboardButtons::KeyA])
        {
            _cam.MoveLeft(speed * timeStep);
        }
        else if (inputState.KeyboardButtonStates[KeyboardButtons::KeyRight] || inputState.KeyboardButtonStates[KeyboardButtons::KeyD])
        {
            _cam.MoveLeft(-speed * timeStep);
        }

        if (inputState.KeyboardButtonStates[KeyboardButtons::KeyUp] || inputState.KeyboardButtonStates[KeyboardButtons::KeyW])
        {
            _cam.MoveForward(speed * timeStep);
        }
        else if (inputState.KeyboardButtonStates[KeyboardButtons::KeyDown] || inputState.KeyboardButtonStates[KeyboardButtons::KeyS])
        {
            _cam.MoveForward(-speed * timeStep);
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

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderSky();
        RenderBsp();

        return true; // to keep running
    }

    void RenderSky()
    {
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        _skyShader.use();

        _skyShader.setupMatrices(_projectionMatrix * (_cam.GetViewMatrix() * glm::rotate(glm::translate(glm::mat4(1.0f), _cam.Position()), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))));

        _skyVertexBuffer.bind();
        glActiveTexture(GL_TEXTURE0);

        for (int i = 0; i < SkyTextures::Count; i++)
        {
            glBindTexture(GL_TEXTURE_2D, _skyTextureIndices[i]);

            glDrawArrays(GL_QUADS, i * 4, 4);
        }

        _skyVertexBuffer.unbind();
    }

    void RenderBsp()
    {
        _vertexBuffer.bind();

        glEnable(GL_DEPTH_TEST);

        glDisable(GL_BLEND);
        RenderModelsByRenderMode(RenderModes::NormalBlending, _normalBlendingShader, _projectionMatrix * _cam.GetViewMatrix());

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_DST_ALPHA);
        RenderModelsByRenderMode(RenderModes::TextureBlending, _normalBlendingShader, _projectionMatrix * _cam.GetViewMatrix());

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderModelsByRenderMode(RenderModes::SolidBlending, _solidBlendingShader, _projectionMatrix * _cam.GetViewMatrix());

        _vertexBuffer.unbind();
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

            auto model = _bspAsset->_models[modelComponent.Model];

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
        const char *root,
        const char *map)
    {
        _fs.FindRootFromFilePath(root);
        _map = map;
    }

private:
    FileSystem _fs;
    std::string _map;
    valve::hl1::BspAsset *_bspAsset = nullptr;
    glm::mat4 _projectionMatrix = glm::mat4(1.0f);
    ShaderType _skyShader;
    BufferType _skyVertexBuffer;
    GLuint _skyTextureIndices[6] = {0, 0, 0, 0, 0, 0};
    ShaderType _normalBlendingShader;
    ShaderType _solidBlendingShader;
    BufferType _vertexBuffer;
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

    if (argc > 2)
    {
        spdlog::debug("loading map {1} from {0}", argv[1], argv[2]);
        t.SetFilename(argv[1], argv[2]);
    }

    return Application::Run<GenMap>(t);
}
