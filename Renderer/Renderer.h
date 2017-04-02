#pragma once

/*
	An opengl render backend, allowing multithreaded draw call submission, based on draw buckets. 
	Right now, requires an opengl context to be created externally and current on the thread which calls Initialize and Render.
	All other calls can only be made from the same thread. 

	Ideally, this would be relatively easy to port to different backends, 
	but for now, I'm only concerned with opengl.
*/

#include <cstdint>
#include <limits>
#include "../Utilities/Utilities.h"
#include "RenderInterface.h"

/*Enables some debug information - less efficient, stores some strings it otherwise wouldn't keep.*/
#define RENDER_DEBUG
struct GLFWwindow;

namespace rkg
{
namespace gl
{

#define RENDER_HANDLE(name)\
	struct name { uint32_t index; };

RENDER_HANDLE(VertexBufferHandle);
RENDER_HANDLE(IndexBufferHandle);
RENDER_HANDLE(ProgramHandle);
RENDER_HANDLE(UniformHandle);
RENDER_HANDLE(TextureHandle);
RENDER_HANDLE(BufferHandle);

constexpr uint32_t INVALID_HANDLE = std::numeric_limits<uint32_t>::max();

constexpr uint32_t MAX_DRAWS_PER_THREAD = 4096;
constexpr uint32_t MAX_DRAWS_PER_FRAME = 4 * MAX_DRAWS_PER_THREAD;
constexpr uint32_t MAX_RENDER_LAYERS = 256;
constexpr uint32_t MAX_VERTEX_BUFFERS = 1024;
constexpr uint32_t MAX_INDEX_BUFFERS = 1024;
constexpr uint32_t MAX_SHADER_STORAGE_BUFFERS = 64;
constexpr uint32_t MAX_ATOMIC_COUNTER_BUFFERS = 64;
constexpr uint32_t MAX_SSBO_BINDINGS = 8;
constexpr uint32_t MAX_ATOMIC_COUNTER_BINDINGS = 8;
constexpr uint32_t MAX_BUFFER_OBJECTS = 4096;
constexpr uint32_t MAX_BUFFER_BINDINGS = 16;
constexpr uint32_t MAX_TEXTURES = 1024;
constexpr uint32_t MAX_TEXTURE_UNITS = 16;
constexpr uint32_t MAX_UNIFORMS = 256;
constexpr uint32_t MAX_SHADER_PROGRAMS = 1024; //No clue what a normal number is for this.
constexpr uint32_t MAX_VERTEX_ARRAY_OBJECTS = 1024;

/*
	Render state - gets reset after every draw call issued.
	We cram it into a 64 bit integer, so that's nice. Easy to set/reset.
*/
namespace RenderState
{
enum Enum : uint64_t
{
	RGB_WRITE = 0x0000000000000001,
	ALPHA_WRITE = 0x0000000000000002,
	DEPTH_WRITE = 0x0000000000000004,
	//Depth tests	
	DEPTH_TEST_LESS = 0x0000000000000010,
	DEPTH_TEST_LEQUAL = 0x0000000000000020,
	DEPTH_TEST_EQUAL = 0x0000000000000030,
	DEPTH_TEST_GEQUAL = 0x0000000000000040,
	DEPTH_TEST_GREATER = 0x0000000000000050,
	DEPTH_TEST_NOTEQUAL = 0x0000000000000060,
	DEPTH_TEST_NEVER = 0x0000000000000070,
	DEPTH_TEST_ALWAYS = 0x0000000000000080,
	DEPTH_TEST_OFF = 0x0000000000000090,
	DEPTH_TEST_MASK = 0x00000000000000f0,
	DEPTH_TEST_SHIFT = 4,
	//Blending func	
	BLEND_ZERO = 0x0000000000000100,
	BLEND_ONE = 0x0000000000000200,
	BLEND_SRC_COLOR = 0x0000000000000300,
	BLEND_ONE_MINUS_SRC_COLOR = 0x0000000000000400,
	BLEND_DST_COLOR = 0x0000000000000500,
	BLEND_ONE_MINUS_DST_COLOR = 0x0000000000000600,
	BLEND_SRC_ALPHA = 0x0000000000000700,
	BLEND_ONE_MINUS_SRC_ALPHA = 0x0000000000000800,
	BLEND_DST_ALPHA = 0x0000000000000900,
	BLEND_ONE_MINUS_DST_ALPHA = 0x0000000000000a00,
	BLEND_CONSTANT_COLOR = 0x0000000000000b00,
	BLEND_ONE_MINUS_CONSTANT_COLOR = 0x0000000000000c00,
	BLEND_CONSTANT_ALPHA = 0x0000000000000d00,
	BLEND_ONE_MINUS_CONSTANT_ALPHA = 0x0000000000000e00,
	BLEND_SRC_ALPHA_SATURATE = 0x0000000000000f00,
	BLEND_SRC1_COLOR = 0x000000000000a100,
	BLEND_ONE_MINUS_SRC1_COLOR = 0x000000000000a200,
	BLEND_SRC1_ALPHA = 0x000000000000a300,
	BLEND_ONE_MINUS_SRC1_ALPHA = 0x000000000000a400,
	BLEND_MASK = 0x000000000000ff00,
	BLEND_SHIFT = 8,
	//Blend equation	
	BLEND_EQUATION_ADD = 0x0000000000010000,
	BLEND_EQUATION_SUBTRACT = 0x0000000000020000,
	BLEND_EQUATION_REVERSE_SUBTRACT = 0x0000000000030000,
	BLEND_EQUATION_MIN = 0x0000000000040000,
	BLEND_EQUATION_MAX = 0x0000000000050000,
	BLEND_EQUATION_MASK = 0x00000000000f0000,
	BLEND_EQUATION_SHIFT = 16,
	//Culling	

