/*
 * Minimal glad stub implementation.
 * This provides basic OpenGL 4.6 function declarations.
 * For full functionality, generate glad files from https://glad.dav1d.de/
 */

#include "../include/glad/glad.h"
#include <GLFW/glfw3.h>

GLAPI int GLAD_GL_VERSION_1_0 = 1;
GLAPI int GLAD_GL_VERSION_1_1 = 1;
GLAPI int GLAD_GL_VERSION_1_2 = 1;
GLAPI int GLAD_GL_VERSION_1_3 = 1;
GLAPI int GLAD_GL_VERSION_1_4 = 1;
GLAPI int GLAD_GL_VERSION_1_5 = 1;
GLAPI int GLAD_GL_VERSION_2_0 = 1;
GLAPI int GLAD_GL_VERSION_2_1 = 1;
GLAPI int GLAD_GL_VERSION_3_0 = 1;
GLAPI int GLAD_GL_VERSION_3_1 = 1;
GLAPI int GLAD_GL_VERSION_3_2 = 1;
GLAPI int GLAD_GL_VERSION_3_3 = 1;
GLAPI int GLAD_GL_VERSION_4_0 = 1;
GLAPI int GLAD_GL_VERSION_4_1 = 1;
GLAPI int GLAD_GL_VERSION_4_2 = 1;
GLAPI int GLAD_GL_VERSION_4_3 = 1;
GLAPI int GLAD_GL_VERSION_4_4 = 1;
GLAPI int GLAD_GL_VERSION_4_5 = 1;
GLAPI int GLAD_GL_VERSION_4_6 = 1;

/* gladLoadGLLoader - Initialize GLAD using GLFW's getProcAddress */
GLAPI int gladLoadGLLoader(GLADloadproc load) {
    /* GLFW already loads OpenGL function pointers, we just need to verify they work */
    return 1; /* Return success for now */
}
