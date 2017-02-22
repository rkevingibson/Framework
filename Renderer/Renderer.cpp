
#include "Renderer.h"
#include <cassert>
#include <cstdio>
#include <type_traits>
#include <array>
#include <atomic>
#include <queue>
#include <mutex>
#include <cstddef>
#include <algorithm>
#include "../Utilities/Utilities.h"
#include "../Utilities/MurmurHash.h"
#include "../Utilities/Allocators.h"
#include "../External/GLFW/glfw3.h"
#include "GLLite.h"

using namespace rkg;
using namespace gl;


namespace
{
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
		} else {
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
		Expects(write_pos_ + num < Size);
		memcpy(&buffer_[write_pos_], data, num);
		write_pos_ += num;
		return write_pos_;
	}

	template<typename T>
	T Read()
	{
		static_assert(std::is_arithmetic<T>::value, "Requires arithmetic type.");
		Expects(read_pos_ + sizeof(T) < Size && "Insufficient data to read type");
		auto result = reinterpret_cast<T*>(&buffer_[read_pos_]);
		read_pos_ += sizeof(T);
		return *result;
	}

	rkg::byte* GetPtr()
	{
		return &buffer_[read_pos_];
	}

	void Seek(const uint32_t pos)
	{
		Expects(pos < Size);
		read_pos_ = pos;
	}

	void Skip(const uint32_t num)
	{
		read_pos_ += num;
	}

	uint32_t GetWritePos()
	{
		return write_pos_;
	}
	uint32_t GetReadPos()
	{
		return read_pos_;
	}

	void Clear()
	{
		write_pos_ = 0;
		read_pos_ = 0;
		memset(buffer_.data(), 0, buffer_.size());
	}
private:
	uint32_t write_pos_;
	uint32_t read_pos_;
	std::array<rkg::byte, Size> buffer_;
};

template<typename T, uint32_t Size>
class ResourceList
{
private:
	T data_[Size];

	//We'll use an intrusive freelist to keep track of free spots.
	struct Node
	{
		Node* next{ nullptr };
	};
	Node* head_{ nullptr };
	static_assert(sizeof(Node) <= sizeof(T), "Invalid ResourceList created: Need larger type to allow for freelist.");

public:
	struct ResultPair
	{
		uint32_t index;
		T* obj;
	};

	ResourceList()
	{
		//To initialize, thread a freelist through the whole thing.
		Clear();
	}

	ResultPair Create()
	{
		Expects(head_);
		uint16_t index;
		auto result = reinterpret_cast<T*>(head_);
		index = result - data_;
		head_ = head_->next;
		memset(result, 0, sizeof(T));

		return ResultPair{ index, result };
	}

	void Remove(uint16_t index)
	{
		memset(&data_[index], 0, sizeof(T));
		auto node = reinterpret_cast<Node*>(&data_[index]);
		node->next = head_;
		head_ = node;
	}

	inline T& operator[](uint32_t index)
	{
		return data_[index];
	}

	void Clear()
	{
		memset(data_, 0, sizeof(T)*Size);
		for (uint16_t i = 0; i < Size - 1; i++) {
			auto node = reinterpret_cast<Node*>(&data_[i]);
			node->next = reinterpret_cast<Node*>(&data_[i + 1]);
		}
		reinterpret_cast<Node*>(&data_[Size - 1])->next = nullptr;
		head_ = reinterpret_cast<Node*>(&data_[0]);
	}
};
#pragma endregion

#pragma region Data Structures


struct Key
{
	//Sorting key for draw calls. Let's us reorder draws to minimize state change.	

	/*
		Draw Key layout:


			  x38      x30      x28      x20       x18      x10       x8       x0
		765543210.76543210.76543210.76543210  76543210.76543210.76543210.76543210
		bbbbbbbbb csssssss sssspppp pppppppp  ddddddddddddddddddddddddddddddddddd

		b - layer - corresponds to the view/renderbuffer.
		c - compute - set if the key is a compute shader key. Reserved right now.
		s - sequence - order of draw in the frame. 0 by default, right now no support for sequenced rendering.
		p - program - the render program
		d - depth
	*/

	uint64_t Encode()
	{
		uint64_t result = 0 |
			((uint64_t)layer << 56) |
			(((uint64_t)compute & 0x1) << 55) |
			(((uint64_t)sequence & 0x7FF) << 44) |
			(((uint64_t)program & 0xFFF) << 32) |
			depth;
		return result;
	}
	static Key Decode(uint64_t k)
	{
		Key result;
		result.layer = (k >> 0x38);
		result.compute = (k >> 0x37) & 0x1;
		result.sequence = (k >> 0x2C) & 0x7FF;
		result.program = (k >> 0x20) & 0xFFF;
		result.depth = k & 0xFFFFFFFF;
		return result;
	}
	uint8_t	layer : 8;
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
	render::VertexLayout layout;
};

struct IndexBuffer
{
	GLuint	buffer;
	uint32_t	num_elements;
	GLenum	type;
};

struct ShaderStorageBuffer
{
	GLuint buffer;
	uint32_t size;
};

struct AtomicCounterBuffer
{
	GLuint buffer;
	uint32_t size;
};

struct Texture
{
	uint16_t width, height;
	TextureFormat format;
	GLuint name;
	GLenum target;
};

struct Uniform
{
	uint32_t hash;//Hash of the uniforms name.
	UniformType type;
#ifdef RENDER_DEBUG
	char name[64];
#endif
};

struct Program
{
	GLuint	id;
	unsigned int num_uniforms;
	UniformHandle uniform_handles[MAX_UNIFORMS];
	GLCache<MAX_UNIFORMS> uniforms;
};

class UniformBuffer;

struct RenderCmd
{
	ProgramHandle program{ INVALID_HANDLE };
	//Uniform updates.
	uint32_t uniform_start;
	uint32_t uniform_end;

