#ifndef ENTITYCOMPONENTS_H
#define ENTITYCOMPONENTS_H

#include <glm/vec3.hpp>
#include <string>

struct OriginComponent
{
    glm::vec3 Origin;
};

struct ModelComponent
{
    int Model;
};

enum RenderModes
{
    NormalBlending = 0,
    ColorBlending = 1,
    TextureBlending = 2,
    GlowBlending = 3,
    SolidBlending = 4,
    AdditiveBlending = 5,
};

struct RenderComponent
{
    short Amount;
    short Color[3];
    RenderModes Mode;
};

#endif // ENTITYCOMPONENTS_H