	CULL_CW = 0x0000000000100000,
	CULL_CCW = 0x0000000000200000,
	CULL_OFF = 0x0000000000300000,
	CULL_MASK = 0x0000000000f00000,
	CULL_SHIFT = 20,
	//Primitive type	
	PRIMITIVE_TRIANGLES = 0x0000000000000000,
	PRIMITIVE_TRI_STRIP = 0x0000000001000000,
	PRIMITIVE_TRI_FAN = 0x0000000002000000,
	PRIMITIVE_POINTS = 0x0000000003000000,
	PRIMITIVE_LINE_STRIP = 0x0000000004000000,
	PRIMITIVE_LINE_LOOP = 0x0000000005000000,
	PRIMITIVE_LINES = 0x0000000006000000,
	PRIMITIVE_PATCHES = 0x0000000007000000,
	PRIMITIVE_MASK = 0x000000000f000000,
	PRIMITIVE_SHIFT = 24,
	//Default state.
	DEFAULT_STATE = RGB_WRITE | ALPHA_WRITE | DEPTH_WRITE | DEPTH_TEST_LESS | PRIMITIVE_TRIANGLES,
};
};

enum class UniformType : uint8_t
{
//4 byte types
	Sampler,
	Int,
	Uint,
	Float,

	Vec2,
	Ivec2,

	Vec3,
	Ivec3,

	Vec4,
	Ivec4,

