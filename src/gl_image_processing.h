#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <memory>
#include <Shader.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

class GlImageProcessing {
    GLFWwindow* window = nullptr;
    GLuint vao = 0;
    std::unique_ptr<Shader> imageShader;

    template<class T>
    static T proc(const char* name) {
        auto address = glfwGetProcAddress(name);
        const auto value = reinterpret_cast<std::intptr_t>(address);
        if (value == 0 || value == 1 || value == 2 || value == 3 || value == -1)
            throw std::runtime_error(std::string("Missing OpenGL function: ") + name);
        return reinterpret_cast<T>(address);
    }

    void close() { imageShader.reset(); if (window) glfwDestroyWindow(window); glfwTerminate(); }

    GLuint compileProgram(GLenum type, const char* source) {
        const auto createShader = proc<GLuint(APIENTRY*)(GLenum)>("glCreateShader");
        const auto shaderSource = proc<void(APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*)>("glShaderSource");
        const auto compileShader = proc<void(APIENTRY*)(GLuint)>("glCompileShader");
        const auto getShaderiv = proc<void(APIENTRY*)(GLuint, GLenum, GLint*)>("glGetShaderiv");
        const auto getShaderLog = proc<void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*)>("glGetShaderInfoLog");
        const auto createProgram = proc<GLuint(APIENTRY*)()>("glCreateProgram");
        const auto attachShader = proc<void(APIENTRY*)(GLuint, GLuint)>("glAttachShader");
        const auto linkProgram = proc<void(APIENTRY*)(GLuint)>("glLinkProgram");
        const auto getProgramiv = proc<void(APIENTRY*)(GLuint, GLenum, GLint*)>("glGetProgramiv");
        const auto getProgramLog = proc<void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*)>("glGetProgramInfoLog");
        const auto deleteShader = proc<void(APIENTRY*)(GLuint)>("glDeleteShader");

        const GLuint shader = createShader(type);
        shaderSource(shader, 1, &source, nullptr);
        compileShader(shader);
        GLint ok = 0;
        char log[2048] = {};
        getShaderiv(shader, 0x8B81, &ok); // GL_COMPILE_STATUS
        if (!ok) {
            getShaderLog(shader, sizeof(log), nullptr, log);
            throw std::runtime_error(std::string("GLSL compilation failed: ") + log);
        }
        const GLuint program = createProgram();
        attachShader(program, shader);
        linkProgram(program);
        deleteShader(shader);
        getProgramiv(program, 0x8B82, &ok); // GL_LINK_STATUS
        if (!ok) {
            getProgramLog(program, sizeof(log), nullptr, log);
            throw std::runtime_error(std::string("GLSL link failed: ") + log);
        }
        return program;
    }



public:
    GlImageProcessing(int w, int h) {
        try {
            if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            window = glfwCreateWindow(w, h, "Image Processing", nullptr, nullptr);
            if (!window) throw std::runtime_error("OpenGL 4.3 context unavailable");
            glfwMakeContextCurrent(window);
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
                throw std::runtime_error("GLAD initialization failed");
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            imageShader = std::make_unique<Shader>("src/res/shaders/image_processing.shader");
        } catch (...) {
            close();
            throw;
        }
    }

    ~GlImageProcessing() { close(); }
    GlImageProcessing(const GlImageProcessing&) = delete;
    GlImageProcessing& operator=(const GlImageProcessing&) = delete;

    std::vector<Byte> render(const std::vector<Byte>& input, int w, int h, bool halftone) {
        const int outW = halftone ? w * 2 : w;
        const int outH = halftone ? h * 2 : h;
        std::vector<Byte> output(size_t(outW) * outH);
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        const GLenum layout = halftone ? GL_RED : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, halftone ? GL_R8 : GL_RGBA8, w, h, 0, layout, GL_UNSIGNED_BYTE, input.data());
        glViewport(0, 0, outW, outH);
        glDrawBuffer(GL_BACK);
        glReadBuffer(GL_BACK);
        glDisable(GL_DITHER);
        imageShader->Bind();
        imageShader->SetUniform1i("inputImage", 0);
        imageShader->SetUniform1i("mode", halftone ? 1 : 0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glReadPixels(0, 0, outW, outH, GL_RED, GL_UNSIGNED_BYTE, output.data());
        glDeleteTextures(1, &texture);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL image processing failed");
        return output;
    }
    std::vector<Byte> compute(const std::vector<Byte>& gray, int w, int h, bool edges) {
        constexpr GLenum storage = 0x90D2; // GL_SHADER_STORAGE_BUFFER
        constexpr GLenum dynamicCopy = 0x88EA; // GL_DYNAMIC_COPY
        const auto genBuffers = proc<void(APIENTRY*)(GLsizei, GLuint*)>("glGenBuffers");
        const auto bindBuffer = proc<void(APIENTRY*)(GLenum, GLuint)>("glBindBuffer");
        const auto bufferData = proc<void(APIENTRY*)(GLenum, std::ptrdiff_t, const void*, GLenum)>("glBufferData");
        const auto bindBase = proc<void(APIENTRY*)(GLenum, GLuint, GLuint)>("glBindBufferBase");
        const auto dispatch = proc<void(APIENTRY*)(GLuint, GLuint, GLuint)>("glDispatchCompute");
        const auto barrier = proc<void(APIENTRY*)(GLbitfield)>("glMemoryBarrier");
        const auto getData = proc<void(APIENTRY*)(GLenum, std::ptrdiff_t, std::ptrdiff_t, void*)>("glGetBufferSubData");
        const auto deleteBuffers = proc<void(APIENTRY*)(GLsizei, const GLuint*)>("glDeleteBuffers");
        const auto deleteProgram = proc<void(APIENTRY*)(GLuint)>("glDeleteProgram");
        const auto useProgram = proc<void(APIENTRY*)(GLuint)>("glUseProgram");
        const auto location = proc<GLint(APIENTRY*)(GLuint, const char*)>("glGetUniformLocation");
        const auto uniform = proc<void(APIENTRY*)(GLint, GLint)>("glUniform1i");

        std::ifstream file("image_compute.glsl");
        if (!file) throw std::runtime_error("Cannot open image_compute.glsl");
        const std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        const std::vector<GLuint> input(gray.begin(), gray.end());
        std::vector<GLuint> values(gray.size());
        const auto bytes = static_cast<std::ptrdiff_t>(values.size() * sizeof(GLuint));
        const GLuint computeProgram = compileProgram(0x91B9, source.c_str()); // GL_COMPUTE_SHADER
        GLuint buffers[4] = {};
        genBuffers(4, buffers);
        for (GLuint i = 0; i < 4; ++i) {
            bindBuffer(storage, buffers[i]);
            bufferData(storage, i == 1 ? bytes * 4 : bytes, i == 0 ? input.data() : nullptr, dynamicCopy);
            bindBase(storage, i, buffers[i]);
        }
        useProgram(computeProgram);
        uniform(location(computeProgram, "width"), w);
        uniform(location(computeProgram, "height"), h);
        uniform(location(computeProgram, "mode"), edges ? 1 : 0);
        dispatch(1, 1, 1);
        barrier(0x0200); // GL_BUFFER_UPDATE_BARRIER_BIT
        bindBuffer(storage, buffers[2]);
        getData(storage, 0, bytes, values.data());
        useProgram(0);
        deleteBuffers(4, buffers);
        deleteProgram(computeProgram);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL compute failed");
        return std::vector<Byte>(values.begin(), values.end());
    }
};
