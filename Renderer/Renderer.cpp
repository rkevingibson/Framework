
#include "Renderer.h"
#include <cassert>
#include <cstdio>
#include <type_traits>
#include <array>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <cstddef>
#include <algorithm>
#include "../Utilities/Utilities.h"
#include "../Utilities/MurmurHash.h"

using namespace render;



#pragma region OpenGL Information
#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define GLAPI WINAPI
#pragma comment(lib, "opengl32.lib")
#endif //_WIN32


#include <GL/gl.h>

namespace {
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
	/*Buffer functions*/ \
	GLX(void, GenBuffers, GLsizei n, GLuint* buffers) \
	GLX(void, BindBuffer, GLenum target, GLuint buffer) \
	GLX(void, BufferData, GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage) \
	GLX(void, BufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) \
	GLX(void, BindVertexArray, GLuint vao) \
	GLX(void, GenVertexArrays, GLuint n, GLuint* vaos) \
	/*Misc functions*/ \
	GLX(void, BlendEquation, GLenum eq) \
	GLX(void, ActiveTexture, GLenum texture) \
	GLX(void, DispatchCompute, GLuint x, GLuint y, GLuint z) \
	GLX(GLint, GetAttribLocation, GLuint program, const GLchar* name) \
	GLX(void, VertexAttribPointer, 	GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer) \
	GLX(void, EnableVertexAttribArray, GLuint attrib) \
	GLX(void, DrawElementsBaseVertex, GLenum mode, GLsizei count, GLenum type, GLvoid *indices, GLint basevertex) \
	GLX(void, DebugMessageCallback, DEBUGPROC callback, void * userParam)
	//TODO: Finish up this list. Want to remove GLEW as a dependency.

#define GLX(ret, name, ...) typedef ret GLAPI name##proc(__VA_ARGS__); name##proc * gl##name;
	GL_FUNCTION_LIST
#undef GLX


	bool LoadGLFunctions()
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

}
#pragma endregion


namespace {
#pragma region Utility Containers

	template<uint32_t Size>
	class GLCache
	{
		/*Simple and fast hash map. Doesn't resolve colissions -
		assumes keys are unique, otherwise will fail in weird ways.
		TODO: Change this up to evict old data.
		*/
	public:
		void Add(uint32_t hash, GLuint val)
		{
			uint32_t loc = hash % Size;
			index_chain_[val] = hash_table_[loc];
			hash_table_[loc].hash = hash;
			hash_table_[loc].index = val;
		}

		GLuint Get(uint32_t hash) const
		{
			uint32_t loc = hash % Size;
			if (hash_table_[loc].hash == hash) {
				return hash_table_[loc].index;
			}
			else {
				loc = hash_table_[loc].index;
				while (loc != INVALID_INDEX && index_chain_[loc].hash != hash) {
					loc = index_chain_[loc].index;
				}
				return loc;
			}
		}

		void Clear()
		{
			for (auto& p : hash_table_) {
				p.index = INVALID_INDEX;
			}
			for (auto& p : index_chain_) {
				p.index = INVALID_INDEX;
			}
		}

		static constexpr GLuint INVALID_INDEX{ 0xFFFFFFFF };
	private:
		struct ValuePair
		{
			uint32_t hash{ INVALID_INDEX };
			GLuint index{ INVALID_INDEX };
		};

		std::array<ValuePair, Size> hash_table_;
		std::array<ValuePair, Size> index_chain_;
	};

	template<uint32_t Size>
	class RawBuffer
	{
		//NOTE: Should look at error handling here, not sure the best approach when the buffer is full.
	public:
		uint32_t Write(const void* data, const uint32_t num)
		{
			Expects(current_pos_ + num < Size);
			memcpy(&buffer_[current_pos_], data, num);
			current_pos_ += num;
			return current_pos_;
		}

		template<typename T>
		T Read()
		{
			static_assert(std::is_arithmetic<T>::value, "Requires arithmetic type.");
			Expects(current_pos_ + sizeof(T) < Size && "Insufficient data to read type");
			auto result = reinterpret_cast<T*>(&buffer_[current_pos_]);
			current_pos_ += sizeof(T);
			return *result;
		}

		byte* GetPtr() { return &buffer_[current_pos_]; }

		void Seek(const uint32_t pos)
		{
			Expects(pos < Size);
			current_pos_ = pos;
		}

		void Skip(const uint32_t num)
		{
			current_pos_ += num;
		}
		uint32_t GetPos() { return current_pos_; }

		void Clear()
		{
			current_pos_ = 0;
			memset(buffer_.data(), 0, buffer_.size());
		}
	private:
		uint32_t current_pos_;
		std::array<byte, Size> buffer_;
	};

	template<typename T, uint32_t Size>
	class Stack
	{
	public:
		struct ResultPair
		{
			uint32_t index;
			T* obj;
		};

		ResultPair Push()
		{
			auto index = pos_.fetch_add(1);
			return ResultPair{ index, &data_[index] };
		}

		inline T& operator[](uint32_t index)
		{
			return data_[index];
		}

		void Clear()
		{
			pos_ = 0;
			memset(data_, 0, sizeof(T)*Size);
		}

	private:
		T data_[Size];
		std::atomic<uint32_t> pos_{ 0 };
	};
#pragma endregion

#pragma region Data Structures


struct Key {
	//Sorting key for draw calls. Let's us reorder draws to minimize state change.	
	
	/*
		Draw Key layout:


		      x38      x30      x28      x20       x18      x10       x8       x0
		765543210.76543210.76543210.76543210  76543210.76543210.76543210.76543210
		bbbbbbbbb csssssss sssspppp pppppppp  ddddddddddddddddddddddddddddddddddd
		
		b - bucket - corresponds to the view/renderbuffer.
		c - compute - set if the key is a compute shader key. Reserved right now.
		s - sequence - order of draw in the frame. 0 by default, right now no support for sequenced rendering.
		p - program - the render program
		d - depth
	*/
	
