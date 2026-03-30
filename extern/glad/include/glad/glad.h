#ifndef GLAD_GL_H_
#define GLAD_GL_H_

/* Don't check for __gl_h_ since we may need to coexist with system OpenGL */
#ifndef __gl_h_
#define __gl_h_
#endif

/* Version flags are defined in glad.c, not as macros here */

#include <stddef.h>
#include <stdlib.h>

#if defined(_WIN32) && !defined(APIENTRY) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
#define APIENTRY __stdcall
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

#define GL_GLEXT_VERSION 202301

/* GLAPI macro definition - extern for declarations */
#ifndef GLAPI
#if defined(_WIN32) && !defined(GLAD_GLAPI_IMPL)
#define GLAPI extern __declspec(dllimport)
#elif defined(__GNUC__) && !defined(GLAD_GLAPI_IMPL)
#define GLAPI extern __attribute__((visibility("default")))
#else
#define GLAPI extern
#endif
#endif

/* Only define types if not already defined by system OpenGL */
#ifndef GL_VERSION_1_1
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLsizeiptr;
typedef int GLintptr;
typedef unsigned char GLubyte;
typedef float GLfloat;
typedef double GLdouble;
typedef short GLshort;
typedef unsigned short GLushort;
typedef signed char GLbyte;
typedef void GLvoid;
typedef unsigned int GLbitfield;
typedef double GLclampd;
typedef float GLclampf;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef long long GLint64;
typedef unsigned long long GLuint64;
#endif

#define GL_VERSION_1_1 1
#define GL_TRUE 1
#define GL_FALSE 0
#define GL_NONE 0
#define GL_ZERO 0
#define GL_ONE 1
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_OUT_OF_MEMORY 0x0505
#define GL_CONTEXT_LOST 0x0507

#define GL_RENDERER 0x1F01
#define GL_VENDOR 0x1F00
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03

#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207

#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_DITHER 0x0BD0
#define GL_CULL_FACE 0x0B44
#define GL_LINE_SMOOTH 0x0B20
#define GL_POLYGON_SMOOTH 0x0B41
#define GL_MULTISAMPLE 0x809D

#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400

#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_QUADS 0x0007
#define GL_QUAD_STRIP 0x0008
#define GL_POLYGON 0x0009

#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_DOUBLE 0x140A
#define GL_HALF_FLOAT 0x140B

#define GL哥哥 0x8225
#define GL_DEPTH_COMPONENT 0x1902
#define GL_RED 0x1903
#define GL_GREEN 0x1904
#define GL_BLUE 0x1905
#define GL_ALPHA 0x1906
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_BGRA 0x80E1
#define GL_BGR 0x80E0

#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE_RECTANGLE 0x84F5
#define GL_TEXTURE_CUBE_MAP 0x8513

#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_TEXTURE_UNITS 0x84E2

#define GL_CLAMP_TO_EDGE 0x812F
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_REPEAT 0x2901
#define GL_MIRRORED_REPEAT 0x8370
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703

#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072

#define GL_RGBA8 0x8058
#define GL_RGB8 0x8051
#define GL_RGBA16 0x805B
#define GL_RGB16 0x8054
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32 0x81A7
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_STENCIL_INDEX8 0x8D48
#define GL_RGBA32F 0x8814
#define GL_RGB32F 0x8815
#define GL_RGBA16F 0x881A
#define GL_RGB16F 0x881B

#define GL_VIEWPORT 0x0BA2
#define GL_DEPTH_RANGE 0x0B70
#define GL_SCISSOR_BOX 0x0C10
#define GL_BLEND_COLOR 0x8005
#define GL_BLEND_EQUATION 0x8009
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_BLEND_DST_ALPHA 0x80CA

#define GL_FUNC_ADD 0x8006
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_MIN 0x8007
#define GL_MAX 0x8008

#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_STENCIL_WRITEMASK 0x0B98

#define GL_CULL_FACE_MODE 0x0B45
#define GL_FRONT_FACE 0x0B46
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_BACK 0x0405
#define GL_FRONT 0x0404
#define GL_FRONT_AND_BACK 0x0408

