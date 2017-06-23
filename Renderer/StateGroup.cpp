#include <cstdint>

#include "Renderer.h"

namespace rkg
{
namespace gl
{

	struct StateGroup
	{
		//Shader program: needs a Technique ID and then flags for options. That comes once we do some shader system work.

		VertexBufferHandle vertex_buffer;
		IndexBufferHandle index_buffer;

		//Raster state:
		uint16_t raster_state;
		uint16_t blend_state;
		
		BufferHandle buffers[MAX_BUFFER_BINDINGS];
		BufferTarget buffer_targets[MAX_BUFFER_BINDINGS];
		TextureHandle textures[MAX_TEXTURE_UNITS];

	};
	
	struct DrawCall
	{
		StateGroup state;

	};
	
}


}