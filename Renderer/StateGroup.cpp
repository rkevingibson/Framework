#include <cstdint>

#include "Renderer.h"

namespace rkg
{
namespace gl
{

	/*
	I've been spinning my wheels on this for a while now. 
	Let's break down the data into clear chunks.
	
	*/

	struct RasterizerState
	{
		enum PolygonMode_
		{
			PolygonMode_Fill = 1,
			PolygonMode_Line,
			PolygonMode_Point
		};

		enum CullDirection_
		{
			CullDirection_CCW = 1,
			CullDirection_CW
		};

		enum CullMode_
		{
			CullMode_Disabled = 1,
			CullMode_FrontFace,
			CullMode_BackFace,
			CullMode_BothFaces
		};

		uint8_t polygon_mode : 2;  //0 = not set, 1 = GL_FILL, 2 = GL_LINE, 3 = GL_POINT.
		uint8_t cull_direction : 2; //0 = not set, 1 = ccw, 2 = cw; 
		uint8_t cull_mode : 4;     //0 = not set, 1 = no culling, 2 = front, 3 = back, 4 = both.
		
		enum Scissor_
		{
			Scissor_Disabled = 1,
			Scissor_Enabled
		};

		uint8_t scissor_state : 2; //0 = not set, 1 = disabled, 2 = enabled.
		//TODO: How to store scissor box? How often is it used?
		int scissor_box[4];
	};

	struct DepthState
	{
		uint8_t depth_test : 2; // 0 = not set, 1 = disabled, 2 = enabled;
		uint8_t depth_write : 2; // 0 = not set, 1 = disabled, 2 = enabled;
		
		enum DepthFunc_
		{
			DepthFunc_Never = 1,
			DepthFunc_Less,
			DepthFunc_Equal,
			DepthFunc_Lequal,
			DepthFunc_Greater,
			DepthFunc_NotEqual,
			DepthFunc_Gequal,
			DepthFunc_Always
		};

		uint8_t depth_func : 4;
	};

	struct StencilState
	{
		//TODO: Stencil buffer state.

		//This needs a better name.
		uint8_t enabled : 2; //0 = not set, 1 = enabled, 2 = disabled;
		uint8_t front_function : 4;
		uint16_t front_operations;
		
	};

	struct BlendState
	{
		enum BlendFunc_
		{
			BlendFunc_Zero = 1,
			BlendFunc_One,
			BlendFunc_SrcColor,
			BlendFunc_OneMinusSrcColor,
			BlendFunc_DstColor,
			BlendFunc_OneMinusDstColor,
			BlendFunc_SrcAlpha,
			BlendFunc_OneMinusSrcAlpha,
			BlendFunc_DstAlpha,
			BlendFunc_OneMinusDstAlpha,
			BlendFunc_ConstantColor,
			BlendFunc_OneMinusConstantColor,
			BlendFunc_ConstantAlpha,
			BlendFunc_OneMinusConstantAlpha,
			BlendFunc_SrcAlphaSaturate,
			BlendFunc_Src1Color,
			BlendFunc_OneMinusSrc1Color,
			BlendFunc_Src1Alpha,
			BlendFunc_OneMinusSrc1Alpha
		};

		enum BlendEq_
		{
			BlendEq_Add = 1,
			BlendEq_Sub,
			BlendEq_ReverseSub,
			BlendEq_Min,
			BlendEq_Max
		};

		uint8_t buffer; // 0 - set for all buffers, otherwise set for buffer i-1. This could be potentially confusing.
		uint8_t func_rgb : 5;
		uint8_t eq_rgb : 3;
		uint8_t func_alpha : 5;
		uint8_t eq_alpha : 3;
	
		//TODO: Support for global BlendColor.
	};

	static constexpr size_t RASTERIZER_STATE_SIZE = sizeof(RasterizerState);
	static constexpr size_t DEPTH_STATE_SIZE = sizeof(DepthState);
	static constexpr size_t STENCIL_STATE_SIZE = sizeof(StencilState);
	static constexpr size_t BLEND_STATE_SIZE = sizeof(BlendState);
	struct TextureBinding
	{
		TextureHandle texture;
		uint32_t sampler_id;
	};

	static constexpr size_t TEXTURE_BINDING_ALIGNMENT = alignof(TextureBinding);
	static constexpr size_t TEXTURE_BINDING_SIZE = sizeof(TextureBinding);