	uint64_t Encode() {
		uint64_t result = 0 |
			((uint64_t) bucket << 56) |
			(((uint64_t) sequence & 0x7FF) << 44) |
			(((uint64_t) program & 0xFFF) << 32) |
			depth;
		return result;
	}
	static Key Decode(uint64_t k) {
		Key result;
		result.bucket = (k >> 0x38);
		result.compute = (k >> 0x37) & 0x1;
		result.sequence = (k >> 0x2C) & 0x7FF;
		result.program = (k >> 0x20) & 0xFFF;
		result.depth = k & 0xFFFFFFFF;
		return result;
	}
	uint8_t	bucket : 8;
	bool compute : 1;
	uint16_t	sequence : 11;
	uint16_t	program : 12;
	uint32_t	depth;
};

struct RenderLayer 
{
	GLuint	framebuffer;
	bool	sequential;
};

struct VertexBuffer
{
	GLuint buffer;
	uint32_t size;
	VertexLayout layout;
};

struct IndexBuffer
{
	GLuint	buffer;
	uint32_t	num_elements;
	GLenum	type;
};

struct Texture 
{
	uint16_t width, height;
	TextureFormat format;
	GLuint name;
	GLenum target;
};

struct Uniform {
	uint32_t hash;//Hash of the uniforms name.
	UniformType type;
#ifdef RENDER_DEBUG
	char name[64];
#endif
};

struct Program 
{
	GLuint	id;
	GLCache<MAX_UNIFORMS> uniforms;
};

class UniformBuffer;

struct RenderCmd 
{
	ProgramHandle program{ INVALID_HANDLE };
	//Uniform updates.
	UniformBuffer* uniform_buffer;
	uint32_t uniform_start;
	uint32_t uniform_end;

	//Textures
	TextureHandle textures[MAX_TEXTURE_UNITS];
};

struct DrawCmd : public RenderCmd
{
	//Store everything needed to make any type of draw call
	DrawCmd() 
	{
		for (int i = 0; i < MAX_TEXTURE_UNITS; i++) {
			textures[i].index = INVALID_HANDLE;
		}
		scissor[0] = 0;
		scissor[1] = 0;
		scissor[2] = UINT32_MAX;
		scissor[3] = UINT32_MAX;
	}

	uint64_t	render_state	{ RenderState::DEFAULT_STATE };
	uint32_t	vertex_buffer	{ INVALID_HANDLE };
	uint32_t	index_buffer	{ INVALID_HANDLE };

	uint32_t vertex_offset;
	uint32_t vertex_count;

	uint32_t index_offset;
	uint32_t index_count;
	

	//Scissor settings
	uint32_t scissor[4];
};

struct ComputeCmd : public RenderCmd
{
	uint32_t x, y, z;
};

#pragma endregion
//I need some queues. First off, a queue for the draw cmds. 
//These are double-buffered, to allow submission while rendering.
uint64_t frame;


struct EncodedKey{
	uint64_t key;
	RenderCmd* cmd;
};

#pragma region Per-Frame Data

std::array<EncodedKey, MAX_DRAWS_PER_FRAME>	keys[2];
thread_local DrawCmd current_draw;
thread_local int render_buffer_count[2]{ 0,0 };
thread_local std::array<DrawCmd, MAX_DRAWS_PER_THREAD> render_buffer[2];

#pragma endregion

std::atomic<unsigned int> key_index[2]{ 0,0 };
std::shared_mutex render_buffer_mutex;
std::atomic<unsigned int> front_buffer{ 0 };//Which of the current buffers are you submitting to this frame.

void SortKeys(unsigned int buffer)
{
	//NB: This function only gets called from the render function, so front_buffer is guaranteed not to switch during execution, 
	//And the arrays won't be written to at all.
	auto key_list = &keys[buffer];
	unsigned int num_keys = key_index[buffer];
	//How to do this? Need to sort in place.
	//Could use std::sort, but I would need to move the key/draw stuff into a single list.
	//Why are they in separate lists? Because once theyre sorted 
	//I don't care about the keys really... but I kind of do. 
	std::sort(key_list->begin(), key_list->begin() + num_keys, [](const EncodedKey& a, const EncodedKey& b) { return a.key < b.key; });
}

//These buffers manage resources which live across many frames.
//Stack really isn't appropriate for these... Really want an allocator or something.
Stack<RenderLayer, MAX_RENDER_LAYERS>	layers;
Stack<VertexBuffer, MAX_VERTEX_BUFFERS>	vertex_buffers;
Stack<IndexBuffer, MAX_INDEX_BUFFERS>	index_buffers;
Stack<Texture, MAX_TEXTURES>	textures;
Stack<Uniform, MAX_UNIFORMS> uniforms;
Stack<Program, MAX_SHADER_PROGRAMS>	programs;

//TODO: Implement this stuff
GLuint transient_vertex_buffer;
GLuint transient_index_buffer;

/*==================== Pre-rendering Command Buffer ====================*/

struct Cmd 
{
	using DispatchFn = void(*)(Cmd*);
	DispatchFn dispatch;
};

class CommandBuffer {
public:

template<typename T>
inline T* Add() {
	static_assert(std::is_base_of<Cmd, T>::value, "Adding invalid command");
	std::lock_guard<std::mutex> lock(add_mutex_);
	auto i = index_;
	index_ += sizeof(T);
	auto count = count_;
	count_++;
	ASSERT(i < buffer_.size());//If not, we're out of room.
	auto cmd = reinterpret_cast<T*>(&buffer_[i]);
	cmd->dispatch = T::DISPATCH;
	cmd_indices_[count] = i;
	return cmd;
}

void Execute() {
	std::lock_guard<std::mutex> lock(add_mutex_);//Shouldn't let anyone write commands now.
	for (unsigned int i = 0; i < count_; ++i) {
		Cmd* cmd = reinterpret_cast<Cmd*>(&buffer_[cmd_indices_[i]]);
		cmd->dispatch(cmd);
	}
	count_ = 0;
	index_ = 0;
}

private:
	std::mutex add_mutex_;
	unsigned int count_;
	unsigned int index_;
	std::array<unsigned int, KILO(1)> cmd_indices_;
	std::array<byte, KILO(4)> buffer_;//Allocate a 4k static buffer
};

CommandBuffer pre_buffer;

class UniformBuffer
{
	/*
	Stores uniform data/instructions in the following packet structure
	2 bytes - uniform handle index.
	1 byte - uniform type
	1 byte - number
	x bytes - payload.
	*/
public:
	bool Add(UniformHandle handle, UniformType type, uint8_t num, const void* data)
	{
		//TODO: Check to make sure the write is successful.
		buffer_.Write(&handle.index, sizeof(handle.index));
		auto type_num = static_cast<uint8_t>(type);
		buffer_.Write(&type_num, sizeof(type_num));
		buffer_.Write(&num, sizeof(num));
		auto type_size = Size(type);
		buffer_.Write(data, type_size*num);

		return true;
	}