	//Textures
	TextureHandle textures[MAX_TEXTURE_UNITS];

	GLuint ssbos[MAX_SSBO_BINDINGS];
	GLuint atomic_counters[MAX_ATOMIC_COUNTER_BINDINGS];

	RenderCmd()
	{
		for (int i = 0; i < MAX_TEXTURE_UNITS; i++) {
			textures[i].index = INVALID_HANDLE;
		}

		for (int i = 0; i < MAX_SSBO_BINDINGS; i++) {
			ssbos[i] = UINT32_MAX;
		}

		for (int i = 0; i < MAX_ATOMIC_COUNTER_BINDINGS; i++) {
			atomic_counters[i] = UINT32_MAX;
		}
	}
};

struct DrawCmd : public RenderCmd
{
	//Store everything needed to make any type of draw call
	DrawCmd()
	{
		scissor[0] = 0;
		scissor[1] = 0;
		scissor[2] = UINT32_MAX;
		scissor[3] = UINT32_MAX;
	}

	uint64_t	render_state{ RenderState::DEFAULT_STATE };
	uint32_t	vertex_buffer{ INVALID_HANDLE };
	uint32_t	index_buffer{ INVALID_HANDLE };

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
std::atomic<uint64_t> frame;
ErrorCallbackFn error_callback;

struct EncodedKey
{
	uint64_t key;
	RenderCmd* cmd;
};

#pragma region Per-Frame Data

std::array<EncodedKey, MAX_DRAWS_PER_FRAME>	keys;
RenderCmd current_rendercmd;
DrawCmd current_draw;
ComputeCmd current_compute;

int render_buffer_count{ 0 };
std::array<DrawCmd, MAX_DRAWS_PER_FRAME> render_buffer;
int compute_buffer_count{ 0 };
std::array<ComputeCmd, MAX_DRAWS_PER_FRAME> compute_buffer;

#pragma endregion

unsigned int key_index{ 0 };

void SortKeys()
{
	//NB: This function only gets called from the render function, so front_buffer is guaranteed not to switch during execution, 
	//And the arrays won't be written to at all.
	auto key_list = &keys;
	unsigned int num_keys = key_index;
	//How to do this? Need to sort in place.
	//Could use std::sort, but I would need to move the key/draw stuff into a single list.
	//Why are they in separate lists? Because once theyre sorted 
	//I don't care about the keys really... but I kind of do. 
	std::sort(key_list->begin(), key_list->begin() + num_keys, [](const EncodedKey& a, const EncodedKey& b) { return a.key < b.key; });
}

//These buffers manage resources which live across many frames.
//Stack really isn't appropriate for these... Really want an allocator or something.
ResourceList<RenderLayer, MAX_RENDER_LAYERS>     layers;
ResourceList<VertexBuffer, MAX_VERTEX_BUFFERS>   vertex_buffers;
ResourceList<IndexBuffer, MAX_INDEX_BUFFERS>     index_buffers;
ResourceList<ShaderStorageBuffer, MAX_SHADER_STORAGE_BUFFERS>     shader_storage_buffers;
ResourceList<AtomicCounterBuffer, MAX_ATOMIC_COUNTER_BUFFERS> atomic_counter_buffers;
ResourceList<Texture, MAX_TEXTURES>              textures;
ResourceList<Uniform, MAX_UNIFORMS>              uniforms;
ResourceList<Program, MAX_SHADER_PROGRAMS>       programs;


/*==================== Pre/post-rendering Command Buffer ====================*/

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
		while (buffer_.GetReadPos() < end) {
			auto handle = buffer_.Read<decltype(UniformHandle::index)>();
			auto type_num = buffer_.Read<uint8_t>();
			auto num = buffer_.Read<uint8_t>();
			const rkg::byte* payload = buffer_.GetPtr();

			auto hash = uniforms[handle].hash;
			GLint location = program->uniforms.Get(hash);
			UniformType type = static_cast<UniformType>(type_num);

			auto payload_size = Size(type)*num;
			buffer_.Skip(payload_size);

			switch (type) {
			case UniformType::Sampler:
			case UniformType::Int:
				glUniform1iv(location, num, reinterpret_cast<const GLint*>(payload));
				break;
			case UniformType::Uint:
				glUniform1uiv(location, num, reinterpret_cast<const GLuint*>(payload));
				break;
			case UniformType::Float:
				glUniform1fv(location, num, reinterpret_cast<const GLfloat*>(payload));
				break;
			case UniformType::Vec2:
				glUniform2fv(location, num, reinterpret_cast<const GLfloat*>(payload));
				break;
			case UniformType::Ivec2:
				glUniform2iv(location, num, reinterpret_cast<const GLint*>(payload));
				break;
			case UniformType::Vec3:
				glUniform3fv(location, num, reinterpret_cast<const GLfloat*>(payload));
				break;
			case UniformType::Ivec3:
				glUniform3iv(location, num, reinterpret_cast<const GLint*>(payload));
				break;
			case UniformType::Vec4:
				glUniform4fv(location, num, reinterpret_cast<const GLfloat*>(payload));
				break;
			case UniformType::Ivec4:
				glUniform4iv(location, num, reinterpret_cast<const GLint*>(payload));
				break;
			case UniformType::Mat3:
				glUniformMatrix3fv(location, num, false, reinterpret_cast<const GLfloat*>(payload));
				break;
			case UniformType::Mat4:
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

	uint32_t GetWritePosition()
	{
		return buffer_.GetWritePos();
	}
private:
	uint32_t Size(UniformType type)
	{
		uint32_t type_size;
		switch (type) {
		case UniformType::Sampler:
		case UniformType::Int:
		case UniformType::Uint:
		case UniformType::Float:
			type_size = 4;
			break;
		case UniformType::Vec2:
		case UniformType::Ivec2:
			type_size = 8;
			break;
		case UniformType::Vec3:
		case UniformType::Ivec3:
			type_size = 12;
			break;
		case UniformType::Vec4:
		case UniformType::Ivec4:
			type_size = 16;
			break;
		case UniformType::Mat3:
			type_size = 9 * 4;
			break;
		case UniformType::Mat4:
			type_size = 16 * 4;
			break;
		default:
			break;
		}
		return type_size;
	}

	RawBuffer<MEGA(2)> buffer_;
};

UniformBuffer uniform_buffer;
} //End anonymous namespace.

#pragma region Memory Management
  /*==================== MEMORY MANAGEMENT ====================*/
namespace
{
struct MemoryRef
{
	MemoryBlock block;
	ReleaseFunction release{ nullptr };
	void* user_data{ nullptr };
};

using RenderAllocator = rkg::Mallocator;

RenderAllocator renderer_allocator;



bool IsMemoryRef(const MemoryBlock* b)
{
	return reinterpret_cast<uintptr_t>(b->ptr) != reinterpret_cast<uintptr_t>(b) + sizeof(MemoryBlock);
}

void DeallocateBlock(const MemoryBlock* b)
{
	if (IsMemoryRef(b)) {
		auto ref = reinterpret_cast<MemoryRef*>(reinterpret_cast<intptr_t>(b) - offsetof(MemoryRef, block));
		if (ref->release) {
			ref->release(ref->block, ref->user_data);
		}
		MemoryBlock block;
		block.length = sizeof(MemoryRef);
		block.ptr = ref;
		renderer_allocator.Deallocate(block);
	} else {
		MemoryBlock block;
		block.ptr = (void*)b;
		block.length = sizeof(MemoryBlock) + b->length;
		renderer_allocator.Deallocate(block);
	}
}

void FreeMemory()
{
	//Not sure how to do this now? Will have to change how I'm doing this.

}


}

const MemoryBlock* gl::Alloc(const uint32_t size)
{
	auto block = renderer_allocator.Allocate(size + sizeof(MemoryBlock));
	auto result = reinterpret_cast<MemoryBlock*>(block.ptr);
	result->length = size;
	result->ptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(block.ptr) + sizeof(MemoryBlock));
	return result;
}

