#ifndef GLMATH_H
#define GLMATH_H

#include <cmath>
#include <sstream>

namespace glm
{

struct vec2
{
    vec2() { }
    vec2(float v) : x(v), y(v) { }
    vec2(float _x, float _y) : x(_x), y(_y) { }

    union { float x; float s; } = 0.0f;
    union { float y; float t; } = 0.0f;

    float const &operator[] (
        int index) const
    {
        if (index == 0) return x;
        
        return y;
    }

    float &operator[] (
        int index)
    {
        if (index == 0) return x;
        
        return y;
    }
};

struct vec3
{
    vec3() { }
    vec3(float v) : x(v), y(v), z(v) { }
    vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) { }

    union { float x; float r; } = 0.0f;
    union { float y; float g; } = 0.0f;
    union { float z; float b; } = 0.0f;

    float const &operator[] (
        int index) const
    {
        if (index == 0) return x;
        if (index == 1) return y;

        return z;
    }

    float &operator[] (
        int index)
    {
        if (index == 0) return x;
        if (index == 1) return y;

        return z;
    }
};

struct vec4
{
    vec4()
    { }
    
    vec4(
        float v)
        : x(v),
          y(v),
          z(v),
          w(v)
    { }
    
    vec4(
        float _x,
        float _y,
        float _z,
        float _w)
        : x(_x),
          y(_y),
          z(_z),
          w(_w)
    { }

    union { float x; float r; } = 0.0f;
    union { float y; float g; } = 0.0f;
    union { float z; float b; } = 0.0f;
    union { float w; float a; } = 0.0f;

    float const &operator[] (
        int index) const
    {
        if (index == 0) return x;
        if (index == 1) return y;
        if (index == 2) return z;

        return w;
    }

    float &operator[] (
        int index)
    {
        if (index == 0) return x;
        if (index == 1) return y;
        if (index == 2) return z;

        return w;
    }
};

struct mat4
{
    mat4()
    { }
    
    mat4(
        float v)
    {
        // Identity
        values[0].x = v;
        values[1].y = v;
        values[2].z = v;
        values[3].w = v;
    }
    mat4(
        vec4 const &v0,
        vec4 const &v1,
        vec4 const &v2,
        vec4 const &v3)
    {
        values[0] = v0;
        values[1] = v1;
        values[2] = v2;
        values[3] = v3;
    }

    vec4 values[4];

    vec4 const &operator [] (
        int index) const
    {
        return values[index];
    }

    vec4 &operator [] (
        int index)
    {
        return values[index];
    }
};


vec2 operator + (vec2 const &v1, vec2 const &v2);
vec2 operator - (vec2 const &v1, vec2 const &v2);

vec2 cross(vec2 const &v1, vec2 const &v2);
float length(vec2 const &v);
vec2 normal(vec2 const &v);

vec3 operator + (vec3 const &v1, vec3 const &v2);
vec3 operator - (vec3 const &v1, vec3 const &v2);

vec3 cross(vec3 const &v1, vec3 const &v2);
float dot(vec3 const &v1, vec3 const &v2);
float length(vec3 const &v);
vec3 normal(vec3 const &v);

vec4 operator * (mat4 const &m, vec4 const &v);
vec4 operator * (vec4 const &v, mat4 const &m);
mat4 operator * (mat4 const &m1, mat4 const &m2);

float radians(float degrees);

float const *value_ptr(mat4 const &m);

mat4 perspective(float fovy, float aspect, float zNear, float zFar);
mat4 lookAt(vec3 const &eye, vec3 const &target, vec3 const &up);

std::string to_string(vec4 const &x);
std::string to_string(mat4 const &x);

vec3 to_vec3(vec4 const &v4);

}

#endif // GLMATH_H

#ifdef GLMATH_IMPLEMENTATION