	void Execute(Program* program, uint32_t start, uint32_t end)
	{
		buffer_.Seek(start);
		while (buffer_.GetPos() < end) {
			auto handle = buffer_.Read<decltype(UniformHandle::index)>();
			auto type_num = buffer_.Read<uint8_t>();
			auto num = buffer_.Read<uint8_t>();
			const byte* payload = buffer_.GetPtr();

			auto hash = uniforms[handle].hash;
			GLint location = program->uniforms.Get(hash);
			UniformType type = static_cast<UniformType>(type_num);
			
			auto payload_size = Size(type)*num;
			buffer_.Skip(payload_size);

			switch (type)
			{
			case render::UniformType::Sampler:
			case render::UniformType::Int:
				glUniform1iv(location, num, reinterpret_cast<const GLint*>(payload));
				break;
			case render::UniformType::Uint:
				glUniform1uiv(location, num, reinterpret_cast<const GLuint*>(payload));
				break;
			case render::UniformType::Float:
				glUniform1fv(location, num, reinterpret_cast<const GLfloat*>(payload));
				break;
			case render::UniformType::Vec2:
				glUniform2fv(location, num, reinterpret_cast<const GLfloat*>(payload));
				break;
			case render::UniformType::Ivec2:
				glUniform2iv(location, num, reinterpret_cast<const GLint*>(payload));
				break;
			case render::UniformType::Vec3:
				glUniform3fv(location, num, reinterpret_cast<const GLfloat*>(payload));
				break;
			case render::UniformType::Ivec3:
				glUniform3iv(location, num, reinterpret_cast<const GLint*>(payload));
				break;
			case render::UniformType::Vec4:
				glUniform4fv(location, num, reinterpret_cast<const GLfloat*>(payload));
				break;
			case render::UniformType::Ivec4:
				glUniform4iv(location, num, reinterpret_cast<const GLint*>(payload));
				break;
			case render::UniformType::Mat3:
				glUniformMatrix3fv(location, num, false, reinterpret_cast<const GLfloat*>(payload));
				break;
			case render::UniformType::Mat4:
				glUniformMatrix4fv(location, num, false, reinterpret_cast<const GLfloat*>(payload));
				break;
			default:
				break;
			}
		}
	}

	void Clear()
	{
		buffer_.Clear();
	}

	uint32_t GetPosition() { return buffer_.GetPos(); }
private:
	uint32_t Size(UniformType type) {
		uint32_t type_size;
		switch (type)
		{
		case render::UniformType::Sampler:
		case render::UniformType::Int:
		case render::UniformType::Uint:
		case render::UniformType::Float:
			type_size = 4;
			break;
		case render::UniformType::Vec2:
		case render::UniformType::Ivec2:
			type_size = 8;
			break;
		case render::UniformType::Vec3:
		case render::UniformType::Ivec3:
			type_size = 12;
			break;
		case render::UniformType::Vec4:
		case render::UniformType::Ivec4:
			type_size = 16;
			break;
		case render::UniformType::Mat3:
			type_size = 9 * 4;
			break;
		case render::UniformType::Mat4:
			type_size = 16 * 4;
			break;
		default:
			break;
		}
		return type_size;
	}

	RawBuffer<MEGA(2)> buffer_;
};

thread_local UniformBuffer uniform_buffer[2];
} //End anonymous namespace.

#pragma region VertexLayout functions
VertexLayout & VertexLayout::Add(const char* name, uint16_t num, AttributeType::Enum type, bool normalized)
{
	Expects(type < AttributeType::Count);
	
	/*
		Pack attribute info into 8 bits:
		76543210
		xnttttss
		x - reserved
		n - normalized
		t - type
		s - number
	*/
	attribs[num_attributes] = 0 |
		((normalized << 6) & 0x40)|
		((type << 2) & 0x3C) |
		((num - 1) & 0x03);
	offset[num_attributes] = stride;
	strcpy_s(names[num_attributes], MAX_ATTRIBUTE_NAME_LENGTH, name);
	//Need to compute type size:
	uint8_t size = 4;
	if (type == AttributeType::Int8 || type == AttributeType::Uint8) {
		size = 1;
	}
	else if (type == AttributeType::Int16 || type == AttributeType::Uint16 || type == AttributeType::Float16) {
		size = 2;
	}
	else if (type == AttributeType::Float64) {
		size = 8;
	}

	stride += size * num;

	num_attributes++;
	return *this;
}

VertexLayout & VertexLayout::Clear()
{
	for (int i = 0; i < MAX_ATTRIBUTES; ++i) {
		offset[i] = 0;
		attribs[i] = 0;
		memset(names[i], 0, MAX_ATTRIBUTE_NAME_LENGTH);
	}
	stride = 0;
	num_attributes = 0;
	return *this;
}

namespace {
	GLenum GetGLEnum(VertexLayout::AttributeType::Enum val)
	{
		switch (val)
		{
		case render::VertexLayout::AttributeType::Int8:
			return GL_BYTE;
			break;
		case render::VertexLayout::AttributeType::Uint8:
			return GL_UNSIGNED_BYTE;
			break;
		case render::VertexLayout::AttributeType::Int16:
			return GL_SHORT;
			break;
		case render::VertexLayout::AttributeType::Uint16:
			return GL_UNSIGNED_SHORT;
			break;
		case render::VertexLayout::AttributeType::Float16:
			return GL_HALF_FLOAT;
			break;
		case render::VertexLayout::AttributeType::Int32:
			return GL_INT;
			break;
		case render::VertexLayout::AttributeType::Uint32:
			return GL_UNSIGNED_INT;
			break;
		case render::VertexLayout::AttributeType::Packed_2_10_10_10_REV:
			return GL_INT_2_10_10_10_REV;
			break;
		case render::VertexLayout::AttributeType::UPacked_2_10_10_10_REV:
			return GL_UNSIGNED_INT_2_10_10_10_REV;
			break;
		case render::VertexLayout::AttributeType::Float32:
			return GL_FLOAT;
			break;
		case render::VertexLayout::AttributeType::Float64:
			return GL_DOUBLE;
			break;
		default:
			//Error, somethings gone wrong.
			return 0;
			break;
		}
	}
}

#pragma endregion


#pragma region Debug Functions
namespace {

	//TODO: Maybe allow the user to set an error callback, given them a nicely formatted error.
	void GLAPI GLErrorCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, void* userParam)
	{
		if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
			return;

		switch (severity)
		{
		case GL_DEBUG_SEVERITY_HIGH:
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			break;
		case GL_DEBUG_SEVERITY_LOW:
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			break;
		default:
			break;
		}

		switch (source)
		{
		case GL_DEBUG_SOURCE_API:
			break;
		case GL_DEBUG_SOURCE_APPLICATION:
			break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER:
			break;
		default:
			break;
		}

		printf(message);
	}
}

#pragma endregion

void render::Initialize()
{
	LoadGLFunctions();

#ifdef RENDER_DEBUG
	glDebugMessageCallback(GLErrorCallback, nullptr);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	CreateLayer();
	return;
}

