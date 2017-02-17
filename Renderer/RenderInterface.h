#pragma once

#include "Utilities/Geometry.h"
#include "Utilities/Allocators.h"
#include "ECS/Entities.h"
#include "Renderer.h"
struct GLFWwindow;

namespace rkg
{
namespace render
{

using RenderHandle = uint64_t;

struct MeshComponent : ecs::Component
{
	RenderHandle mesh_handle;
	RenderHandle material_handle;

};


void Initialize(GLFWwindow* window);
void ResizeWindow(int w, int h);

MeshComponent* CreateMeshComponent(ecs::EntityID);
//void UpdateMeshData(RenderHandle mesh, const MemoryBlock* vertex_data, const MemoryBlock* index_data);

RenderHandle CreateMesh(const MemoryBlock* vertex_data, layout, const MemoryBlock* index_data, gl::IndexType type);


void EndFrame();

//
//IMGUI FUNCTIONS
//

void InitImguiRendering(const MemoryBlock* font_data, int width, int height);
void UpdateImguiData(const MemoryBlock* vertex_data, const MemoryBlock* index_data, const Vec2& display_size);
void DrawImguiCmd(uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, uint32_t scissor_x, uint32_t scissor_y, uint32_t scissor_w, uint32_t scissor_h);




}
}

