#pragma once

#include <iostream>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "gl_header.h"
#include "utils.h"

extern std::string glsl_path;

namespace SimpleRender {

class GLShaderProgram {
public:
    ~GLShaderProgram() {
        glDeleteShader(vertex_);
        glDeleteShader(geometry_);
        glDeleteShader(fragment_);
        glDeleteProgram(program_);
    }

    static std::shared_ptr<GLShaderProgram> load_program(
            const std::string& vertex_fn,
            const std::string& geom_fn, 
            const std::string& frag_fn,
            const char* vert_defines = 0,
            const char* geom_defines = 0,
            const char* frag_defines = 0) {
        // Vertex Shader
        std::shared_ptr<GLShaderProgram> p(new GLShaderProgram());
        bool success = true;
        if (!vertex_fn.empty()) {
            success = p->add_shader_from_source_code(
                    GL_VERTEX_SHADER, p->vertex_, vertex_fn, vert_defines);
        }
        // Geometry Shader
        if (!geom_fn.empty()) {
             success = p->add_shader_from_source_code(
                    GL_GEOMETRY_SHADER, p->geometry_, geom_fn, geom_defines);
        }
        // Fragment Shader
        if (!frag_fn.empty()) {
             success = p->add_shader_from_source_code(
                    GL_FRAGMENT_SHADER, p->fragment_, frag_fn, frag_defines);
        } 
        if (!success) {
            p.reset();
            exit(1);
        }

        return p;
    }

    void link() {
        // Shader Program
        GLint success;
        program_ = glCreateProgram();
        if (vertex_) glAttachShader(program_, vertex_);
        if (geometry_) glAttachShader(program_, geometry_);
        if (fragment_) glAttachShader(program_, fragment_);
        glLinkProgram(program_);
        // Print linking errors if any
        glGetProgramiv(program_, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar log[512];
            glGetProgramInfoLog(program_, 512, NULL, log);
            fprintf(stderr, "ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", log);
            fflush(stderr);
        }
        // Delete the shaders as they're linked into our program now and no
        // longer necessery
        if (vertex_) glDeleteShader(vertex_);
        if (geometry_) glDeleteShader(geometry_);
        if (fragment_) glDeleteShader(fragment_);
        CHECK_GL_ERROR;
    }

    void use() const {
        glUseProgram(program_);
        CHECK_GL_ERROR;
    }

    void release() const {
        glUseProgram(0);
    }

    GLint get_attribute_location(const char* name) const {
        return glGetAttribLocation(program_, name);
    }

    void bind_attribute_location(const char* name, GLuint index) {
        glBindAttribLocation(program_, index, name);
    }

    GLint get_uniform_location(const char* name) const {
        return glGetUniformLocation(program_, name);
    }

    void set_uniform_value(GLint loc, const glm::mat4& mat) {
        GLint t;
        glGetIntegerv(GL_CURRENT_PROGRAM, &t);
        assert(GLint(program_) == t);
        glUniformMatrix4fv(loc, 1, GL_FALSE, &mat[0][0]);
        CHECK_GL_ERROR;
    }

    void set_uniform_value(GLint loc, const glm::vec2& vec) {
        GLint t;
        glGetIntegerv(GL_CURRENT_PROGRAM, &t);
        assert(GLint(program_) == t);
        glUniform2fv(loc, 1, (const GLfloat*)&vec);
        CHECK_GL_ERROR;
    }

    void set_uniform_value(GLint loc, const glm::vec3& vec) {
        GLint t;
        glGetIntegerv(GL_CURRENT_PROGRAM, &t);
        assert(GLint(program_) == t);
        glUniform3fv(loc, 1, (const GLfloat*)&vec);
        CHECK_GL_ERROR;
    }

    void set_uniform_value(GLint loc, const glm::vec4& vec) {
        GLint t;
        glGetIntegerv(GL_CURRENT_PROGRAM, &t);
        assert(GLint(program_) == t);
        glUniform4fv(loc, 1, (const GLfloat*)&vec);
        CHECK_GL_ERROR;
    }

    void set_uniform_value(GLint loc, const float v) {
        GLint t;
        glGetIntegerv(GL_CURRENT_PROGRAM, &t);
        assert(GLint(program_) == t);
        glUniform1f(loc, v);
        CHECK_GL_ERROR;
    }


    void set_uniform_value(GLint loc, const int v) {
        GLint t;
        glGetIntegerv(GL_CURRENT_PROGRAM, &t);
        assert(GLint(program_) == t);
        glUniform1i(loc, v);
        CHECK_GL_ERROR;
    }

protected:
    GLuint vertex_;
    GLuint geometry_;
    GLuint fragment_;
    GLuint program_;

    GLShaderProgram() : vertex_(0), geometry_(0), fragment_(0), program_(0) {} 

    bool add_shader_from_source_code(const GLenum shader_type, 
                                     GLuint& shader_ref,
                                     const std::string& shader_fn,
                                     const char* shader_defines) {
        std::string shader_code = read_file(glsl_path + "/" + shader_fn);
        std::string header = "";
        if (shader_defines) {
            header += shader_defines;
        }
        shader_ref = glCreateShader(shader_type);
        auto err = glGetError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "ERROR::SHADER::CREATION_FAILED: %d\n", err);
            fflush(stderr);
            return false;
        }

        shader_code = header + shader_code;
        const char* tmp = shader_code.c_str();
        glShaderSource(shader_ref, 1, &tmp, NULL);

        glCompileShader(shader_ref);
        GLint success;
        glGetShaderiv(shader_ref, GL_COMPILE_STATUS, &success);
        if (success == GL_FALSE) {
            GLchar log[512];
            glGetShaderInfoLog(shader_ref, 512, NULL, log);
            fprintf(stderr, "ERROR::SHADER::COMPILATION_FAILED: %s\n%s\n",
                    shader_fn.c_str(), log);
            fflush(stderr);
            return false;
        }
        return true;
    }
};

} // namespace