const MemoryBlock* gl::AllocAndCopy(const void * const data, const uint32_t size)
{
	auto block = Alloc(size);
	memcpy(block->ptr, data, size);
	return block;
}

const MemoryBlock* gl::MakeRef(const void* data, const uint32_t size, ReleaseFunction fn, void* user_data)
{
	auto block = renderer_allocator.Allocate(sizeof(MemoryRef));
	auto ref = reinterpret_cast<MemoryRef*>(block.ptr);
	ref->block.length = size;
	ref->block.ptr = (void*)data; //Dont like this, but I don't see a way around it.
	ref->release = fn;
	ref->user_data = user_data;
	return &ref->block;
}

const MemoryBlock* gl::LoadShaderFile(const char* file)
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
	auto block = Alloc(len + 1);
	rewind(f);
	fread(block->ptr, 1, len, f);
	((char*)block->ptr)[len] = '\0';
	fclose(f);
	return block;
}

#pragma region VertexLayout functions
namespace
{
GLenum GetGLEnum(render::VertexLayout::AttributeType val)
{
	using namespace render;
	switch (val) {
	case VertexLayout::AttributeType::INT8:
		return GL_BYTE;
		break;
	case VertexLayout::AttributeType::UINT8:
		return GL_UNSIGNED_BYTE;
		break;
	case VertexLayout::AttributeType::INT16:
		return GL_SHORT;
		break;
	case VertexLayout::AttributeType::UINT16:
		return GL_UNSIGNED_SHORT;
		break;
	case VertexLayout::AttributeType::FLOAT16:
		return GL_HALF_FLOAT;
		break;
	case VertexLayout::AttributeType::INT32:
		return GL_INT;
		break;
	case VertexLayout::AttributeType::UINT32:
		return GL_UNSIGNED_INT;
		break;
	case VertexLayout::AttributeType::PACKED_2_10_10_10_REV:
		return GL_INT_2_10_10_10_REV;
		break;
	case VertexLayout::AttributeType::UPACKED_2_10_10_10_REV:
		return GL_UNSIGNED_INT_2_10_10_10_REV;
		break;
	case VertexLayout::AttributeType::FLOAT32:
		return GL_FLOAT;
		break;
	case VertexLayout::AttributeType::FLOAT64:
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
namespace
{

	//TODO: Maybe allow the user to set an error callback, given them a nicely formatted error.
void GLAPI GLErrorCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, void* userParam)
{
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
		return;

	switch (severity) {
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

	switch (source) {
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


void gl::SetErrorCallback(ErrorCallbackFn f)
{
	error_callback = f;
}

//namespace
//{
//void Resize(Cmd*);
//struct ResizeCmd : Cmd
//{
//	static constexpr DispatchFn DISPATCH{ Resize };
//	int w, h;
//};
//
//void Resize(Cmd* cmd)
//{
//	auto data = reinterpret_cast<ResizeCmd*>(cmd);
//	glViewport(0, 0, data->w, data->h);
//}
//}




#pragma endregion
/*==================== RESOURCE MANAGEMENT ====================*/


namespace
{
UniformType UniformTypeFromEnum(GLenum e)
{
	switch (e) {
	case GL_SAMPLER_1D:
	case GL_SAMPLER_2D:
	case GL_SAMPLER_3D:
	case GL_SAMPLER_CUBE:
		return UniformType::Sampler;
		break;
	case GL_INT:
		return UniformType::Int;
		break;
	case GL_UNSIGNED_INT:
		return UniformType::Uint;
		break;
	case GL_FLOAT:
		return UniformType::Float;
		break;

	case GL_FLOAT_VEC2:
		return UniformType::Vec2;
		break;
	case GL_INT_VEC2:
		return UniformType::Ivec2;
		break;
	case GL_FLOAT_VEC3:
		return UniformType::Vec3;
		break;
	case GL_INT_VEC3:
		return UniformType::Ivec3;
		break;
	case GL_FLOAT_VEC4:
		return UniformType::Vec4;
		break;
	case GL_INT_VEC4:
		return UniformType::Ivec4;
		break;
	case GL_FLOAT_MAT3:
		return UniformType::Mat3;
		break;
	case GL_FLOAT_MAT4:
		return UniformType::Mat4;
		break;
	default:
		return UniformType::Count;
		break;
	}
}
}

ProgramHandle gl::CreateProgram(const MemoryBlock* vertex_shader, const MemoryBlock* frag_shader)
{
	auto program_handle = programs.Create();
	
	GLuint program;
	program = glCreateProgram();

	auto vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, (const GLchar**)&vertex_shader->ptr, nullptr);
	glCompileShader(vert);
	glAttachShader(program, vert);

	auto frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, (const GLchar**)&frag_shader->ptr, nullptr);
	glCompileShader(frag);
	glAttachShader(program, frag);

	if (error_callback != nullptr) {
		GLint status;
		glGetShaderiv(vert, GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE) {
			char buffer[512];
			glGetShaderInfoLog(vert, 512, NULL, buffer);
			error_callback(buffer);
		}

		glGetShaderiv(frag, GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE) {
			char buffer[512];
			glGetShaderInfoLog(frag, 512, NULL, buffer);
			error_callback(buffer);
		}
	}

	glLinkProgram(program);
	glDeleteShader(vert);
	glDeleteShader(frag);
	DeallocateBlock(vertex_shader);
	DeallocateBlock(frag_shader);

	//Get uniform information.
	{
		GLint num_uniforms, uniform_name_buffer_size;
		glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &num_uniforms);
		constexpr size_t UNIFORM_NAME_BUFFER_SIZE = 128;
		GLchar uniform_name_buffer[UNIFORM_NAME_BUFFER_SIZE];

#ifdef RENDER_DEBUG
		glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_name_buffer_size);
		uniform_name_buffer_size++;
		ASSERT(num_uniforms < 0 || uniform_name_buffer_size < UNIFORM_NAME_BUFFER_SIZE);
#endif
		for (GLint i = 0; i < num_uniforms; ++i) {
			GLint size;
			GLsizei length;
			GLenum type;
			//NOTE:CHECK THIS FUNCTION. WROTE WITHOUT DOCS.
			glGetActiveUniform(program, i, UNIFORM_NAME_BUFFER_SIZE, &length,
							   &size, &type, uniform_name_buffer);
			auto loc = glGetUniformLocation(program, uniform_name_buffer);

			if (loc < 0) continue;
			rkg::MurmurHash murmur;
			murmur.Add(uniform_name_buffer, length);
			auto hash = murmur.Finish();
			program_handle.obj->uniforms.Add(hash, loc);


			auto uni = uniforms.Create();
			uni.obj->hash = hash;
			strcpy_s(uni.obj->name, uniform_name_buffer);
			uni.obj->type = UniformTypeFromEnum(type);
			program_handle.obj->uniform_handles[i].index = uni.index;

		}
		program_handle.obj->num_uniforms = num_uniforms;
	}

	program_handle.obj->id = program;


	return ProgramHandle{ program_handle.index };
}

ProgramHandle gl::CreateComputeProgram(const MemoryBlock * compute_shader)
{
	auto program_handle = programs.Create();

	GLuint program;
	program = glCreateProgram();

	auto compute = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute, 1, (const GLchar**)&compute_shader->ptr, nullptr);
	glCompileShader(compute);
	glAttachShader(program, compute);

	if (error_callback != nullptr) {
		GLint status;
		glGetShaderiv(compute, GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE) {
			char buffer[512];
			glGetShaderInfoLog(compute, 512, NULL, buffer);
			error_callback(buffer);
		}
	}

	glLinkProgram(program);
	glDeleteShader(compute);
	DeallocateBlock(compute_shader);

	{
		GLint num_uniforms, uniform_name_buffer_size;
		glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &num_uniforms);
		constexpr size_t UNIFORM_NAME_BUFFER_SIZE = 128;
		GLchar uniform_name_buffer[UNIFORM_NAME_BUFFER_SIZE];

#ifdef RENDER_DEBUG
		glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_name_buffer_size);
		uniform_name_buffer_size++;
		ASSERT(num_uniforms < 0 || uniform_name_buffer_size < UNIFORM_NAME_BUFFER_SIZE);
#endif
		for (GLint i = 0; i < num_uniforms; ++i) {
			GLint size;
			GLsizei length;
			GLenum type;
			//NOTE:CHECK THIS FUNCTION. WROTE WITHOUT DOCS.
			glGetActiveUniform(program, i, UNIFORM_NAME_BUFFER_SIZE, &length,
							   &size, &type, uniform_name_buffer);
			auto loc = glGetUniformLocation(program, uniform_name_buffer);

			if (loc < 0) continue;
			rkg::MurmurHash murmur;
			murmur.Add(uniform_name_buffer, length);
			auto hash = murmur.Finish();
			program_handle.obj->uniforms.Add(hash, loc);


			auto uni = uniforms.Create();
			uni.obj->hash = hash;
			strcpy_s(uni.obj->name, uniform_name_buffer);
			uni.obj->type = UniformTypeFromEnum(type);
			program_handle.obj->uniform_handles[i].index = uni.index;

		}
		program_handle.obj->num_uniforms = num_uniforms;
	}

	program_handle.obj->id = program;

	return ProgramHandle{ program_handle.index };
}

