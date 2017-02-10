#pragma once

#include "Utilities/Geometry.h"
#include "External/GLFW/glfw3.h"
#include "Utilities/Allocators.h"

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

void Initialize(GLFWwindow* window);
void ResizeWindow(int w, int h);

void Create(MeshObject *mesh_object);
//void UpdateMeshData(RenderHandle mesh, const MemoryBlock* vertex_data, const MemoryBlock* index_data);


void EndFrame();

//
//IMGUI FUNCTIONS
//

void InitImguiRendering(const MemoryBlock* font_data, int width, int height);
void UpdateImguiData(const MemoryBlock* vertex_data, const MemoryBlock* index_data, const Vec2& display_size);
void DrawImguiCmd(uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, uint32_t scissor_x, uint32_t scissor_y, uint32_t scissor_w, uint32_t scissor_h);




}
}