namespace {
	void Resize(Cmd*);
	struct ResizeCmd : Cmd {
		static constexpr DispatchFn DISPATCH{ Resize };
		int w, h;
	};

	void Resize(Cmd* cmd)
	{
		auto data = reinterpret_cast<ResizeCmd*>(cmd);
		glViewport(0, 0, data->w, data->h);
	}
}

void render::Resize(int w, int h)
{
	auto cmd = pre_buffer.Add<ResizeCmd>();
	cmd->w = w;
	cmd->h = h;
}

#pragma region Memory Management
/*==================== MEMORY MANAGEMENT ====================*/
namespace {
	std::array<MemoryBlock, 2048> blocks;
	std::atomic<uint32_t> num_blocks{ 0 };

	void FreeMemory()
	{
		for (int i = 0; i < 2048; i++) {
			
			if (blocks[i].size != 0) {
				free(blocks[i].data);
			}
			blocks[i].size = 0;
		}
		num_blocks.store(0);
	}
}

const MemoryBlock* render::Alloc(const uint32_t size)
{
	//For now, just allocate off the heap. 
	//Later on, will have something better maybe.
	auto data = malloc(size);
	if (data == nullptr) {
		return  nullptr;
	}

	//TODO: Handle possibility of running out of memory blocks to allocate.
	auto block_index = num_blocks.fetch_add(1);
	blocks[block_index].data = data;
	blocks[block_index].size = size;
	return &blocks[block_index];
}

const MemoryBlock* render::AllocAndCopy(const void * const data, const uint32_t size)
{
	auto block = Alloc(size);
	memcpy(block->data, data, size);
	return block;
}

const MemoryBlock* render::LoadShaderFile(const char* file)
{
	using namespace std;
	FILE* f;
	fopen_s(&f, file, "rb");
	if (!f) {
		//File open failed!
		return nullptr;
	}
	fseek(f, 0, SEEK_END);
	auto len = ftell(f);
	auto block = Alloc(len);
	rewind(f);
	fread(block->data, 1, len, f);
	fclose(f);
	return block;
}

#pragma endregion
/*==================== RESOURCE MANAGEMENT ====================*/

LayerHandle render::CreateLayer()
{
	auto layer = layers.Push();
	LayerHandle result{ layer.index };
	*layer.obj = RenderLayer{};
	return result;
}


namespace {
	void CreateProgram(Cmd* cmd);
	struct CreateProgramCmd : Cmd {
		static constexpr DispatchFn DISPATCH = { CreateProgram };
		uint32_t index;
		const MemoryBlock* vert_shader;
		const MemoryBlock* frag_shader;
	};
	void CreateProgram(Cmd* cmd)
	{
		auto data = reinterpret_cast<CreateProgramCmd*>(cmd);
		auto vert = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vert, 1, (const GLchar**)&data->vert_shader->data, nullptr);
		glCompileShader(vert);

		auto frag = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(frag, 1, (const GLchar**)&data->frag_shader->data, nullptr);
		glCompileShader(frag);

		auto program = glCreateProgram();
		glAttachShader(program, vert);
		glAttachShader(program, frag);
		glLinkProgram(program);
		glDeleteShader(vert);
		glDeleteShader(frag);

		//Get uniform information.
		{
			GLint num_uniforms, uniform_name_buffer_size;
			glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &num_uniforms);
			constexpr size_t UNIFORM_NAME_BUFFER_SIZE = 128;
			GLchar uniform_name_buffer[UNIFORM_NAME_BUFFER_SIZE];

#ifdef RENDER_DEBUG
			glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_name_buffer_size);
			uniform_name_buffer_size++;
			ASSERT(uniform_name_buffer_size < UNIFORM_NAME_BUFFER_SIZE);
#endif
			for (GLint i = 0; i < num_uniforms; ++i) {
				GLint size;
				GLsizei length;
				GLenum type;
				//NOTE:CHECK THIS FUNCTION. WROTE WITHOUT DOCS.
				glGetActiveUniform(program, i, UNIFORM_NAME_BUFFER_SIZE, &length,
					&size, &type, uniform_name_buffer);
				auto loc = glGetUniformLocation(program, uniform_name_buffer);
				rkg::MurmurHash murmur;
				murmur.Add(uniform_name_buffer, length);
				programs[data->index].uniforms.Add(murmur.Finish(), loc);
			}
		}

		programs[data->index].id = program;
	}

}

ProgramHandle render::CreateProgram(const MemoryBlock* vertex_shader, const MemoryBlock* frag_shader)
{
	auto program = programs.Push();
	ProgramHandle result{ program.index };
	
	auto cmd = pre_buffer.Add<CreateProgramCmd>();
	cmd->vert_shader = vertex_shader;
	cmd->frag_shader = frag_shader;
	cmd->index = program.index;
	
	return result;
}

#pragma region Vertex Buffer Functions

namespace {
	void CreateVertexBuffer(Cmd* cmd);
	struct CreateVertexBufferCmd : Cmd
	{
		uint32_t index;
		static constexpr DispatchFn DISPATCH = { CreateVertexBuffer };
		const MemoryBlock* block;
	};
	void CreateVertexBuffer(Cmd* cmd)
	{
		auto data = reinterpret_cast<CreateVertexBufferCmd*>(cmd);
		GLuint vb;
		glGenBuffers(1, &vb);
		glBindBuffer(GL_ARRAY_BUFFER, vb);
		glBufferData(GL_ARRAY_BUFFER, data->block->size, data->block->data, GL_STATIC_DRAW);
		vertex_buffers[data->index].buffer = vb;
		vertex_buffers[data->index].size = data->block->size;
	}
}

VertexBufferHandle render::CreateVertexBuffer(const MemoryBlock* data, const VertexLayout& layout)
{
	auto vb = vertex_buffers.Push();
	VertexBufferHandle result{ vb.index };

	auto cmd = pre_buffer.Add<CreateVertexBufferCmd>();
	cmd->block = data;
	cmd->index = vb.index;
	//NOTE: It might be better to move this stuff to the CmdBuffer. A little more memory being copied for a bit fewer memory accesses.
	vb.obj->layout = layout;
	return result;
}

namespace {

	void CreateDynamicVertexBuffer(Cmd* cmd);
	struct CreateDynamicVertexBufferCmd : Cmd
	{
		uint32_t index;
		static constexpr DispatchFn DISPATCH = { CreateDynamicVertexBuffer };
		const MemoryBlock* block;
	};
	void CreateDynamicVertexBuffer(Cmd* cmd)
	{
		auto data = reinterpret_cast<CreateDynamicVertexBufferCmd*>(cmd);
		GLuint vb;
		glGenBuffers(1, &vb);
		glBindBuffer(GL_ARRAY_BUFFER, vb);
		if (data->block != nullptr) {
			glBufferData(GL_ARRAY_BUFFER, data->block->size, data->block->data, GL_DYNAMIC_DRAW);
		}
		vertex_buffers[data->index].buffer = vb;
		vertex_buffers[data->index].size = (data->block == nullptr) ? 0 : data->block->size;
	}

}

