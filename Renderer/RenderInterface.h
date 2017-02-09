#pragma once

#include <vector>

#include "Renderer.h"
#include "Utilities/Geometry.h"
#include <memory>



namespace rkg
{
namespace render
{

using RenderHandle = uint64_t;

//Everything that has a counterpart on the renderthread will inherit from this object.
struct RenderSystemObject
{
	RenderHandle render_handle;
};



struct MeshObject : public RenderSystemObject
{

};


void Create(MeshObject *mesh_object);


void RenderLoop();

}
}

