#pragma once

#include <cstdint>

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define GLAPI WINAPI
#pragma comment(lib, "opengl32.lib")
#endif //_WIN32
#include <GL/gl.h>

using GLchar = char;
using GLubyte = uint8_t;
using GLfixed = int32_t;
using GLint64 = int64_t;
using GLuint64 = uint64_t;
using GLintptr = intptr_t;
using GLsizeiptr = ptrdiff_t;

typedef void (GLAPI DEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, void *userParam);

/*Constants obtained from https://www.opengl.org/registry/api/GL/glext.h */
#define GL_HALF_FLOAT                     0x140B
#define GL_INT_2_10_10_10_REV             0x8D9F
#define GL_UNSIGNED_INT_2_10_10_10_REV    0x8368
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_COMPUTE_SHADER                 0x91B9
#define GL_GEOMETRY_SHADER                0x8DD9
#define GL_ACTIVE_UNIFORMS                0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH      0x8B87
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_ARRAY_BUFFER_BINDING           0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING   0x8895
#define GL_FUNC_ADD                       0x8006
#define GL_TEXTURE0                       0x84C0
#define GL_CONSTANT_COLOR                 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR       0x8002
#define GL_CONSTANT_ALPHA                 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA       0x8004
#define GL_SRC1_COLOR                     0x88F9
#define GL_SRC1_ALPHA                     0x8589
#define GL_ONE_MINUS_SRC1_COLOR           0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA           0x88FB
#define GL_FUNC_SUBTRACT                  0x800A
#define GL_FUNC_REVERSE_SUBTRACT          0x800B
#define GL_MIN                            0x8007
#define GL_MAX                            0x8008
#define GL_PATCHES                        0x000E
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242
#define GL_DEBUG_SEVERITY_HIGH            0x9146
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#define GL_DEBUG_SEVERITY_LOW             0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION    0x826B
#define GL_DEBUG_SOURCE_API               0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM     0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER   0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY       0x8249
#define GL_DEBUG_SOURCE_APPLICATION       0x824A
#define GL_DEBUG_SOURCE_OTHER             0x824B
#define GL_COMPILE_STATUS                 0x8B81
#define GL_FLOAT_VEC2                     0x8B50
#define GL_FLOAT_VEC3                     0x8B51
#define GL_FLOAT_VEC4                     0x8B52
#define GL_INT_VEC2                       0x8B53
#define GL_INT_VEC3                       0x8B54
#define GL_INT_VEC4                       0x8B55
#define GL_BOOL                           0x8B56
#define GL_BOOL_VEC2                      0x8B57
#define GL_BOOL_VEC3                      0x8B58
#define GL_BOOL_VEC4                      0x8B59
#define GL_FLOAT_MAT2                     0x8B5A
#define GL_FLOAT_MAT3                     0x8B5B
#define GL_FLOAT_MAT4                     0x8B5C
#define GL_SAMPLER_1D                     0x8B5D
#define GL_SAMPLER_2D                     0x8B5E
#define GL_SAMPLER_3D                     0x8B5F
#define GL_SAMPLER_CUBE                   0x8B60
#define GL_SHADER_STORAGE_BUFFER          0x90D2
#define GL_ATOMIC_COUNTER_BUFFER          0x92C0
#define GL_SHADER_STORAGE_BARRIER_BIT     0x00002000
#define GL_UNIFORM_BUFFER                 0x8A11
#define GL_UNIFORM_BUFFER_BINDING         0x8A28
#define GL_UNIFORM_BUFFER_START           0x8A29
#define GL_UNIFORM_BUFFER_SIZE            0x8A2A
#define GL_COPY_READ_BUFFER               0x8F36
#define GL_COPY_WRITE_BUFFER              0x8F37