DynamicVertexBufferHandle render::CreateDynamicVertexBuffer(const MemoryBlock* data, const VertexLayout& layout)
{
	auto vb = vertex_buffers.Push();
	DynamicVertexBufferHandle result{ vb.index };
	auto cmd = pre_buffer.Add<CreateDynamicVertexBufferCmd>();
	cmd->block = data;
	cmd->index = vb.index;

	vb.obj->layout = layout;
	return result;
}

DynamicVertexBufferHandle render::CreateDynamicVertexBuffer(const VertexLayout& layout)
{
	return CreateDynamicVertexBuffer(nullptr, layout);
}

namespace {
	void UpdateDynamicVertexBuffer(Cmd* cmd);
	struct UpdateDynamicVertexBufferCmd : Cmd
	{
		static constexpr DispatchFn DISPATCH = { UpdateDynamicVertexBuffer };
		DynamicVertexBufferHandle vb;
		GLintptr offset;
		const MemoryBlock* block;

	};
	void UpdateDynamicVertexBuffer(Cmd* cmd)
	{
		auto data = reinterpret_cast<UpdateDynamicVertexBufferCmd*>(cmd);

		GLint prev_buffer;
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_buffer);

		auto& vb = vertex_buffers[data->vb.index];
		glBindBuffer(GL_ARRAY_BUFFER, vb.buffer);
		if (vb.size < data->offset + data->block->size) {
			//TODO: Right now, this breaks the use of offset, by not copying the data over.
			glBufferData(GL_ARRAY_BUFFER, data->offset + data->block->size, 0, GL_DYNAMIC_DRAW);
		}

		glBufferSubData(GL_ARRAY_BUFFER, data->offset, data->block->size, data->block->data);
		vb.size = data->block->size;

		glBindBuffer(GL_ARRAY_BUFFER, prev_buffer);
	}

}

void render::UpdateDynamicVertexBuffer(DynamicVertexBufferHandle handle, const MemoryBlock* data, const ptrdiff_t offset)
{
	auto cmd = pre_buffer.Add<UpdateDynamicVertexBufferCmd>();
	cmd->block = data;
	cmd->offset = offset;
	cmd->vb = handle;
}

#pragma endregion

#pragma region Index Buffer Functions

namespace {
	void CreateIndexBuffer(Cmd* cmd);
	struct CreateIndexBufferCmd : Cmd
	{
		static constexpr DispatchFn DISPATCH = { CreateIndexBuffer };
		uint32_t index;
		const MemoryBlock* block;
		IndexType type;
	};
	void CreateIndexBuffer(Cmd* cmd)
	{
		auto data = reinterpret_cast<CreateIndexBufferCmd*>(cmd);
		GLuint ib;
		glGenBuffers(1, &ib);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data->block->size, data->block->data, GL_STATIC_DRAW);
		index_buffers[data->index].buffer = ib;

		switch (data->type)
		{
		case IndexType::UByte:
			index_buffers[data->index].type = GL_UNSIGNED_BYTE;
			index_buffers[data->index].num_elements = data->block->size;
			break;
		case IndexType::UShort:
			index_buffers[data->index].type = GL_UNSIGNED_SHORT;
			index_buffers[data->index].num_elements = data->block->size / 2;
			break;
		case IndexType::UInt:
			index_buffers[data->index].type = GL_UNSIGNED_INT;
			index_buffers[data->index].num_elements = data->block->size / 4;
			break;
		default:
			break;
		}
	}

}

IndexBufferHandle render::CreateIndexBuffer(const MemoryBlock* data, IndexType type)
{
	auto ib = index_buffers.Push();
	IndexBufferHandle result{ ib.index };

	auto cmd = pre_buffer.Add<CreateIndexBufferCmd>();
	cmd->block = data;
	cmd->type = type;
	cmd->index = ib.index;

	*ib.obj = IndexBuffer{};
	return result;
}

namespace {
	void CreateDynamicIndexBuffer(Cmd* cmd);
	struct CreateDynamicIndexBufferCmd : Cmd
	{
		static constexpr DispatchFn DISPATCH = { CreateDynamicIndexBuffer };
		uint32_t index;
		const MemoryBlock* block;
		IndexType type;
	};
	void CreateDynamicIndexBuffer(Cmd* cmd)
	{
		auto data = reinterpret_cast<CreateDynamicIndexBufferCmd*>(cmd);
		GLuint ib;
		auto size = 0;
		glGenBuffers(1, &ib);
		if (data->block != nullptr) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, data->block->size, data->block->data, GL_DYNAMIC_DRAW);
			size = data->block->size;
		}
		index_buffers[data->index].buffer = ib;



		switch (data->type)
		{
		case IndexType::UByte:
			index_buffers[data->index].type = GL_UNSIGNED_BYTE;
			index_buffers[data->index].num_elements = size;
			break;
		case IndexType::UShort:
			index_buffers[data->index].type = GL_UNSIGNED_SHORT;
			index_buffers[data->index].num_elements = size / 2;
			break;
		case IndexType::UInt:
			index_buffers[data->index].type = GL_UNSIGNED_INT;
			index_buffers[data->index].num_elements = size / 4;
			break;
		default:
			break;
		}
	}
}

DynamicIndexBufferHandle render::CreateDynamicIndexBuffer(const MemoryBlock* data, IndexType type)
{
	auto ib = index_buffers.Push();
	DynamicIndexBufferHandle result{ ib.index };
	
	auto cmd = pre_buffer.Add<CreateDynamicIndexBufferCmd>();
	cmd->block = data;
	cmd->type = type;
	cmd->index = ib.index;
	*ib.obj = IndexBuffer{};
	return result;
}

DynamicIndexBufferHandle render::CreateDynamicIndexBuffer(IndexType type)
{
	return CreateDynamicIndexBuffer(nullptr, type);
}

namespace {
	void UpdateDynamicIndexBuffer(Cmd* cmd);
	struct UpdateDynamicIndexBufferCmd : Cmd
	{
		static constexpr DispatchFn DISPATCH = { UpdateDynamicIndexBuffer };
		DynamicIndexBufferHandle ib;
		const MemoryBlock* block;
		GLintptr offset;
	};
	void UpdateDynamicIndexBuffer(Cmd* cmd)
	{
		auto data = reinterpret_cast<UpdateDynamicIndexBufferCmd*>(cmd);
		auto& ib = index_buffers[data->ib.index];

		GLint prev_buffer;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prev_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib.buffer);
		int previous_size = ib.num_elements;
		if (ib.type == GL_UNSIGNED_SHORT) { previous_size *= 2; }
		else if (ib.type == GL_UNSIGNED_INT) { previous_size *= 4; }