unsigned int gl::GetNumUniforms(ProgramHandle h)
{
	return programs[h.index].num_uniforms;
}

int gl::GetProgramUniforms(ProgramHandle h, UniformHandle* buffer, int size)
{
	memcpy_s(buffer,
			 size * sizeof(UniformHandle),
			 programs[h.index].uniform_handles,
			 programs[h.index].num_uniforms * sizeof(UniformHandle));
	return programs[h.index].num_uniforms;
}

void gl::GetUniformInfo(UniformHandle h, char* name, int name_size, UniformType* type)
{
	//Get the name and type
#ifdef RENDER_DEBUG
	if (name != nullptr) {
		strcpy_s(name, name_size, uniforms[h.index].name);
	}
#endif
	if (type != nullptr) {
		*type = uniforms[h.index].type;
	}
}

#pragma region Vertex Buffer Functions

VertexBufferHandle gl::CreateVertexBuffer(const MemoryBlock* data, const render::VertexLayout& layout)
{
	GLuint buffer;
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, data->length, data->ptr, GL_STATIC_DRAW);
	auto vb = vertex_buffers.Create();
	vb.obj->buffer = buffer;
	vb.obj->size = data->length;
	vb.obj->layout = layout;
	DeallocateBlock(data);

	return VertexBufferHandle{ vb.index };
}