	struct BufferBinding
	{
		BufferHandle buffer;
		enum class Target : uint8_t
		{
			UniformBufferObject,
			ShaderStorageBufferObject,
			AtomicCounter
		};
		Target target;
		uint32_t offset;
		uint32_t size;
	};

	static constexpr size_t BUFFER_BINDING_ALIGNMENT = alignof(BufferBinding);
	static constexpr size_t BUFFER_BINDING_SIZE = sizeof(BufferBinding);

	struct DrawCall
	{
		//Primitive Assembly. 
		VertexBufferHandle vertex_buffer{ 0 };
		IndexBufferHandle index_buffer{ 0 };

		//Shader program.
		ProgramHandle program{ 0 };

		//TODO: Framebuffers. 
		//FramebufferHandle framebuffer;
		//Blend state is per-framebuffer output. So 

		//Textures & Buffers
		static constexpr size_t MAX_TEXTURES = 16;
		static constexpr size_t MAX_BUFFERS = 16;
		
		//NOTE: We don't allow for partial overwriting of texture or buffer state - whole buffer bindings must be overwritten. 
		TextureBinding textures[MAX_TEXTURES];
		BufferBinding buffers[MAX_BUFFERS];

		RasterizerState rasterizer_state{ 0 };
		DepthState depth_state{ 0 };
	};
	static constexpr size_t DRAW_CALL_SIZE = sizeof(DrawCall);

	inline DrawCall* AllocateDrawCall()
	{
		//TODO: Arena allocator or something here.
		return (DrawCall*) malloc(sizeof(DrawCall));
	}

	inline DrawCall* Compile(DrawCall** calls, int num_calls)
	{
		//Resolve overwrites in order and create a final draw call.

		DrawCall* result = AllocateDrawCall();
		*result = *(calls[0]);
		RasterizerState* raster_state = &result->rasterizer_state;
		DepthState* depth_state = &result->depth_state;

		for (int i = 1; i < num_calls; i++) {
			result->vertex_buffer = (result->vertex_buffer.index == 0) ? calls[i]->vertex_buffer : result->vertex_buffer;
			result->index_buffer  = (result->index_buffer.index == 0)  ? calls[i]->index_buffer  : result->index_buffer;
			result->program = (result->program.index == 0) ? calls[i]->program : result->program;
			
			for (int j = 0; j < DrawCall::MAX_TEXTURES; j++) {
				if (result->textures[j].texture.index == 0) {
					result->textures[j] = calls[i]->textures[j];
				}
			}

			for (int j = 0; j < DrawCall::MAX_BUFFERS; j++) {
				if (result->buffers[j].buffer.index == 0) {
					result->buffers[j] = calls[i]->buffers[j];
				}
			}

			raster_state->polygon_mode = (raster_state->polygon_mode == 0) ? calls[i]->rasterizer_state.polygon_mode : 0;
			raster_state->cull_direction = (raster_state->cull_direction == 0) ? calls[i]->rasterizer_state.cull_direction : 0;
			raster_state->cull_mode = (raster_state->cull_direction == 0) ? calls[i]->rasterizer_state.cull_direction : 0;
			
			raster_state->scissor_box[0] = (raster_state->scissor_state == 0) ? calls[i]->rasterizer_state.scissor_box[0] : 0;
			raster_state->scissor_box[1] = (raster_state->scissor_state == 0) ? calls[i]->rasterizer_state.scissor_box[1] : 0;
			raster_state->scissor_box[2] = (raster_state->scissor_state == 0) ? calls[i]->rasterizer_state.scissor_box[2] : 0;
			raster_state->scissor_box[3] = (raster_state->scissor_state == 0) ? calls[i]->rasterizer_state.scissor_box[3] : 0;
			raster_state->scissor_state = (raster_state->scissor_state == 0) ? calls[i]->rasterizer_state.scissor_state : 0;

			depth_state->depth_func = (depth_state->depth_func == 0) ? calls[i]->depth_state.depth_func : 0;
			depth_state->depth_test = (depth_state->depth_test == 0) ? calls[i]->depth_state.depth_test : 0;
			depth_state->depth_write = (depth_state->depth_write == 0) ? calls[i]->depth_state.depth_write : 0;
		}

		return result;
	}

	inline void RenderDrawCall(DrawCall* draw)
	{
		//To start with, set rasterizer state and depth state.

		static RasterizerState current_rasterizer_state;
		static ProgramHandle current_program;
		static VertexBufferHandle current_vertex_buffer;
		static IndexBufferHandle current_vertex_buffer;

	}

}//End namespace gl
}//End namespace rkg