		if (previous_size < data->offset + data->block->size) {
			//TODO: Right now, this breaks the use of offset, by not copying the data over.
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, data->offset + data->block->size, 0, GL_DYNAMIC_DRAW);
		}
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, data->offset, data->block->size, data->block->data);

		switch (ib.type)
		{
		case GL_UNSIGNED_BYTE:
			ib.num_elements = data->block->size;
			break;
		case GL_UNSIGNED_SHORT:
			ib.num_elements = data->block->size / 2;
			break;
		case GL_UNSIGNED_INT:
			ib.num_elements = data->block->size / 4;
			break;
		default:
			break;
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prev_buffer);
	}

}

void render::UpdateDynamicIndexBuffer(DynamicIndexBufferHandle handle, const MemoryBlock* data, const ptrdiff_t offset)
{
	auto cmd = pre_buffer.Add<UpdateDynamicIndexBufferCmd>();
	cmd->block = data;
	cmd->offset = offset;
	cmd->ib = handle;
}

#pragma endregion

#pragma region Texture Functions

namespace {
	void GetTextureFormats(TextureFormat f, GLenum* internal_format, GLenum* format, GLenum* type)
	{
		ASSERT(internal_format != nullptr && format != nullptr && type != nullptr);

		switch (f)
		{
		case render::TextureFormat::RGB8:
			*internal_format = GL_RGB8;
			*format = GL_RGB;
			*type = GL_UNSIGNED_BYTE;
			break;
		case render::TextureFormat::RGBA8:
			*internal_format = GL_RGBA8;
			*format = GL_RGBA;
			*type = GL_UNSIGNED_BYTE;
			break;
		default:
			break;
		}
	}

	void CreateTexture(Cmd* cmd);
	struct CreateTextureCmd : Cmd
	{
		static constexpr DispatchFn DISPATCH = { CreateTexture };
		uint32_t index;
		GLenum target;
		TextureFormat format;
		uint16_t width;
		uint16_t height;
		const MemoryBlock* block;
	};
	void CreateTexture(Cmd* cmd)
	{
		auto data = reinterpret_cast<CreateTextureCmd*>(cmd);
		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(data->target, tex);

		//TODO: Figure out how you want to deal with these params.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		//TODO: not always gonna be 2d
		//Extract these from the texture format.
		GLenum internal_format;
		GLenum format;
		GLenum type;
		GetTextureFormats(data->format, &internal_format, &format, &type);

		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, data->width, data->height, 0, format, type, data->block->data);

		textures[data->index].format = data->format;
		textures[data->index].height = data->height;
		textures[data->index].width = data->width;
		textures[data->index].name = tex;
		textures[data->index].target = data->target;
	}

}

TextureHandle render::CreateTexture2D(uint16_t width, uint16_t height, TextureFormat format, const MemoryBlock* data)
{
	auto tex = textures.Push();
	
	//TODO: Check that memory block is right size.

	auto cmd = pre_buffer.Add<CreateTextureCmd>();
	cmd->block = data;
	cmd->format = format;
	cmd->width = width;
	cmd->height = height;
	cmd->target = GL_TEXTURE_2D;
	cmd->index = tex.index;

	return TextureHandle{ tex.index };
}

namespace {
	void UpdateTexture(Cmd* cmd);
	struct UpdateTextureCmd : Cmd
	{
		static constexpr DispatchFn DISPATCH = { UpdateTexture };
		TextureHandle handle;
		const MemoryBlock* block;
	};
	void UpdateTexture(Cmd* cmd)
	{
		auto data = reinterpret_cast<UpdateTextureCmd*>(cmd);
		auto& texture = textures[data->handle.index];

		GLenum internal_format;
		GLenum format;
		GLenum type;
		GetTextureFormats(texture.format, &internal_format, &format, &type);
		glBindTexture(GL_TEXTURE_2D, texture.name);
		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture.width, texture.height, 0, format, type, data->block->data);
		
	}
}

void render::UpdateTexture2D(TextureHandle handle, const MemoryBlock* data)
{
	auto cmd = pre_buffer.Add<UpdateTextureCmd>();
	cmd->block = data;
	cmd->handle = handle;
}
#pragma endregion

#pragma region Uniforms

UniformHandle render::CreateUniform(const char * name, UniformType type)
{
	auto uniform = uniforms.Push();
	UniformHandle result{ uniform.index };
	rkg::MurmurHash murmur;
	murmur.Add(name, strlen(name));
	uniform.obj->hash = murmur.Finish();
	uniform.obj->type = type;
#ifdef RENDER_DEBUG
	strcpy_s(uniform.obj->name, name);
#endif
	return result;
}

void render::SetUniform(UniformHandle handle, const void* data, int num)
{
	std::shared_lock<std::shared_mutex> lock(render_buffer_mutex);

	auto type = uniforms[handle.index].type;
	uniform_buffer[front_buffer].Add(handle, type, num, data);
}

#pragma endregion

void render::SetState(uint64_t flags)
{
	current_draw.render_state = flags;
}

void render::SetVertexBuffer(VertexBufferHandle h, uint32_t first_vertex, uint32_t num_verts)
{
	current_draw.vertex_buffer = h.index;
	current_draw.vertex_offset = first_vertex;
	current_draw.vertex_count = num_verts;
}

void render::SetVertexBuffer(DynamicVertexBufferHandle h, uint32_t first_vertex, uint32_t num_verts)
{
	current_draw.vertex_buffer = h.index;
	current_draw.vertex_offset = first_vertex;
	current_draw.vertex_count = num_verts;
}

void render::SetIndexBuffer(IndexBufferHandle h, uint32_t offset, uint32_t num_elements)
{
	current_draw.index_buffer = h.index;
	current_draw.index_offset = offset;
	current_draw.index_count = num_elements;
}

void render::SetIndexBuffer(DynamicIndexBufferHandle h, uint32_t offset, uint32_t num_elements)
{
	current_draw.index_buffer = h.index;
	current_draw.index_offset = offset;
	current_draw.index_count = num_elements;
}

void render::SetTexture(TextureHandle tex, UniformHandle sampler, uint16_t texture_unit)
{
	GLint unit = texture_unit;
	SetUniform(sampler, &unit);
	current_draw.textures[texture_unit] = tex;
}