#define GL_LINE_WIDTH 0x0B21
#define GL_LINE_WIDTH_RANGE 0x0B22
#define GL_POINT_SIZE 0x0B11
#define GL_POINT_SIZE_RANGE 0x0B12
#define GL_POLYGON_MODE 0x0B40
#define GL_FILL 0x1B02
#define GL_LINE 0x1B01
#define GL_POINT 0x1B00

#define GL_STENCIL_FUNC 0x0B92
#define GL_STENCIL_REF 0x0B97
#define GL_STENCIL_VALUE_MASK 0x0B93
#define GL_STENCIL_FAIL 0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL 0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS 0x0B96

#define GL_DEPTH_FUNC 0x0B74
#define GL_DEPTH_CLEAR_VALUE 0x0B73
#define GL_DEPTH_TEST 0x0B71

#define GL_CURRENT_PROGRAM 0x8259
#define GL_SHADER_TYPE 0x8B4F
#define GL_DELETE_STATUS 0x8B80
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ATTACHED_SHADERS 0x8B85
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH 0x8B87
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH 0x8B8A
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_CURRENT_VERTEX_ATTRIB 0x8626
#define GL_MAX_VERTEX_ATTRIBS 0x8869

#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_COMPUTE_SHADER 0x91B9
#define GL_TESS_CONTROL_SHADER 0x8E88
#define GL_TESS_EVALUATION_SHADER 0x8E87

#define GL_PROGRAM_POINT_SIZE 0x8642
#define GL_RASTERIZER_DISCARD 0x8C89

#define GL_UNSIGNED_BYTE_3_3_2 0x8032
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#define GL_UNSIGNED_INT_10_10_10_2 0x8036

#define GL_PACK_ALIGNMENT 0x0D05
#define GL_PACK_ROW_LENGTH 0x0D02
#define GL_PACK_SKIP_PIXELS 0x0D04
#define GL_PACK_SKIP_ROWS 0x0D03
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#define GL_UNPACK_SKIP_ROWS 0x0CF3

#define GLFramebuffer 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8CDD

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_VERTEX_ARRAY_BUFFER_BINDING 0x8896
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E5
#define GL_STATIC_READ 0x88E6
#define GL_STATIC_COPY 0x88E7
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA

#define GL_QUERY 0x82E3
#define GL_SAMPLER 0x82E6
#define GL_TRANSFORM_FEEDBACK 0x8E22

#define GL_DEBUG_OUTPUT 0x9244
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_DEPRECATED 0x824D
#define GL_DEBUG_TYPE_UNDEFINED 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP 0x826A
#define GL_DEBUG_TYPE_POP_GROUP 0x8269
#define GL_DEBUG_SOURCE_API 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY 0x824B
#define GL_DEBUG_SOURCE_OTHER 0x8249

typedef void (APIENTRY* GLDEBUGPROC)(unsigned int source, int type, unsigned int id, int severity, int length, const char* message, const void* userParam);

GLAPI int GLAD_GL_VERSION_1_0;
GLAPI int GLAD_GL_VERSION_1_1;
GLAPI int GLAD_GL_VERSION_1_2;
GLAPI int GLAD_GL_VERSION_1_3;
GLAPI int GLAD_GL_VERSION_1_4;
GLAPI int GLAD_GL_VERSION_1_5;
GLAPI int GLAD_GL_VERSION_2_0;
GLAPI int GLAD_GL_VERSION_2_1;
GLAPI int GLAD_GL_VERSION_3_0;
GLAPI int GLAD_GL_VERSION_3_1;
GLAPI int GLAD_GL_VERSION_3_2;
GLAPI int GLAD_GL_VERSION_3_3;
GLAPI int GLAD_GL_VERSION_4_0;
GLAPI int GLAD_GL_VERSION_4_1;
GLAPI int GLAD_GL_VERSION_4_2;
GLAPI int GLAD_GL_VERSION_4_3;
GLAPI int GLAD_GL_VERSION_4_4;
GLAPI int GLAD_GL_VERSION_4_5;
GLAPI int GLAD_GL_VERSION_4_6;