namespace
{

GLuint CreateBuffer(const MemoryBlock* block, GLenum target, GLenum usage)
{
	GLuint buffer;
	glGenBuffers(1, &buffer);
	glBindBuffer(target, buffer);
	if (block) {
		glBufferData(target, block->length, block->ptr, usage);
		DeallocateBlock(block);
	}
	return buffer;
}

}

VertexBufferHandle gl::CreateDynamicVertexBuffer(const MemoryBlock* data, const render::VertexLayout& layout)
{
	auto vb = vertex_buffers.Create();
	VertexBufferHandle result{ vb.index };

	vb.obj->layout = layout;
	vb.obj->size = (data) ? data->length : 0;
	vb.obj->buffer = CreateBuffer(data, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
	return result;
}

VertexBufferHandle gl::CreateDynamicVertexBuffer(const render::VertexLayout& layout)
{
	return CreateDynamicVertexBuffer(nullptr, layout);
}

namespace
{
void UpdateDynamicBuffer(GLenum target, GLuint buffer, const MemoryBlock* block, uint32_t* size)
{
	glBindBuffer(target, buffer);
	if (*size < block->length) {
		//TODO: Enable offsets to only update part of a buffer.
		glBufferData(target, block->length, 0, GL_DYNAMIC_DRAW);
	}
	glBufferSubData(target, 0, block->length, block->ptr);
	*size = block->length;
	DeallocateBlock(block);
}
}

void gl::UpdateDynamicVertexBuffer(VertexBufferHandle handle, const MemoryBlock* data, const ptrdiff_t offset)
{
	auto& vb = vertex_buffers[handle.index];
	UpdateDynamicBuffer(GL_ARRAY_BUFFER, vb.buffer, data, &vb.size);
}

#pragma endregion

#pragma region Index Buffer Functions

namespace
{
	uint32_t CreateElementBuffer(GLenum usage, render::IndexType type, const MemoryBlock* block)
	{
		auto buffer = index_buffers.Create();
		*buffer.obj = IndexBuffer{};
		GLuint ib;
		auto size = 0;
		glGenBuffers(1, &ib);
		if (block) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, block->length, block->ptr, usage);
			size = block->length;
		}
		buffer.obj->buffer = ib;

		switch (type) {
		case render::IndexType::UByte:
			buffer.obj->type = GL_UNSIGNED_BYTE;
			buffer.obj->num_elements = size;
			break;
		case render::IndexType::UShort:
			buffer.obj->type = GL_UNSIGNED_SHORT;
			buffer.obj->num_elements = size / 2;
			break;
		case render::IndexType::UInt:
			buffer.obj->type = GL_UNSIGNED_INT;
			buffer.obj->num_elements = size / 4;
			break;
		default:
			break;
		}

		if (block) {
			DeallocateBlock(block);
		}

		return buffer.index;
	}
}

IndexBufferHandle gl::CreateIndexBuffer(const MemoryBlock* data, render::IndexType type)
{
	IndexBufferHandle result{ CreateElementBuffer(GL_STATIC_DRAW, type, data) };
	return result;
}

IndexBufferHandle gl::CreateDynamicIndexBuffer(const MemoryBlock* data, render::IndexType type)
{
	IndexBufferHandle result{ CreateElementBuffer(GL_DYNAMIC_DRAW, type, data) };
	return result;
}

IndexBufferHandle gl::CreateDynamicIndexBuffer(render::IndexType type)
{
	return CreateDynamicIndexBuffer(nullptr, type);
}

namespace
{
void UpdateDynamicIndexBuffer(IndexBuffer* ib, const MemoryBlock* block)
{
	//TODO: Handle offsets properly.
	GLint prev_buffer;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prev_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib->buffer);
	int previous_size = ib->num_elements;
	if (ib->type == GL_UNSIGNED_SHORT) {
		previous_size *= 2;
	} else if (ib->type == GL_UNSIGNED_INT) {
		previous_size *= 4;
	}

	if (previous_size < block->length) {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, block->length, 0, GL_DYNAMIC_DRAW);
	}
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, block->length, block->ptr);

	switch (ib->type) {
	case GL_UNSIGNED_BYTE:
		ib->num_elements = block->length;
		break;
	case GL_UNSIGNED_SHORT:
		ib->num_elements = block->length / 2;
		break;
	case GL_UNSIGNED_INT:
		ib->num_elements = block->length / 4;
		break;
	default:
		break;
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prev_buffer);
	DeallocateBlock(block);
}

}

void gl::UpdateDynamicIndexBuffer(IndexBufferHandle handle, const MemoryBlock* data, const ptrdiff_t offset)
{
	UpdateDynamicIndexBuffer(&index_buffers[handle.index], data);
}

#pragma endregion

#pragma region SSBO Functions