void render::SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	current_draw.scissor[0] = x;
	current_draw.scissor[1] = y;
	current_draw.scissor[2] = w;
	current_draw.scissor[3] = h;
}

void render::Submit(LayerHandle layer, ProgramHandle program, uint32_t depth, bool preserve_state) 
{
	//TODO: Do we want these heavy locks? Or just require render gets called at sync point?
	std::shared_lock<std::shared_mutex> lock(render_buffer_mutex);

	//Create a sort key for this draw call
	Key key;
	key.bucket = layer.index;
	key.compute = false;
	key.program = program.index;
	key.depth = depth;

	//TODO: Key Sequential ordering.
	static thread_local uint64_t previous_frame = 0; 
	if (previous_frame != frame) //First draw this frame.
	{
		current_draw.uniform_start = 0;
	}

	//Todo: Check on the memory order stuff - not entirely sure this is correct.
	//Note: May want to figure out a better way to store keys, since this will have multiple threads writing to the same cache line.
	
	unsigned int index = key_index[front_buffer].fetch_add(1, std::memory_order_relaxed);
	key.sequence = 0;//TODO: Something about sequential rendering needs to go here.
	auto encoded_key = key.Encode();
	
	current_draw.program = program;
	//Handle Uniforms
	auto uniform_end = uniform_buffer[front_buffer].GetPosition();
	current_draw.uniform_buffer = &uniform_buffer[front_buffer];
	current_draw.uniform_end = uniform_end;
	//This stuff is thread local, so doesn't need to be atomic.

	//TODO: Really don't like the naming here - very unclear. This code is brittle as hell.
	auto buffer = &render_buffer[front_buffer];
	auto draw_index = render_buffer_count[front_buffer];
	(*buffer)[draw_index] = current_draw;
	keys[front_buffer][index] = { encoded_key,  &(*buffer)[draw_index] };
	render_buffer_count[front_buffer] = (draw_index + 1) % MAX_DRAWS_PER_THREAD;
	
	if (!preserve_state) {
		//Note: Could replace this with a default constructor for DrawCmd.
		current_draw = DrawCmd{};
	}
	current_draw.uniform_start = uniform_end;
	previous_frame = frame;
}

