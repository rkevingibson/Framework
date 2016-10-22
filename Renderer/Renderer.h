#pragma once

/*
	An opengl render backend, allowing multithreaded draw call submission, based on draw buckets. 
	Right now, requires an opengl context to be created externally and current on the thread which calls Initialize and Render.
	All other calls can be made from any thread, and are thread-safe. 

	Ideally, this would be relatively easy to port to different backends, 
	but for now, I'm only concerned with opengl.
*/

#include <cstdint>
#include <limits>

/*Enables some debug information - less efficient, stores some strings it otherwise wouldn't keep.*/
#define RENDER_DEBUG

struct GLFWwindow;

namespace render {

#define RENDER_HANDLE(name)\
	struct name { uint32_t index; };

	RENDER_HANDLE(VertexBufferHandle);
	RENDER_HANDLE(IndexBufferHandle);
	RENDER_HANDLE(LayerHandle);
	RENDER_HANDLE(ProgramHandle);
	RENDER_HANDLE(UniformHandle);
	RENDER_HANDLE(DynamicVertexBufferHandle);
	RENDER_HANDLE(DynamicIndexBufferHandle);
	RENDER_HANDLE(TextureHandle);


	constexpr uint32_t INVALID_HANDLE = std::numeric_limits<uint32_t>::max();

	constexpr uint32_t MAX_DRAWS_PER_THREAD = 2048;
	constexpr uint32_t MAX_DRAWS_PER_FRAME = 4 * MAX_DRAWS_PER_THREAD;
	constexpr uint32_t MAX_RENDER_LAYERS = 256;
	constexpr uint32_t MAX_VERTEX_BUFFERS = 1024;
	constexpr uint32_t MAX_INDEX_BUFFERS = 1024;
	constexpr uint32_t MAX_TEXTURES = 1024;
	constexpr uint32_t MAX_TEXTURE_UNITS = 16;
	constexpr uint32_t MAX_UNIFORMS = 256;
	constexpr uint32_t MAX_SHADER_PROGRAMS = 1024; //No clue what a normal number is for this.
	constexpr uint32_t MAX_VERTEX_ARRAY_OBJECTS = 1024;


	/*
		Render state - gets reset after every draw call issued.
		We cram it into a 64 bit integer, so that's nice. Easy to set/reset.
	*/
	namespace RenderState {
		enum Enum : uint64_t {
			RGB_WRITE =		0x0000000000000001,
			ALPHA_WRITE =	0x0000000000000002,
			DEPTH_WRITE =	0x0000000000000004,
			//Depth tests	
			DEPTH_TEST_LESS =		0x0000000000000010,
			DEPTH_TEST_LEQUAL =		0x0000000000000020,
			DEPTH_TEST_EQUAL =		0x0000000000000030,
			DEPTH_TEST_GEQUAL =		0x0000000000000040,
			DEPTH_TEST_GREATER =	0x0000000000000050,
			DEPTH_TEST_NOTEQUAL =	0x0000000000000060,
			DEPTH_TEST_NEVER =		0x0000000000000070,
			DEPTH_TEST_ALWAYS =		0x0000000000000080,
			DEPTH_TEST_OFF =		0x0000000000000090,
			DEPTH_TEST_MASK =		0x00000000000000f0,
			DEPTH_TEST_SHIFT = 4,
			//Blending func	
			BLEND_ZERO =						0x0000000000000100,
			BLEND_ONE =							0x0000000000000200,
			BLEND_SRC_COLOR =					0x0000000000000300,
			BLEND_ONE_MINUS_SRC_COLOR =			0x0000000000000400,
			BLEND_DST_COLOR =					0x0000000000000500,
			BLEND_ONE_MINUS_DST_COLOR =			0x0000000000000600,
			BLEND_SRC_ALPHA =					0x0000000000000700,
			BLEND_ONE_MINUS_SRC_ALPHA =			0x0000000000000800,
			BLEND_DST_ALPHA =					0x0000000000000900,
			BLEND_ONE_MINUS_DST_ALPHA =			0x0000000000000a00,
			BLEND_CONSTANT_COLOR =				0x0000000000000b00,
			BLEND_ONE_MINUS_CONSTANT_COLOR =	0x0000000000000c00,
			BLEND_CONSTANT_ALPHA =				0x0000000000000d00,
			BLEND_ONE_MINUS_CONSTANT_ALPHA =	0x0000000000000e00,
			BLEND_SRC_ALPHA_SATURATE =			0x0000000000000f00,
			BLEND_SRC1_COLOR =					0x000000000000a100,
			BLEND_ONE_MINUS_SRC1_COLOR =		0x000000000000a200,
			BLEND_SRC1_ALPHA =					0x000000000000a300,
			BLEND_ONE_MINUS_SRC1_ALPHA =		0x000000000000a400,
			BLEND_MASK =						0x000000000000ff00,
			BLEND_SHIFT = 8,
			//Blend equation	
			BLEND_EQUATION_ADD =				0x0000000000010000,
			BLEND_EQUATION_SUBTRACT =			0x0000000000020000,
			BLEND_EQUATION_REVERSE_SUBTRACT =	0x0000000000030000,
			BLEND_EQUATION_MIN =				0x0000000000040000,
			BLEND_EQUATION_MAX =				0x0000000000050000,
			BLEND_EQUATION_MASK =				0x00000000000f0000,
			BLEND_EQUATION_SHIFT = 16,
			//Culling	