SSBOHandle gl::CreateShaderStorageBuffer(const MemoryBlock* data)
{
	auto ssbo = shader_storage_buffers.Create();
	SSBOHandle result{ ssbo.index };
	ssbo.obj->size = data->length;
	ssbo.obj->buffer = CreateBuffer(data, GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW);
	return result;
}

void gl::UpdateShaderStorageBuffer(SSBOHandle handle, const MemoryBlock* data)
{
	auto& ssbo = shader_storage_buffers[handle.index];
	UpdateDynamicBuffer(GL_SHADER_STORAGE_BUFFER, ssbo.buffer, data, &ssbo.size);
}

AtomicCounterBufferHandle gl::CreateAtomicCounterBuffer(const MemoryBlock * data)
{
	auto atomic_buffer = atomic_counter_buffers.Create();

	AtomicCounterBufferHandle result{ atomic_buffer.index };
	atomic_buffer.obj->size = data->length;
	atomic_buffer.obj->buffer = CreateBuffer(data, GL_ATOMIC_COUNTER_BUFFER, GL_DYNAMIC_DRAW);
	
	return result;
}

void gl::UpdateAtomicCounterBuffer(AtomicCounterBufferHandle h, const MemoryBlock* data)
{
	auto& atomic_buffer = atomic_counter_buffers[h.index];
	UpdateDynamicBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_buffer.buffer, data, &atomic_buffer.size);
}

#pragma endregion

#pragma region Texture Functions

namespace
{
void GetTextureFormats(TextureFormat f, GLenum* internal_format, GLenum* format, GLenum* type)
{
	ASSERT(internal_format != nullptr && format != nullptr && type != nullptr);

	switch (f) {
	case gl::TextureFormat::RGB8:
		*internal_format = GL_RGB8;
		*format = GL_RGB;
		*type = GL_UNSIGNED_BYTE;
		break;
	case gl::TextureFormat::RGBA8:
		*internal_format = GL_RGBA8;
		*format = GL_RGBA;
		*type = GL_UNSIGNED_BYTE;
		break;
	default:
		break;
	}
}

uint32_t CreateTexture(GLenum target, TextureFormat format, uint16_t w, uint16_t h, const MemoryBlock* block)
{
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(target, tex);

	//TODO: Figure out how you want to deal with these params.
	glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//TODO: not always gonna be 2d
	//Extract these from the texture format.
	GLenum internal_format;
	GLenum gl_format;
	GLenum type;
	GetTextureFormats(format, &internal_format, &gl_format, &type);
	glTexImage2D(target, 0, internal_format, w, h, 0, gl_format, type, block->ptr);


	auto texture_obj = textures.Create();
	auto texture = texture_obj.obj;
	texture->format = format;
	texture->height = h;
	texture->width = w;
	texture->name = tex;
	texture->target = target;
	DeallocateBlock(block);

	return texture_obj.index;
}

}

TextureHandle gl::CreateTexture2D(uint16_t width, uint16_t height, TextureFormat format, const MemoryBlock* data)
{
	return TextureHandle{ CreateTexture(GL_TEXTURE_2D, format, width, height, data) };
}

namespace
{
void UpdateTexture(Texture* texture, GLenum target, const MemoryBlock* block)
{
	GLenum internal_format;
	GLenum format;
	GLenum type;
	GetTextureFormats(texture->format, &internal_format, &format, &type);
	glBindTexture(target, texture->name);
	glTexImage2D(target, 0, internal_format, texture->width, texture->height, 0, format, type, block->ptr);
	DeallocateBlock(block);
}
}

void gl::UpdateTexture2D(TextureHandle handle, const MemoryBlock* data)
{
	UpdateTexture(&textures[handle.index], GL_TEXTURE_2D, data);
}
#pragma endregion

#pragma region Uniforms

UniformHandle gl::CreateUniform(const char * name, UniformType type)
{
	auto uniform = uniforms.Create();
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

void gl::SetUniform(UniformHandle handle, const void* data, int num)
{
	auto type = uniforms[handle.index].type;
	uniform_buffer.Add(handle, type, num, data);
}

#pragma endregion

#pragma region Destroy Functions

void gl::Destroy(ProgramHandle h)
{
	//NOTE: Do I want to destroy all the uniforms associated with this program? Probably by default yes.
	for (int i = 0; i < programs[h.index].num_uniforms; i++) {
		gl::Destroy(programs[h.index].uniform_handles[i]);
	}

	glDeleteProgram(programs[h.index].id);
	programs.Remove(h.index);
}

void gl::Destroy(VertexBufferHandle h)
{
	glDeleteBuffers(1, &vertex_buffers[h.index].buffer);
	vertex_buffers.Remove(h.index);
}


void	gl::Destroy(IndexBufferHandle h)
{
	glDeleteBuffers(1, &index_buffers[h.index].buffer);
	index_buffers.Remove(h.index);
}

void	gl::Destroy(TextureHandle h)
{

}

void gl::Destroy(UniformHandle h)
{
	uniforms.Remove(h.index);
}

#pragma endregion

void gl::SetState(uint64_t flags)
{
	current_draw.render_state = flags;
}

void gl::SetVertexBuffer(VertexBufferHandle h, uint32_t first_vertex, uint32_t num_verts)
{
	current_draw.vertex_buffer = h.index;
	current_draw.vertex_offset = first_vertex;
	current_draw.vertex_count = num_verts;
}

void gl::SetIndexBuffer(IndexBufferHandle h, uint32_t offset, uint32_t num_elements)
{
	current_draw.index_buffer = h.index;
	current_draw.index_offset = offset;
	current_draw.index_count = num_elements;
}

void gl::SetTexture(TextureHandle tex, UniformHandle sampler, uint16_t texture_unit)
{
	GLint unit = texture_unit;
	SetUniform(sampler, &unit);
	current_rendercmd.textures[texture_unit] = tex;
}

void gl::SetShaderStorageBuffer(SSBOHandle h, uint32_t binding)
{
	current_rendercmd.ssbos[binding] = shader_storage_buffers[h.index].buffer;
}

void gl::SetAtomicCounterBuffer(AtomicCounterBufferHandle h, uint32_t binding)
{
	current_rendercmd.atomic_counters[binding] = atomic_counter_buffers[h.index].buffer;
}

void gl::SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	current_draw.scissor[0] = x;
	current_draw.scissor[1] = y;
	current_draw.scissor[2] = w;
	current_draw.scissor[3] = h;
}

