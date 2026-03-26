/*
 * Minimal glad stub - uses system OpenGL
 */

#ifndef GLAD_GL_H_
#define GLAD_GL_H_

#include <GL/gl.h>

typedef void* (*GLADloadproc)(const char*);

#define GLAD_LOADER_LOAD_LOADER 1

static inline int gladLoadGLLoader(GLADloadproc load) {
    return 1;
}

#endif /* GLAD_GL_H_ */