			CULL_CW =	0x0000000000100000,
			CULL_CCW =	0x0000000000200000,
			CULL_OFF =	0x0000000000300000,
			CULL_MASK =	0x0000000000f00000,
			CULL_SHIFT = 20,
			//Primitive type	
			PRIMITIVE_TRIANGLES =	0x0000000000000000,
			PRIMITIVE_TRI_STRIP =	0x0000000001000000,
			PRIMITIVE_TRI_FAN =		0x0000000002000000,
			PRIMITIVE_POINTS =		0x0000000003000000,
			PRIMITIVE_LINE_STRIP =	0x0000000004000000,
			PRIMITIVE_LINE_LOOP =	0x0000000005000000,
			PRIMITIVE_LINES =		0x0000000006000000,
			PRIMITIVE_PATCHES =		0x0000000007000000,
			PRIMITIVE_MASK =		0x000000000f000000,
			PRIMITIVE_SHIFT = 24,
			//Default state.
			DEFAULT_STATE = RGB_WRITE | ALPHA_WRITE | DEPTH_WRITE | DEPTH_TEST_LESS | PRIMITIVE_TRIANGLES,
		};
	};

	enum class UniformType : uint8_t {
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

	//Types that can be used for an index buffer
	enum class IndexType {
		UByte,
		UShort,
		UInt,
	};

	enum class TextureFormat {
		//TODO: Get more texture formats working
		RGB8,
		RGBA8,
	};

	struct VertexLayout
	{

		struct AttributeType
		{
			enum Enum : uint8_t {
				Int8,
				Uint8,

				Int16,
				Uint16,
				Float16,

				Int32,
				Uint32,
				Packed_2_10_10_10_REV,//GL_INT_2_10_10_10_REV
				UPacked_2_10_10_10_REV,//GL_UNSIGNED_INT_2_10_10_10_REV
				Float32,

				Float64,
				Count
			};
		};

		static constexpr uint16_t MAX_ATTRIBUTES = 16;
		static constexpr uint16_t MAX_ATTRIBUTE_NAME_LENGTH = 16;
		//Add an attribute type.
		VertexLayout& Add(const char* name, uint16_t num, AttributeType::Enum type, bool normalized = false);

		//TODO: Figure out vertex layout description.
		inline uint16_t SizeOfVertex() {
			return stride;
		}


		VertexLayout& Clear();

		uint8_t num_attributes{ 0 };
		uint16_t stride{ 0 };
		uint16_t offset[MAX_ATTRIBUTES];
		uint8_t attribs[MAX_ATTRIBUTES];
		char names[MAX_ATTRIBUTES][MAX_ATTRIBUTE_NAME_LENGTH];
	};

	struct MemoryBlock
	{
		void * data;
		uint32_t size;
		uint64_t frame_to_delete; //Free the memory at the end of this frame.
	};

	using ErrorCallbackFn = void(*)(const char* msg);

	void Initialize(GLFWwindow* window);
	void SetErrorCallback(ErrorCallbackFn f);
	void Resize(int w, int h);


	/*==================== Resource Management ====================*/