void gl::Submit(uint8_t layer, ProgramHandle program, uint32_t depth, bool preserve_state)
{
	//Create a sort key for this draw call
	Key key;
	key.layer = layer;
	key.compute = false;
	key.program = program.index;
	key.depth = depth;

	unsigned int index = key_index++;
	key.sequence = index;//TODO: Something about sequential rendering needs to go here.
	auto encoded_key = key.Encode();

	current_rendercmd.program = program;
	//Handle Uniforms
	auto uniform_end = uniform_buffer.GetWritePosition();
	current_rendercmd.uniform_end = uniform_end;

	auto draw_index = render_buffer_count++;
	render_buffer[draw_index] = current_draw;
	memcpy(&render_buffer[draw_index], &current_rendercmd, sizeof(RenderCmd));

	keys[index] = { encoded_key,  &render_buffer[draw_index] };

	if (!preserve_state) {
		current_draw = DrawCmd{};
		current_rendercmd = RenderCmd{};
	}
	current_rendercmd.uniform_start = uniform_end;
}

void gl::SubmitCompute(uint8_t layer, ProgramHandle program, uint16_t num_x, uint16_t num_y, uint16_t num_z)
{
	Key key;
	key.layer = layer;
	key.compute = true;
	key.program = program.index;
	key.depth = 0; //Could probably find something else to do with these bits. Have 32 bits to use to encode... something?

	//This isn't right - need to figure out another way. Just reset uniform start on render.

	unsigned int index = key_index++;
	key.sequence = 0;//TODO: Something about sequential rendering needs to go here.
	auto encoded_key = key.Encode();

	current_rendercmd.program = program;
	current_compute.x = num_x;
	current_compute.y = num_y;
	current_compute.z = num_z;

	//Handle Uniforms
	auto uniform_end = uniform_buffer.GetWritePosition();
	current_rendercmd.uniform_end = uniform_end;


	auto compute_index = compute_buffer_count++;
	compute_buffer[compute_index] = current_compute;
	memcpy(&compute_buffer[compute_index], &current_rendercmd, sizeof(RenderCmd));
	keys[index] = { encoded_key, &compute_buffer[compute_index] };

	current_compute = ComputeCmd{};
	current_rendercmd = RenderCmd{};
	current_rendercmd.uniform_start = uniform_end;
}

namespace
{

GLFWwindow* current_window = nullptr;

}