GLAPI void APIENTRY glCullFace(GLenum mode);
GLAPI void APIENTRY glFrontFace(GLenum mode);
GLAPI void APIENTRY glHint(GLenum target, GLenum mode);
GLAPI void APIENTRY glLineWidth(GLfloat width);
GLAPI void APIENTRY glPointSize(GLfloat size);
GLAPI void APIENTRY glPolygonMode(GLenum face, GLenum mode);
GLAPI void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param);
GLAPI void APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params);
GLAPI void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param);
GLAPI void APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint* params);
GLAPI void APIENTRY glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels);
GLAPI void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
GLAPI void APIENTRY glDrawBuffer(GLenum mode);
GLAPI void APIENTRY glClear(GLbitfield mask);
GLAPI void APIENTRY glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
GLAPI void APIENTRY glClearStencil(GLint s);
GLAPI void APIENTRY glClearDepth(GLdouble depth);
GLAPI void APIENTRY glStencilMask(GLuint mask);
GLAPI void APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
GLAPI void APIENTRY glDepthMask(GLboolean flag);
GLAPI void APIENTRY glDisable(GLenum cap);
GLAPI void APIENTRY glEnable(GLenum cap);
GLAPI void APIENTRY glFinish(void);
GLAPI void APIENTRY glFlush(void);
GLAPI void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor);
GLAPI void APIENTRY glLogicOp(GLenum opcode);
GLAPI void APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask);
GLAPI void APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
GLAPI void APIENTRY glDepthFunc(GLenum func);
GLAPI void APIENTRY glPixelStoref(GLenum pname, GLfloat param);
GLAPI void APIENTRY glPixelStorei(GLenum pname, GLint param);
GLAPI void APIENTRY glReadBuffer(GLenum src);
GLAPI void APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
GLAPI void APIENTRY glGetBooleanv(GLenum pname, GLboolean* data);
GLAPI void APIENTRY glGetDoublev(GLenum pname, GLdouble* data);
GLAPI GLenum APIENTRY glGetError(void);
GLAPI void APIENTRY glGetFloatv(GLenum pname, GLfloat* data);
GLAPI void APIENTRY glGetIntegerv(GLenum pname, GLint* data);
GLAPI const GLubyte* APIENTRY glGetString(GLenum name);
GLAPI const GLubyte* APIENTRY glGetStringi(GLenum name, GLuint index);
GLAPI void APIENTRY glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels);
GLAPI void APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params);
GLAPI void APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint* params);
GLAPI void APIENTRY glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params);
GLAPI void APIENTRY glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params);
GLAPI GLboolean APIENTRY glIsEnabled(GLenum cap);
GLAPI void APIENTRY glDepthRange(GLdouble near, GLdouble far);
GLAPI void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count);
GLAPI void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
GLAPI void APIENTRY glEnableVertexAttribArray(GLuint index);
GLAPI void APIENTRY glDisableVertexAttribArray(GLuint index);
GLAPI void APIENTRY glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
GLAPI void APIENTRY glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
GLAPI void APIENTRY glVertexAttribDivisor(GLuint index, GLuint divisor);
GLAPI void APIENTRY glBindBuffer(GLenum target, GLuint buffer);
GLAPI void APIENTRY glDeleteBuffers(GLsizei n, const GLuint* buffers);
GLAPI void APIENTRY glGenBuffers(GLsizei n, GLuint* buffers);
GLAPI GLboolean APIENTRY glIsBuffer(GLuint buffer);
GLAPI void* APIENTRY glMapBuffer(GLenum target, GLenum access);
GLAPI void* APIENTRY glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
GLAPI GLboolean APIENTRY glUnmapBuffer(GLenum target);
GLAPI void APIENTRY glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params);
GLAPI void APIENTRY glGetBufferPointerv(GLenum target, GLenum pname, void** params);
GLAPI void APIENTRY glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void* data);
GLAPI void APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
GLAPI void APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
GLAPI void APIENTRY glActiveTexture(GLenum texture);
GLAPI void APIENTRY glCreateShader(GLenum type);
GLAPI void APIENTRY glDeleteShader(GLuint shader);
GLAPI void APIENTRY glDetachShader(GLuint program, GLuint shader);
GLAPI void APIENTRY glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders);
GLAPI void APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
GLAPI void APIENTRY glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
GLAPI void APIENTRY glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source);
GLAPI GLboolean APIENTRY glIsShader(GLuint shader);
GLAPI void APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
GLAPI void APIENTRY glCompileShader(GLuint shader);
GLAPI GLuint APIENTRY glCreateProgram(void);
GLAPI void APIENTRY glDeleteProgram(GLuint program);
GLAPI void APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint* params);
GLAPI void APIENTRY glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
GLAPI GLboolean APIENTRY glIsProgram(GLuint program);
GLAPI void APIENTRY glLinkProgram(GLuint program);
GLAPI void APIENTRY glUseProgram(GLuint program);
GLAPI void APIENTRY glValidateProgram(GLuint program);
GLAPI void APIENTRY glAttachShader(GLuint program, GLuint shader);
GLAPI void APIENTRY glBindAttribLocation(GLuint program, GLuint index, const GLchar* name);
GLAPI void APIENTRY glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
GLAPI GLint APIENTRY glGetAttribLocation(GLuint program, const GLchar* name);
GLAPI void APIENTRY glGetFragDataLocation(GLuint program, const GLchar* name);
GLAPI void APIENTRY glUniform1f(GLint location, GLfloat v0);
GLAPI void APIENTRY glUniform2f(GLint location, GLfloat v0, GLfloat v1);
GLAPI void APIENTRY glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
GLAPI void APIENTRY glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
GLAPI void APIENTRY glUniform1i(GLint location, GLint v0);
GLAPI void APIENTRY glUniform2i(GLint location, GLint v0, GLint v1);
GLAPI void APIENTRY glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
GLAPI void APIENTRY glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
GLAPI void APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat* value);
GLAPI void APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat* value);
GLAPI void APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat* value);
GLAPI void APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat* value);
GLAPI void APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint* value);
GLAPI void APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint* value);
GLAPI void APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint* value);
GLAPI void APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint* value);
GLAPI void APIENTRY glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GLAPI void APIENTRY glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GLAPI void APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GLAPI void APIENTRY glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GLAPI void APIENTRY glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GLAPI void APIENTRY glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GLAPI void APIENTRY glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GLAPI void APIENTRY glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GLAPI void APIENTRY glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GLAPI void APIENTRY glGetUniformfv(GLuint program, GLint location, GLfloat* params);
GLAPI void APIENTRY glGetUniformiv(GLuint program, GLint location, GLint* params);
GLAPI GLint APIENTRY glGetUniformLocation(GLuint program, const GLchar* name);
GLAPI void APIENTRY glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar* const* uniformNames, GLuint* uniformIndices);
GLAPI void APIENTRY glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformName);
GLAPI void APIENTRY glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName);
GLAPI void APIENTRY glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
GLAPI void APIENTRY glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
GLAPI void APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
GLAPI void APIENTRY glGenFramebuffers(GLsizei n, GLuint* framebuffers);
GLAPI GLenum APIENTRY glCheckFramebufferStatus(GLenum target);
GLAPI void APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer);
GLAPI void APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
GLAPI void APIENTRY glGenRenderbuffers(GLsizei n, GLuint* renderbuffers);
GLAPI void APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer);
GLAPI void APIENTRY glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
GLAPI void APIENTRY glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params);
GLAPI void APIENTRY glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
GLAPI void APIENTRY glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GLAPI void APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GLAPI void APIENTRY glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
GLAPI void APIENTRY glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
GLAPI void APIENTRY glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params);
GLAPI void APIENTRY glGenerateMipmap(GLenum target);
GLAPI void APIENTRY glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
GLAPI void APIENTRY glBindVertexArray(GLuint array);
GLAPI void APIENTRY glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
GLAPI void APIENTRY glGenVertexArrays(GLsizei n, GLuint* arrays);
GLAPI GLboolean APIENTRY glIsVertexArray(GLuint array);
GLAPI void APIENTRY glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
GLAPI void APIENTRY glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);
GLAPI void APIENTRY glDrawTransformFeedback(GLenum mode, GLuint id);
GLAPI void APIENTRY glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream);
GLAPI void APIENTRY glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount);
GLAPI void APIENTRY glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream, GLsizei instancecount);
GLAPI void APIENTRY glGetInteger64i_v(GLenum target, GLuint index, GLint64* data);
GLAPI void APIENTRY glGetInteger64_v(GLenum pname, GLint64* params);
GLAPI void APIENTRY glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64* params);
GLAPI void APIENTRY glGenSamplers(GLsizei n, GLuint* samplers);
GLAPI void APIENTRY glDeleteSamplers(GLsizei n, const GLuint* samplers);
GLAPI GLboolean APIENTRY glIsSampler(GLuint sampler);
GLAPI void APIENTRY glBindSampler(GLuint unit, GLuint sampler);
GLAPI void APIENTRY glSamplerParameteri(GLuint sampler, GLenum pname, GLint param);
GLAPI void APIENTRY glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint* param);
GLAPI void APIENTRY glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param);
GLAPI void APIENTRY glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat* param);
GLAPI void APIENTRY glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint* params);
GLAPI void APIENTRY glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint* params);
GLAPI void APIENTRY glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint* params);
GLAPI void APIENTRY glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint* params);
GLAPI void APIENTRY glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat* params);
GLAPI void APIENTRY glGetSamplerParameterIfv(GLuint sampler, GLenum pname, GLfloat* params);
GLAPI void APIENTRY glBeginTransformFeedback(GLenum primitiveMode);
GLAPI void APIENTRY glEndTransformFeedback(void);
GLAPI void APIENTRY glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar* const* varyings, GLenum bufferMode);
GLAPI void APIENTRY glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name);
GLAPI void APIENTRY glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
GLAPI void APIENTRY glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
GLAPI void APIENTRY glPauseTransformFeedback(void);
GLAPI void APIENTRY glResumeTransformFeedback(void);
GLAPI void APIENTRY glGetFragDataIndex(GLuint program, const GLchar* name);
GLAPI void APIENTRY glPatchParameteri(GLenum pname, GLint value);
GLAPI void APIENTRY glPatchParameterfv(GLenum pname, const GLfloat* values);
GLAPI void APIENTRY glBindTransformFeedback(GLenum target, GLuint id);
GLAPI void APIENTRY glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids);
GLAPI void APIENTRY glGenTransformFeedbacks(GLsizei n, GLuint* ids);
GLAPI GLboolean APIENTRY glIsTransformFeedback(GLuint id);
GLAPI void APIENTRY glPauseTransformFeedback(void);
GLAPI void APIENTRY glResumeTransformFeedback(void);
GLAPI void APIENTRY glBeginQuery(GLenum target, GLuint id);
GLAPI void APIENTRY glEndQuery(GLenum target);
GLAPI void APIENTRY glGetQueryiv(GLenum target, GLenum pname, GLint* params);
GLAPI void APIENTRY glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params);
GLAPI void APIENTRY glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params);
GLAPI void APIENTRY glQueryCounter(GLuint id, GLenum target);
GLAPI void APIENTRY glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params);
GLAPI void APIENTRY glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params);
GLAPI void APIENTRY glDrawArraysIndirect(GLenum mode, const void* indirect);
GLAPI void APIENTRY glDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect);
GLAPI void APIENTRY glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
GLAPI void APIENTRY glBlendFunci(GLuint buf, GLenum src, GLenum dst);
GLAPI void APIENTRY glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
GLAPI void APIENTRY glBlendEquation(GLenum mode);
GLAPI void APIENTRY glBlendEquationi(GLuint buf, GLenum mode);
GLAPI void APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
GLAPI void APIENTRY glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha);
GLAPI void APIENTRY glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
GLAPI void APIENTRY glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
GLAPI void APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask);
GLAPI void APIENTRY glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
GLAPI void APIENTRY glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value);
GLAPI void APIENTRY glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value);
GLAPI void APIENTRY glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value);
GLAPI void APIENTRY glEnablei(GLenum cap, GLuint index);
GLAPI void APIENTRY glDisablei(GLenum cap, GLuint index);
GLAPI void APIENTRY glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance);
GLAPI void APIENTRY glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLuint baseinstance);
GLAPI void APIENTRY glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance);
GLAPI void APIENTRY glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint* params);
GLAPI void APIENTRY glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex, GLenum pname, GLint* params);
GLAPI void APIENTRY glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
GLAPI void APIENTRY glDispatchComputeIndirect(GLintptr indirect);
GLAPI void APIENTRY glMemoryBarrier(GLbitfield barriers);
GLAPI void APIENTRY glMemoryBarrierByRegion(GLbitfield barriers);
GLAPI void APIENTRY glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
GLAPI void APIENTRY glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
GLAPI void APIENTRY glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
GLAPI void APIENTRY glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
GLAPI void APIENTRY glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
GLAPI void APIENTRY glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
GLAPI void APIENTRY glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
GLAPI void APIENTRY glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
GLAPI void APIENTRY glTextureView(GLuint texture, GLenum target, GLuint originaltexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);
GLAPI void APIENTRY glBindTextureUnit(GLuint unit, GLuint texture);
GLAPI void APIENTRY glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels);
GLAPI void APIENTRY glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void* pixels);
GLAPI void APIENTRY glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize, void* pixels);
GLAPI void APIENTRY glCompressedTextureSubImage1D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data);
GLAPI void APIENTRY glCompressedTextureSubImage2D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
GLAPI void APIENTRY glCompressedTextureSubImage3D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
GLAPI void APIENTRY glTextureSubImage1D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels);
GLAPI void APIENTRY glTextureSubImage2D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
GLAPI void APIENTRY glTextureSubImage3D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
GLAPI void APIENTRY glCopyTextureSubImage1D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
GLAPI void APIENTRY glCopyTextureSubImage2D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void APIENTRY glCopyTextureSubImage3D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void APIENTRY glDebugMessageCallback(GLDEBUGPROC callback, const void* userParam);
GLAPI void APIENTRY glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
GLAPI void APIENTRY glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf);
GLAPI GLuint APIENTRY glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog);
GLAPI void APIENTRY glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar* label);
GLAPI void APIENTRY glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar* label);
GLAPI void APIENTRY glObjectPtrLabel(const void* ptr, GLsizei length, const GLchar* label);
GLAPI void APIENTRY glGetObjectPtrLabel(const void* ptr, GLsizei bufSize, GLsizei* length, GLchar* label);
GLAPI void APIENTRY glMaxShaderCompilerThreads(GLuint count);
GLAPI GLuint APIENTRY glGetGraphicsResetStatus(void);
GLAPI void APIENTRY glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void* data);
GLAPI void APIENTRY glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat* params);
GLAPI void APIENTRY glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint* params);
GLAPI void APIENTRY glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint* params);
GLAPI void APIENTRY glMinSampleShading(GLfloat value);
GLAPI void APIENTRY glPointParameterf(GLenum pname, GLfloat param);
GLAPI void APIENTRY glPointParameterfv(GLenum pname, const GLfloat* params);
GLAPI void APIENTRY glPointParameteri(GLenum pname, GLint param);
GLAPI void APIENTRY glPointParameteriv(GLenum pname, const GLint* params);

typedef GLboolean(APIENTRY* GLADloadproc)(const char* name);

GLAPI int GLAD_GL_VERSION_4_6;
GLAPI int gladLoadGLLoader(GLADloadproc load);

#endif /* GLAD_GL_H_ */
