#include "opengl_main.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <iostream>

//--- HELPER ---//
static void APIENTRY OpenGLDebugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    std::cerr << "\nOpenGL Debug: " << message << std::endl;
} // end of OpenGLDebugCallback()


//--- PUBLIC ---//
OpenGLMain::OpenGLMain() = default;

OpenGLMain::~OpenGLMain() = default;

void OpenGLMain::init()
{
	// INITIALIZE GLAD + CHECK
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		throw std::runtime_error("GLAD initialization failure!");
	} // end if

#ifdef _DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    glDebugMessageCallback(OpenGLDebugCallback, nullptr);

    // disable all spam notification non error messages
    glDebugMessageControl(
        GL_DONT_CARE,
        GL_DONT_CARE,
        GL_DEBUG_SEVERITY_NOTIFICATION,
        0,
        nullptr,
        GL_FALSE
    );
#endif
} // end of init()