	//Memory! For now, really simple. Just to get memory controlled by the renderer.
#pragma region Memory Management
	const MemoryBlock*	Alloc(const uint32_t size);
	const MemoryBlock*	AllocAndCopy(const void * const data, const uint32_t size);
	//MemoryBlock* CreateReference(const void* const data, const uint32_t size);//Create a reference to existing data. Must be free'd by the application, but not before it is used. 

	const MemoryBlock*	LoadShaderFile(const char * file);
#pragma endregion

#pragma region Layer Functions
	//Create a new layer to render on - draws will be sorted by their layer.
	static constexpr LayerHandle DEFAULT_LAYER{ 0 };

	
	LayerHandle	CreateLayer();
	ProgramHandle	CreateProgram(const MemoryBlock* vertex_shader, const MemoryBlock* frag_shader);
	unsigned int GetNumUniforms(ProgramHandle h); //NOTE: This function must be called a frame after the program was created.
	int GetProgramUniforms(ProgramHandle h, UniformHandle* buffer, int size);
	void GetUniformInfo(UniformHandle h, char* name, int name_size, UniformType* type);
#pragma endregion

#pragma region Vertex Buffer Functions
	VertexBufferHandle	CreateVertexBuffer(const MemoryBlock* data, const VertexLayout& layout);
	DynamicVertexBufferHandle	CreateDynamicVertexBuffer(const MemoryBlock* data, const VertexLayout& layout);
	DynamicVertexBufferHandle	CreateDynamicVertexBuffer(const VertexLayout& layout);
	void	UpdateDynamicVertexBuffer(DynamicVertexBufferHandle handle, const MemoryBlock* data, const ptrdiff_t offset = 0);

#pragma endregion

#pragma region Index Buffer Functions
	IndexBufferHandle	CreateIndexBuffer(const MemoryBlock* data, IndexType type);
	DynamicIndexBufferHandle CreateDynamicIndexBuffer(const MemoryBlock* data, IndexType type);
	DynamicIndexBufferHandle	CreateDynamicIndexBuffer(IndexType type);
	void	UpdateDynamicIndexBuffer(DynamicIndexBufferHandle handle, const MemoryBlock* data, const ptrdiff_t offset = 0);
#pragma endregion

#pragma region Texture Functions
	TextureHandle CreateTexture2D(uint16_t width, uint16_t height, TextureFormat format, const MemoryBlock* data = nullptr);
	void UpdateTexture2D(TextureHandle handle, const MemoryBlock* data);
#pragma endregion



	UniformHandle	CreateUniform(const char* name, UniformType type);
	//Sets uniforms - these will be set on the program of whatever the next submit call is on the calling thread.
	void SetUniform(UniformHandle handle, const void* data, int num = 1);


	//Because handles are all unique types, we can have nice overloading to keep things simple.
	//TODO: Right now these don't do anything.
	void	Destroy(LayerHandle);
	void	Destroy(ProgramHandle);
	void	Destroy(VertexBufferHandle);
	void	Destroy(DynamicVertexBufferHandle);	
	void	Destroy(IndexBufferHandle);
	void	Destroy(DynamicIndexBufferHandle);
	void	Destroy(TextureHandle);
	void	Destroy(UniformHandle);
	//TODO: None of these are implemented yet. 

	//Set state for the next draw call on this thread - thread local. 
	void SetState(uint64_t flags);

	void SetVertexBuffer(VertexBufferHandle h, uint32_t first_vertex = 0, uint32_t num_verts = UINT32_MAX);
	void SetVertexBuffer(DynamicVertexBufferHandle h, uint32_t first_vertex = 0, uint32_t num_verts = UINT32_MAX);
	void SetIndexBuffer(IndexBufferHandle h, uint32_t first_element = 0, uint32_t num_elements = UINT32_MAX);
	void SetIndexBuffer(DynamicIndexBufferHandle h, uint32_t first_element = 0, uint32_t num_elements = UINT32_MAX);
	void SetTexture(TextureHandle tex, UniformHandle sampler, uint16_t texture_unit);
	void SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h);




	//Submit a draw call using the currently bound buffers.
	void Submit(LayerHandle layer, ProgramHandle program, uint32_t depth = 0, bool preserve_state = false);
	void EndFrame();//Submit frame to render thread.
	void Render();//Submit the frame to the renderer.


}