void render::Render()
{
	unsigned int back_buffer=0;
	{
		std::lock_guard<std::shared_mutex> lock(render_buffer_mutex);
		back_buffer = front_buffer.fetch_xor(1);
		frame++;
	}
	//Do any pre-render stuff wrt resources that need management.
	pre_buffer.Execute();

	SortKeys(back_buffer);

	//Need to store the current state of the important stuff
	IndexBufferHandle index_buffer_handle{ INVALID_HANDLE};
	VertexBufferHandle vertex_buffer_handle{ INVALID_HANDLE };
	ProgramHandle program_handle{ INVALID_HANDLE };
	
	TextureHandle bound_textures[MAX_TEXTURE_UNITS];
	for (int tex_unit = 0; tex_unit < MAX_TEXTURE_UNITS; tex_unit++) { bound_textures[tex_unit].index = INVALID_HANDLE; }
	static uint64_t raster_state = RenderState::DEFAULT_STATE;
	GLenum primitive_type = GL_TRIANGLES;

	Program* program = 0;

	int framebuffer_size[4];
	glGetIntegerv(GL_VIEWPORT, framebuffer_size);
	uint32_t scissor[4] = { 0,0, framebuffer_size[2], framebuffer_size[3] };
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);


	unsigned int num_commands = key_index[back_buffer];
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (unsigned int command_index = 0; command_index < num_commands; command_index++) {
		auto encoded_key = keys[back_buffer][command_index];

		auto cmd = encoded_key.cmd;
		auto key = Key::Decode(encoded_key.key);
		bool indexed_draw = false;

		//Set the program
		if (cmd->program.index != program_handle.index) {
			program = &programs[cmd->program.index];
			glUseProgram(program->id);
			program_handle = cmd->program;
		}

		//Update uniforms
		cmd->uniform_buffer->Execute(program, cmd->uniform_start, cmd->uniform_end);

		//Set textures:
		for (int tex_unit = 0; tex_unit < MAX_TEXTURE_UNITS; tex_unit++) {
			if (cmd->textures[tex_unit].index != INVALID_HANDLE
				&& bound_textures[tex_unit].index != cmd->textures[tex_unit].index)
			{
				auto& tex = textures[cmd->textures[tex_unit].index];
				glActiveTexture(GL_TEXTURE0 + tex_unit);
				glBindTexture(tex.target, tex.name);
				bound_textures[tex_unit] = cmd->textures[tex_unit];
			}
		}

		//Compute commands handled here
		if (key.compute) 
		{
			auto compute_cmd = reinterpret_cast<ComputeCmd*>(cmd);
			glDispatchCompute(compute_cmd->x, compute_cmd->y, compute_cmd->z);
			continue;
		}

		//Everything else is a draw command

		auto draw_cmd = reinterpret_cast<DrawCmd*>(cmd);

		//Check view and update it

		

		//Update rasterization state
		if (raster_state != draw_cmd->render_state ) //Raster state has changed.
		{
			//Depth test
			if (((raster_state ^ draw_cmd->render_state) & RenderState::DEPTH_TEST_MASK) != 0) 
			{
				
				//NOTE: This list has to match the order in the RenderState enum. I don't love that brittleness.
				if ((raster_state & RenderState::DEPTH_TEST_MASK) == RenderState::DEPTH_TEST_OFF) {
					glEnable(GL_DEPTH_TEST);
				}
				
				if ((draw_cmd->render_state & RenderState::DEPTH_TEST_MASK) == RenderState::DEPTH_TEST_OFF) {
					glDisable(GL_DEPTH_TEST);
				}
				else
				{
					constexpr GLenum depth_funcs[] = { GL_LESS, GL_LEQUAL, GL_EQUAL, GL_GEQUAL,
						GL_GREATER, GL_NOTEQUAL, GL_NEVER, GL_ALWAYS };
					int depth_func_index = (draw_cmd->render_state & RenderState::DEPTH_TEST_MASK) >> RenderState::DEPTH_TEST_SHIFT;
					if (depth_func_index != 0) {
						glDepthFunc(depth_funcs[depth_func_index - 1]);
					}
				}
			}

			//Blending function
			if (((raster_state ^ draw_cmd->render_state) & RenderState::BLEND_MASK) != 0) 
			{
				
				constexpr GLenum blend_funcs[] = { GL_ZERO, GL_ONE, GL_SRC_COLOR, 
					GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, 
					GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, 
					GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR, 
					GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_SRC_ALPHA_SATURATE, 
					GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR, GL_SRC1_ALPHA, 
					GL_ONE_MINUS_SRC1_ALPHA };
				int blend_func_index = (draw_cmd->render_state & RenderState::BLEND_MASK) >> RenderState::BLEND_SHIFT;
				if (blend_func_index != 0) {
					glBlendFunc(GL_SRC_ALPHA, blend_funcs[blend_func_index - 1]);
				}
			}
			
			//Blending Equation
			if (((raster_state ^ draw_cmd->render_state) & RenderState::BLEND_EQUATION_MASK) != 0) 
			{
				
				constexpr GLenum blend_eqs[] = { GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX };
				int blend_eq_index = (draw_cmd->render_state & RenderState::BLEND_EQUATION_MASK) >> RenderState::BLEND_EQUATION_SHIFT;
				if (blend_eq_index != 0) {
					glBlendEquation(blend_eqs[blend_eq_index - 1]);
				}
			}

			//Culling settings
			if (((raster_state ^ draw_cmd->render_state) & RenderState::CULL_MASK) != 0) 
			{
				//Culling state

				auto cull_state = draw_cmd->render_state & RenderState::CULL_MASK;
				if ((raster_state & RenderState::CULL_MASK) == RenderState::CULL_OFF) {
					glEnable(GL_CULL_FACE);
				}
				else if (cull_state == RenderState::CULL_OFF) {
					glDisable(GL_CULL_FACE);
				}
				
				if (cull_state == RenderState::CULL_CW) {
					glFrontFace(GL_CW);
				} 
				else if (cull_state == RenderState::CULL_CCW) {
					glFrontFace(GL_CCW);
				}
			}

			if ((raster_state ^ draw_cmd->render_state) & RenderState::PRIMITIVE_MASK != 0) 
			{
				constexpr GLenum primitive_types[] = {
					GL_TRIANGLES,
					GL_TRIANGLE_STRIP,
					GL_TRIANGLE_FAN,
					GL_POINTS,
					GL_LINE_STRIP,
					GL_LINE_LOOP,
					GL_LINES,
					GL_PATCHES
				};
				const int primitive_index = (draw_cmd->render_state & RenderState::PRIMITIVE_MASK) >> RenderState::PRIMITIVE_SHIFT;
				primitive_type = primitive_types[primitive_index];

			}


			raster_state = draw_cmd->render_state;
		}

		//Update scissoring
		

		if (draw_cmd->scissor[2] == UINT32_MAX ){
			draw_cmd->scissor[2] = framebuffer_size[2];
		}
		if (draw_cmd->scissor[3] == UINT32_MAX) {
			draw_cmd->scissor[3] = framebuffer_size[3];
		}
		if(std::memcmp(scissor, draw_cmd->scissor, sizeof(scissor)) != 0)
		{
			glScissor(draw_cmd->scissor[0], draw_cmd->scissor[1], draw_cmd->scissor[2], draw_cmd->scissor[3]);
			std::memcpy(scissor, draw_cmd->scissor, sizeof(scissor));
		}

		
		//VAOs - lookup the appropriate one, create it if necessary.
		{
			if (index_buffer_handle.index != draw_cmd->index_buffer ||
				vertex_buffer_handle.index != draw_cmd->vertex_buffer)
			{
				vertex_buffer_handle = VertexBufferHandle{ draw_cmd->vertex_buffer };
				index_buffer_handle = IndexBufferHandle{ draw_cmd->index_buffer };
				//Compute hash
				rkg::MurmurHash hash;
				hash.Add(draw_cmd->vertex_buffer);
				hash.Add(draw_cmd->index_buffer);
				auto vao_hash = hash.Finish();
				
				//Attempt to lookup vao.
				static GLCache<MAX_VERTEX_ARRAY_OBJECTS> vao_cache;
				auto vao = vao_cache.Get(vao_hash);
				if (vao != vao_cache.INVALID_INDEX) { 
					glBindVertexArray(vao);
				}
				else {
					//Create a new VAO for this data.
					glGenVertexArrays(1, &vao);
					vao_cache.Add(vao_hash, vao);
					glBindVertexArray(vao);
					auto& vertex_buffer = vertex_buffers[draw_cmd->vertex_buffer];
					
					
					auto& vertex_layout = vertex_buffer.layout;
					glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer.buffer);

					if (draw_cmd->index_buffer != INVALID_HANDLE) {
						auto& index_buffer = index_buffers[draw_cmd->index_buffer];
						glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer.buffer);
					}
					auto vertex_size = vertex_layout.SizeOfVertex();
					for (int i = 0; i < vertex_layout.num_attributes; i++) {
						auto attrib_loc = glGetAttribLocation(program->id, vertex_layout.names[i]);
						/*
						Pack attribute info into 8 bits:
						76543210
						xnttttss
						x - reserved
						n - normalized
						t - type
						s - number
						*/
						auto attrib = vertex_layout.attribs[i];
						bool normalized = (attrib & 0x40) >> 6;
						GLuint size = (attrib & 0x03) + 1;
						auto type = (VertexLayout::AttributeType::Enum)((attrib & 0x3C) >> 2);
						
						glVertexAttribPointer(attrib_loc, size, GetGLEnum(type), normalized, vertex_size, (void*)vertex_layout.offset[i]);
						glEnableVertexAttribArray(attrib_loc);
					}
				}
			}
		}



		//Draw.
		if (index_buffer_handle.index != INVALID_HANDLE) {
			auto& ib = index_buffers[index_buffer_handle.index];
			
			unsigned int offset = draw_cmd->index_offset;
			if (draw_cmd->index_offset != 0) {
				switch (ib.type)
				{
				case GL_UNSIGNED_SHORT:
					offset *= 2;
					break;
				case GL_UNSIGNED_INT:
					offset *= 4;
					break;
				default:
					break;
				}
			}
			auto num_elements = ib.num_elements - offset;
			if (draw_cmd->index_count != UINT32_MAX) {
				num_elements = draw_cmd->index_count;
			}

			glDrawElementsBaseVertex(primitive_type, num_elements, 
				ib.type, 
				(GLvoid*)(offset), 
				draw_cmd->vertex_offset);
		}
		else {
			//NOTE: Take a look, vertex offset might have the same problem as index offset did.
			auto& vb = vertex_buffers[vertex_buffer_handle.index];
			auto num_verts = vb.size / vb.layout.SizeOfVertex();
			if (draw_cmd->vertex_count != UINT32_MAX) {
				num_verts = draw_cmd->vertex_count;
			}
			glDrawArrays(primitive_type, draw_cmd->vertex_offset, num_verts);
		}
	}
	
	key_index[back_buffer] = 0;
	uniform_buffer[back_buffer].Clear();
	FreeMemory();
}
