#ifndef GLSHADER_H
#define GLSHADER_H

#include "glad/glad.h"
//#include <glmath.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>

class ShaderType
{
public:
    ShaderType() = default;

    virtual ~ShaderType() = default;

    GLuint id() const
    {
        return _shaderId;
    }

    void use() const
    {
        glUseProgram(_shaderId);
    }

    bool compileDefaultShader()
    {
        static GLuint defaultShader = 0;

        if (defaultShader == 0)
        {
            std::string const vshader(
                "#version 150\n"

                "in vec3 vertex;"
                "in vec4 color;"

                "uniform mat4 u_matrix;"

                "out vec4 f_color;"

                "void main()"
                "{"
                "    gl_Position = u_matrix * vec4(vertex.xyz, 1.0);"
                "    f_color = color;"
                "}");

            std::string const fshader(
                "#version 150\n"

                "in vec4 f_color;"
                "out vec4 color;"

                "void main()"
                "{"
                "   color = f_color;"
                "}");

            if (compile(vshader, fshader))
            {
                defaultShader = _shaderId;

                return true;
            }

            return false;
        }

        return true;
    }

    virtual bool compile(
        const std::string &vertShaderStr,
        const std::string &fragShaderStr)
    {
        GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
        const char *vertShaderSrc = vertShaderStr.c_str();
        const char *fragShaderSrc = fragShaderStr.c_str();

        GLint result = GL_FALSE;
        GLint logLength;

        // Compile vertex shader
        glShaderSource(vertShader, 1, &vertShaderSrc, NULL);
        glCompileShader(vertShader);

        // Check vertex shader
        glGetShaderiv(vertShader, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE)
        {
            glGetShaderiv(vertShader, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> vertShaderError(static_cast<size_t>((logLength > 1) ? logLength : 1));
            glGetShaderInfoLog(vertShader, logLength, NULL, &vertShaderError[0]);
            std::cerr << &vertShaderError[0] << std::endl;

            return false;
        }

        // Compile fragment shader
        glShaderSource(fragShader, 1, &fragShaderSrc, NULL);
        glCompileShader(fragShader);

        // Check fragment shader
        glGetShaderiv(fragShader, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE)
        {
            glGetShaderiv(fragShader, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> fragShaderError(static_cast<size_t>((logLength > 1) ? logLength : 1));
            glGetShaderInfoLog(fragShader, logLength, NULL, &fragShaderError[0]);
            std::cerr << &fragShaderError[0] << std::endl;

            return false;
        }

        _shaderId = glCreateProgram();
        glAttachShader(_shaderId, vertShader);
        glAttachShader(_shaderId, fragShader);
        glLinkProgram(_shaderId);

        glGetProgramiv(_shaderId, GL_LINK_STATUS, &result);
        if (result == GL_FALSE)
        {
            glGetProgramiv(_shaderId, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> programError(static_cast<size_t>((logLength > 1) ? logLength : 1));
            glGetProgramInfoLog(_shaderId, logLength, NULL, &programError[0]);
            std::cerr << &programError[0] << std::endl;

            return false;
        }

        glDeleteShader(vertShader);
        glDeleteShader(fragShader);

        _matrixUniformId = glGetUniformLocation(_shaderId, _matrixUniformName.c_str());

        return true;
    }

    void setupMatrices(
        const glm::mat4 &matrix)
    {
        use();

        glUniformMatrix4fv(_matrixUniformId, 1, false, glm::value_ptr(matrix));
    }

    void setupAttributes() const
    {
        auto vertexSize = static_cast<GLsizei>(sizeof(glm::vec3) + sizeof(glm::vec4));

        auto vertexAttrib = glGetAttribLocation(_shaderId, _vertexAttributeName.c_str());
        glVertexAttribPointer(
            GLuint(vertexAttrib),
            sizeof(glm::vec3) / sizeof(float),
            GL_FLOAT,
            GL_FALSE,
            vertexSize, 0);

        glEnableVertexAttribArray(GLuint(vertexAttrib));

        auto colorAttrib = glGetAttribLocation(_shaderId, _colorAttributeName.c_str());
        glVertexAttribPointer(
            GLuint(colorAttrib),
            sizeof(glm::vec4) / sizeof(float),
            GL_FLOAT,
            GL_FALSE,
            vertexSize,
            reinterpret_cast<const GLvoid *>(sizeof(glm::vec3)));

        glEnableVertexAttribArray(GLuint(colorAttrib));
    }

private:
    GLuint _shaderId = 0;
    GLuint _matrixUniformId = 0;

    std::string _matrixUniformName = "u_matrix";
    std::string _vertexAttributeName = "vertex";
    std::string _colorAttributeName = "color";
};

#endif // GLSHADER_H