namespace glm
{

vec2 operator + (vec2 const &v1, vec2 const &v2)
{
    return vec2(v1.x + v2.x, v1.y + v2.y);
}

vec2 operator - (vec2 const &v1, vec2 const &v2)
{
    return vec2(v1.x - v2.x, v1.y - v2.y);
}

vec2 cross(vec2 const &v1, vec2 const &v2)
{
    return v1.x * v2.y - v1.y * v2.x;
}

float length(vec2 const &v)
{
    return float(sqrt(v.x*v.x + v.y*v.y));
}

vec2 normal(vec2 const &v)
{
    auto l = length(v);
    return vec2(v.x / l, v.y / l);
}

vec3 operator + (vec3 const &v1, vec3 const &v2)
{
    return vec3(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
}

vec3 operator - (vec3 const &v1, vec3 const &v2)
{
    return vec3(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}

vec3 cross(vec3 const &v1, vec3 const &v2)
{
    return vec3(v1.y * v2.z - v1.z * v2.y,
                v1.z * v2.x - v1.x * v2.z,
                v1.x * v2.y - v1.y * v2.x);
}

float dot(vec3 const &v1, vec3 const &v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float length(vec3 const &v)
{
    return float(sqrt(v.x*v.x + v.y*v.y + v.z*v.z));
}

vec3 normal(vec3 const &v)
{
    auto l = length(v);
    return vec3(v.x / l, v.y / l, v.z / l);
}

vec4 operator * (mat4 const &m, vec4 const &v)
{
    return vec4(
        m[0][0] * v[0] + m[1][0] * v[1] + m[2][0] * v[2] + m[3][0] * v[3],
        m[0][1] * v[0] + m[1][1] * v[1] + m[2][1] * v[2] + m[3][1] * v[3],
        m[0][2] * v[0] + m[1][2] * v[1] + m[2][2] * v[2] + m[3][2] * v[3],
        m[0][3] * v[0] + m[1][3] * v[1] + m[2][3] * v[2] + m[3][3] * v[3]
    );
}

vec4 operator * (vec4 const &v, mat4 const &m)
{
    return vec4(
        v[0] * m[0][0] + v[1] * m[0][1] + v[2] * m[0][2] + v[3] * m[0][3],
        v[0] * m[1][0] + v[1] * m[1][1] + v[2] * m[1][2] + v[3] * m[1][3],
        v[0] * m[2][0] + v[1] * m[2][1] + v[2] * m[2][2] + v[3] * m[2][3],
        v[0] * m[3][0] + v[1] * m[3][1] + v[2] * m[3][2] + v[3] * m[3][3]
    );
}

mat4 operator * (mat4 const &m1, mat4 const &m2)
{
    vec4 X = m1 * m2[0];
    vec4 Y = m1 * m2[1];
    vec4 Z = m1 * m2[2];
    vec4 W = m1 * m2[3];

    return mat4(X, Y, Z, W);
}

float radians(float degrees)
{
    return degrees * 0.01745329251994329576923690768489f;
}

float const *value_ptr(mat4 const &m)
{
    return &m.values[0].x;
}

mat4 perspective(float fovy, float aspect, float zNear, float zFar)
{
    float const tanHalfFovy = tan(fovy / static_cast<float>(2));

    mat4 m(static_cast<float>(0));
    m[0][0] = static_cast<float>(1) / (aspect * tanHalfFovy);
    m[1][1] = static_cast<float>(1) / (tanHalfFovy);
    m[2][2] = - (zFar + zNear) / (zFar - zNear);
    m[2][3] = - static_cast<float>(1);
    m[3][2] = - (static_cast<float>(2) * zFar * zNear) / (zFar - zNear);

    return m;
}

mat4 lookAt(vec3 const &eye, vec3 const &target, vec3 const &up)
{
    vec3 zaxis = normal(eye - target);
    vec3 xaxis = normal(cross(up, zaxis));
    vec3 yaxis = cross(zaxis, xaxis);

    mat4 orientation = {
        vec4(xaxis.x, yaxis.x, zaxis.x, 0),
        vec4(xaxis.y, yaxis.y, zaxis.y, 0),
        vec4(xaxis.z, yaxis.z, zaxis.z, 0),
        vec4(  0,       0,       0,     1)
    };

    mat4 translation = {
        vec4(   1,      0,      0,   0),
        vec4(   0,      1,      0,   0),
        vec4(   0,      0,      1,   0),
        vec4(-eye.x, -eye.y, -eye.z, 1)
    };

    return (orientation * translation);
}

std::string to_string(vec4 const &x)
{
    std::stringstream ss;

    ss << "(" << x.x << ", " << x.y << ", " << x.z << ", " << x.w << ")";

    return ss.str();
}

std::string to_string(mat4 const &x)
{
    std::stringstream ss;

    ss << "mat4x4(" << to_string(x.values[0]) << ", "
            << to_string(x.values[1]) << ", "
            << to_string(x.values[2]) << ", "
            << to_string(x.values[3]) << ")";

    return ss.str();
}

vec3 to_vec3(vec4 const &v4)
{
    return vec3(v4.x, v4.y, v4.z);
}

}

#endif // GLMATH_IMPLEMENTATION