void gl::Render()
{
	//Do any pre-render stuff wrt resources that need management.
	frame++;
	SortKeys();

	//Need to store the current state of the important stuff
	IndexBufferHandle index_buffer_handle{ INVALID_HANDLE };
	VertexBufferHandle vertex_buffer_handle{ INVALID_HANDLE };
	ProgramHandle program_handle{ INVALID_HANDLE };

	TextureHandle bound_textures[MAX_TEXTURE_UNITS];
	for (int tex_unit = 0; tex_unit < MAX_TEXTURE_UNITS; tex_unit++) {
		bound_textures[tex_unit].index = INVALID_HANDLE;
	}
	static uint64_t raster_state = RenderState::DEFAULT_STATE;
	GLenum primitive_type = GL_TRIANGLES;

	Program* program = 0;

	int framebuffer_size[4];
	glGetIntegerv(GL_VIEWPORT, framebuffer_size);
	uint32_t scissor[4] = { 0u, 0u, framebuffer_size[2], framebuffer_size[3] };
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);


	unsigned int num_commands = key_index;
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (unsigned int command_index = 0; command_index < num_commands; command_index++) {
		auto encoded_key = keys[command_index];

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
		uniform_buffer.Execute(program, cmd->uniform_start, cmd->uniform_end);

		//Set textures:
		for (int tex_unit = 0; tex_unit < MAX_TEXTURE_UNITS; tex_unit++) {
			if (cmd->textures[tex_unit].index != INVALID_HANDLE
				&& bound_textures[tex_unit].index != cmd->textures[tex_unit].index) {
				auto& tex = textures[cmd->textures[tex_unit].index];
				glActiveTexture(GL_TEXTURE0 + tex_unit);
				glBindTexture(tex.target, tex.name);
				bound_textures[tex_unit] = cmd->textures[tex_unit];
			}
		}

		//Bind any SSBOs:
		for (int ssbo_binding = 0; ssbo_binding < MAX_SSBO_BINDINGS; ssbo_binding++) {
			if (cmd->ssbos[ssbo_binding] != UINT32_MAX) {
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
								 ssbo_binding,
								 cmd->ssbos[ssbo_binding]);
			}
		}

		//Bind any atomic counters. May be able to unify the buffer bindings.
		for (int binding = 0; binding < MAX_ATOMIC_COUNTER_BINDINGS; binding++) {
			if (cmd->atomic_counters[binding] != UINT32_MAX) {
				glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, binding, cmd->atomic_counters[binding]);
			}
		}

		//Compute commands handled here
		if (key.compute) {
			auto compute_cmd = reinterpret_cast<ComputeCmd*>(cmd);
			glDispatchCompute(compute_cmd->x, compute_cmd->y, compute_cmd->z);
			continue;
		}

		//Everything else is a draw command
		auto draw_cmd = reinterpret_cast<DrawCmd*>(cmd);

		//Check view and update it
		//TODO: Views!!!

		//Update rasterization state
		if (raster_state != draw_cmd->render_state) //Raster state has changed.
		{
			//Depth test
			if (((raster_state ^ draw_cmd->render_state) & RenderState::DEPTH_TEST_MASK) != 0) {
				//NOTE: This list has to match the order in the RenderState enum. I don't love that brittleness.
				if ((raster_state & RenderState::DEPTH_TEST_MASK) == RenderState::DEPTH_TEST_OFF) {
					glEnable(GL_DEPTH_TEST);
				}

				if ((draw_cmd->render_state & RenderState::DEPTH_TEST_MASK) == RenderState::DEPTH_TEST_OFF) {
					glDisable(GL_DEPTH_TEST);
				} else {
					constexpr GLenum depth_funcs[] = { GL_LESS, GL_LEQUAL, GL_EQUAL, GL_GEQUAL,
						GL_GREATER, GL_NOTEQUAL, GL_NEVER, GL_ALWAYS };
					int depth_func_index = (draw_cmd->render_state & RenderState::DEPTH_TEST_MASK) >> RenderState::DEPTH_TEST_SHIFT;
					if (depth_func_index != 0) {
						glDepthFunc(depth_funcs[depth_func_index - 1]);
					}
				}
			}

			//Blending function
			if (((raster_state ^ draw_cmd->render_state) & RenderState::BLEND_MASK) != 0) {

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
			if (((raster_state ^ draw_cmd->render_state) & RenderState::BLEND_EQUATION_MASK) != 0) {

				constexpr GLenum blend_eqs[] = { GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX };
				int blend_eq_index = (draw_cmd->render_state & RenderState::BLEND_EQUATION_MASK) >> RenderState::BLEND_EQUATION_SHIFT;
				if (blend_eq_index != 0) {
					glBlendEquation(blend_eqs[blend_eq_index - 1]);
				}
			}

			//Culling settings
			if (((raster_state ^ draw_cmd->render_state) & RenderState::CULL_MASK) != 0) {
				//Culling state
				auto cull_state = draw_cmd->render_state & RenderState::CULL_MASK;
				if ((raster_state & RenderState::CULL_MASK) == RenderState::CULL_OFF) {
					glEnable(GL_CULL_FACE);
				} else if (cull_state == RenderState::CULL_OFF) {
					glDisable(GL_CULL_FACE);
				}

				if (cull_state == RenderState::CULL_CW) {
					glFrontFace(GL_CW);
				} else if (cull_state == RenderState::CULL_CCW) {
					glFrontFace(GL_CCW);
				}
			}

			if (((raster_state ^ draw_cmd->render_state) & RenderState::PRIMITIVE_MASK) != 0) {
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
		if (draw_cmd->scissor[2] == UINT32_MAX) {
			draw_cmd->scissor[2] = framebuffer_size[2];
		}
		if (draw_cmd->scissor[3] == UINT32_MAX) {
			draw_cmd->scissor[3] = framebuffer_size[3];
		}
		if (std::memcmp(scissor, draw_cmd->scissor, sizeof(scissor)) != 0) {
			glScissor(draw_cmd->scissor[0], draw_cmd->scissor[1], draw_cmd->scissor[2], draw_cmd->scissor[3]);
			std::memcpy(scissor, draw_cmd->scissor, sizeof(scissor));
		}

		//VAOs - lookup the appropriate one, create it if necessary.
		{
			if (index_buffer_handle.index != draw_cmd->index_buffer ||
				vertex_buffer_handle.index != draw_cmd->vertex_buffer) {
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
				} else {
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
					auto num_verts = vertex_buffer.size / vertex_size;
					uint32_t attrib_offset = 0;
					for (int i = 0; i < vertex_layout.MAX_ATTRIBUTES; i++) {
						if (vertex_layout.types[i] == render::VertexLayout::AttributeType::UNUSED) {
							continue;
						}
						/*
						Pack attribute info into 8 bits:
						76543210
						xnttttss
						x - reserved
						n - normalized
						t - type
						s - number
						*/
						
						auto count = vertex_layout.counts[i];
						uint8_t attrib_loc = (count & 0b0111'1100) >> 2;
						bool normalized = (bool)(count & 0b1000'0000) >> 7;
						GLuint size = (count & 0b0000'0011) + 1;
						glVertexAttribPointer(attrib_loc, size, 
											  GetGLEnum(vertex_layout.types[i]), normalized, 
											  0, 
											  (GLvoid*)attrib_offset);
						glEnableVertexAttribArray(attrib_loc);
						if (vertex_layout.interleaved) {
							attrib_offset += size*render::VertexLayout::SizeOfType(vertex_layout.types[i]);
						} else {
							attrib_offset += num_verts*size*render::VertexLayout::SizeOfType(vertex_layout.types[i]);
						}
					}
				}
			}
		}

		//Draw.
		if (index_buffer_handle.index != INVALID_HANDLE) {
			auto& ib = index_buffers[index_buffer_handle.index];

			unsigned int offset = draw_cmd->index_offset;
			if (draw_cmd->index_offset != 0) {
				switch (ib.type) {
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
									 (void*)offset,
									 draw_cmd->vertex_offset);
		} else {
			//NOTE: Take a look, vertex offset might have the same problem as index offset did.
			auto& vb = vertex_buffers[vertex_buffer_handle.index];
			auto num_verts = vb.size / vb.layout.SizeOfVertex();
			if (draw_cmd->vertex_count != UINT32_MAX) {
				num_verts = draw_cmd->vertex_count;
			}
			glDrawArrays(primitive_type, draw_cmd->vertex_offset, num_verts);
		}
	}

	key_index = 0;
	render_buffer_count = 0;
	compute_buffer_count = 0;
	uniform_buffer.Clear();
	FreeMemory();
	current_rendercmd.uniform_start = 0;
	
}

void gl::InitializeBackend(GLFWwindow* window)
{
	glfwMakeContextCurrent(window);
	current_window = window;
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
	return;
}