	Mat3,
	Mat4,
	Count
};

enum class TextureFormat
{
//TODO: Get more texture formats working
	RGB8,
	RGBA8,
};

enum class BufferTarget
{
	SHADER_STORAGE,
	UNIFORM,
	ATOMIC_COUNTER,
};

using ErrorCallbackFn = void(*)(const char* msg);

void InitializeBackend(GLFWwindow* window);
void SetErrorCallback(ErrorCallbackFn f);
void Resize(int w, int h);


/*==================== Resource Management ====================*/

//Memory! For now, really simple. Just to get memory controlled by the renderer.
#pragma region Memory Management
using ReleaseFunction = void(*) (MemoryBlock, void* user_data);

const MemoryBlock*	Alloc(const uint32_t size);
const MemoryBlock*	AllocAndCopy(const void * const data, const uint32_t size);
const MemoryBlock*	MakeRef(const void* data, const uint32_t size, ReleaseFunction = nullptr, void* user_data = nullptr);//Creates a reference to memory which is managed by the user. Must be kept for two frames 
const MemoryBlock*	LoadShaderFile(const char * file);
#pragma endregion

#pragma region Layer Functions
	//Create a new layer to render on - draws will be sorted by their layer.
static constexpr uint8_t DEFAULT_LAYER{ 0 };


ProgramHandle	CreateProgram(const MemoryBlock* vertex_shader, const MemoryBlock* frag_shader);
ProgramHandle	CreateComputeProgram(const MemoryBlock* compute_shader);

unsigned int	GetNumUniforms(ProgramHandle h); //NOTE: This function must be called a frame after the program was created.
int	GetProgramUniforms(ProgramHandle h, UniformHandle* buffer, int size);
void	GetUniformInfo(UniformHandle h, char* name, int name_size, UniformType* type);
void GetUniformBlockInfo(ProgramHandle h, const char* block_name, render::PropertyBlock* block);
#pragma endregion

#pragma region Vertex Buffer Functions
VertexBufferHandle	CreateVertexBuffer(const MemoryBlock* data, const render::VertexLayout& layout);
VertexBufferHandle	CreateDynamicVertexBuffer(const MemoryBlock* data, const render::VertexLayout& layout);
VertexBufferHandle	CreateDynamicVertexBuffer(const render::VertexLayout& layout);
void	UpdateDynamicVertexBuffer(VertexBufferHandle handle, const MemoryBlock* data, const ptrdiff_t offset = 0);

#pragma endregion

#pragma region Index Buffer Functions
IndexBufferHandle	CreateIndexBuffer(const MemoryBlock* data, render::IndexType type);
IndexBufferHandle	CreateDynamicIndexBuffer(const MemoryBlock* data, render::IndexType type);
IndexBufferHandle	CreateDynamicIndexBuffer(render::IndexType type);
void	UpdateDynamicIndexBuffer(IndexBufferHandle handle, const MemoryBlock* data, const ptrdiff_t offset = 0);
#pragma endregion

#pragma region Texture Functions
TextureHandle CreateTexture2D(uint16_t width, uint16_t height, TextureFormat format, const MemoryBlock* data = nullptr);
void UpdateTexture2D(TextureHandle handle, const MemoryBlock* data);
#pragma endregion



UniformHandle	CreateUniform(const char* name, UniformType type);
//Sets uniforms - these will be set on the program of whatever the next submit call is on the calling thread.
void SetUniform(UniformHandle handle, const void* data, int num = 1);

BufferHandle CreateBufferObject(const MemoryBlock* data = nullptr);
void UpdateBufferObject(BufferHandle handle, const MemoryBlock* data);

//Because handles are all unique types, we can have nice overloading to keep things simple.
//TODO: Right now these don't do anything.
void	Destroy(ProgramHandle);
void	Destroy(VertexBufferHandle);
void	Destroy(IndexBufferHandle);
void	Destroy(TextureHandle);
void	Destroy(UniformHandle);
void	Destroy(BufferHandle);
//TODO: None of these are implemented yet. 

//Set state for the next draw call on this thread - thread local. 
void SetState(uint64_t flags);

void SetVertexBuffer(VertexBufferHandle h, uint32_t first_vertex = 0, uint32_t num_verts = UINT32_MAX);
void SetIndexBuffer(IndexBufferHandle h, uint32_t first_element = 0, uint32_t num_elements = UINT32_MAX);

void SetTexture(TextureHandle tex, UniformHandle sampler, uint16_t texture_unit);
void SetBufferObject(BufferHandle h, BufferTarget target, uint32_t binding);

void SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h);



//Submit a draw call using the currently bound buffers.
void Submit(uint8_t layer, ProgramHandle program, uint32_t depth = 0, bool preserve_state = false);
void SubmitCompute(uint8_t layer, ProgramHandle program, uint16_t num_x = 1, uint16_t num_y = 1, uint16_t num_z = 1);

void Render();//Submit the frame to the renderer.


}//End namespace render
}//end namespace rkg