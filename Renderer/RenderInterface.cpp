#include "RenderInterface.h"

#include <array>
using namespace rkg;

namespace
{


enum class RenderObjectType
{
	MESH,
};

//Everything that exists on the render thread will inherit from this object
struct RenderThreadObject
{
	RenderObjectType type;
};


struct RenderMesh : public RenderThreadObject
{
	render::VertexBufferHandle vertex_buffer;
	render::IndexBufferHandle index_buffer;
	Mat4 transform;
	
};





}

void render::Create(render::MeshObject* mesh_object)
{



}


void render::RenderLoop()
{
	//Eventually, will be a forward+ renderer, but for now, just a simple forward one.


	//Update any global uniform buffers here - lights, etc.
}