/*
Define X-macro of opengl functions to load.
order is ret, name, args...
*/
#define GL_FUNCTION_LIST \
	/*Uniforms*/ \
	GLX(void, Uniform1iv, GLint location, GLsizei count, const GLint *value) \
	GLX(void, Uniform1uiv, GLint location, GLsizei count, const GLuint *value) \
	GLX(void, Uniform1fv, GLint location, GLsizei count, const GLfloat *value) \
	GLX(void, Uniform2fv, GLint location, GLsizei count, const GLfloat *value) \
	GLX(void, Uniform2iv, GLint location, GLsizei count, const GLint *value) \
	GLX(void, Uniform3fv, GLint location, GLsizei count, const GLfloat *value) \
	GLX(void, Uniform3iv, GLint location, GLsizei count, const GLint *value) \
	GLX(void, Uniform4fv, GLint location, GLsizei count, const GLfloat *value) \
	GLX(void, Uniform4iv, GLint location, GLsizei count, const GLint *value) \
	GLX(void, UniformMatrix3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) \
	GLX(void, UniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) \
	/*Shader functions*/ \
	GLX(GLuint, CreateShader, GLenum shader_type) \
	GLX(void, ShaderSource, GLuint shader, GLsizei count, const GLchar **string, const GLint *length) \
	GLX(void, CompileShader, GLuint shader) \
	GLX(GLuint, CreateProgram) \
	GLX(void, AttachShader, GLuint program, GLuint shader) \
	GLX(void, LinkProgram, GLuint program) \
	GLX(void, DeleteShader, GLuint shader) \
	GLX(void, GetProgramiv, GLuint program, GLenum pname, GLint *params) \
	GLX(void, GetActiveUniform, GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) \
	GLX(GLint, GetUniformLocation, GLuint program, const GLchar* name) \
	GLX(void, UseProgram, GLuint program) \
	GLX(void, GetShaderiv, GLuint shader, GLenum param, GLint* params)\
	GLX(void, GetShaderInfoLog, GLuint shader, GLsizei maxLength, GLsizei * length, GLchar* infoLog )\
	GLX(void, DeleteProgram, GLuint program)\
	/*Buffer functions*/ \
	GLX(void, GenBuffers, GLsizei n, GLuint* buffers) \
	GLX(void, BindBuffer, GLenum target, GLuint buffer) \
	GLX(void, BufferData, GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage) \
	GLX(void, BufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) \
	GLX(void, BindVertexArray, GLuint vao) \
	GLX(void, GenVertexArrays, GLuint n, GLuint* vaos) \
	GLX(void, DeleteBuffers, GLuint n, GLuint* buffers) \
	GLX(void, BindBufferBase, GLenum target, GLuint index, GLuint buffer) \
	GLX(void, BindBufferRange, GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)\
	/*Misc functions*/ \
	GLX(void, BlendEquation, GLenum eq) \
	GLX(void, ActiveTexture, GLenum texture) \
	GLX(void, DispatchCompute, GLuint x, GLuint y, GLuint z) \
	GLX(GLint, GetAttribLocation, GLuint program, const GLchar* name) \
	GLX(void, VertexAttribPointer, 	GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer) \
	GLX(void, EnableVertexAttribArray, GLuint attrib) \
	GLX(void, DrawElementsBaseVertex, GLenum mode, GLsizei count, GLenum type, GLvoid *indices, GLint basevertex) \
	GLX(void, DebugMessageCallback, DEBUGPROC callback, void * userParam)\
	GLX(void, MemoryBarrier, GLbitfield region)
//TODO: Finish up this list. Want to remove GLEW as a dependency.

#define GLX(ret, name, ...) typedef ret GLAPI name##proc(__VA_ARGS__); name##proc * gl##name;
GL_FUNCTION_LIST
#undef GLX


inline bool LoadGLFunctions()
{
#ifdef _WIN32
	auto dll = LoadLibraryA("opengl32.dll");
	typedef PROC WINAPI wglGetProcAddressProc(LPCSTR lpszProc);
	if (!dll) {
		OutputDebugStringA("OpenGL dll not found!\n");
		return false;
	}
	auto wglGetProcAddress = (wglGetProcAddressProc*)GetProcAddress(dll, "wglGetProcAddress");

#define GLX(ret, name, ...) \
		gl##name = (name##proc *) wglGetProcAddress("gl"#name); \
		if (!gl##name) { \
			OutputDebugStringA("OpenGl function gl" #name " couldn't be loaded."); \
			return false; \
		}

	GL_FUNCTION_LIST
#undef GLX

#else 
#error "Open GL loading not implemented for this platform yet."
#endif //_Win32



	return true;
}