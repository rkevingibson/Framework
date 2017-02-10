#include "RenderInterface.h"
#include "Utilities/CommandStream.h"
#include "Renderer.h"
#include <atomic>
#include <thread>

using namespace rkg;

namespace
{


enum class RenderObjectType
{
	MESH,
	IMGUI,
};

//Everything that exists on the render thread will inherit from this object
struct RenderThreadObject
{
	RenderObjectType type;
};


struct RenderMesh : public RenderThreadObject
{
	gl::VertexBufferHandle vertex_buffer;
	gl::IndexBufferHandle index_buffer;
	Mat4 transform;

};

std::atomic_flag render_fence{ ATOMIC_FLAG_INIT };
std::atomic_flag game_fence{ ATOMIC_FLAG_INIT };
CommandStream render_commands;


void RenderLoop(GLFWwindow* window)
{
	gl::InitializeBackend(window);
	while (true) {
		while (render_fence.test_and_set(std::memory_order_acquire)) {
			std::this_thread::yield();
		} //Spin while this flag hasn't been set by the other thread.
		render_commands.SwapBuffers(); //Need to signal to the other threads that they can now make render calls.
		game_fence.clear();
		//Update our state by pumping the command list. This syncs state between the game + render threads.
		render_commands.ExecuteAll();

		//Other rendering happens here. All calls to the render backend occur here.


		gl::Render();
		glfwSwapBuffers(window);
	}
}


}

void ResizeWindow(int w, int h)
{

}

void render::Initialize(GLFWwindow* window)
{
	//Spawn thread and 
	glfwMakeContextCurrent(nullptr);
	std::thread render_thread(RenderLoop, window);
	render_thread.detach();

}

void render::ResizeWindow(int w, int h)
{
	struct CmdType : Cmd
	{
		int w, h;
	} cmd;
	cmd.w = w;
	cmd.h = h;
	cmd.dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		glViewport(0, 0, data->w, data->h);
	};
	render_commands.Push(cmd);
}

void render::Create(render::MeshObject* mesh_object)
{


}

//void render::UpdateMeshData(RenderHandle mesh, const MemoryBlock * vertex_data, const MemoryBlock * index_data)
//{
//	struct CmdType : Cmd
//	{
//		RenderHandle mesh;
//		const MemoryBlock* vertex_data;
//		const MemoryBlock* index_data;
//	} cmd;
//	cmd.mesh = mesh;
//	cmd.vertex_data = vertex_data;
//	cmd.index_data = index_data;
//	cmd.dispatch = [](Cmd* cmd) {
//		auto data = reinterpret_cast<CmdType*>(cmd);
//		//Look up the mesh from the handle.
//
//
//		if (data->vertex_data) {
//			gl::UpdateDynamicVertexBuffer(, data->vertex_data);
//		}
//		if (data->index_data) {
//			gl::UpdateDynamicIndexBuffer(, data->index_data);
//		}
//	};
//	render_commands.Push(cmd);
//}


void render::EndFrame()
{
	render_fence.clear();
	while (game_fence.test_and_set(std::memory_order_acquire)) {
		std::this_thread::yield();
	}
}



//
//IMGUI FUNCTIONS
//

namespace
{
struct
{
	gl::VertexBufferHandle vertex_buffer;
	gl::IndexBufferHandle index_buffer;
	gl::UniformHandle projection_matrix;
	gl::UniformHandle font_sampler;
	gl::ProgramHandle program;
	gl::TextureHandle texture;
	Vec2 display_size;
	uint8_t render_layer{ 2 };
} imgui;
}

void render::InitImguiRendering(const MemoryBlock* font_data, int width, int height)
{
	struct CmdType :Cmd
	{
		const MemoryBlock* font_data;
		int width, height;

	} cmd;
	cmd.font_data = font_data;
	cmd.width = width;
	cmd.height = height;
	cmd.dispatch = [](Cmd* cmd) {
		const char vertex_shader[] =
			"#version 330\n"
			"uniform mat4 ProjMtx;\n"
			"in vec2 Position;\n"
			"in vec2 UV;\n"
			"in vec4 Color;\n"
			"out vec2 Frag_UV;\n"
			"out vec4 Frag_Color;\n"
			"void main()\n"
			"{\n"
			"	Frag_UV = UV;\n"
			"	Frag_Color = Color;\n"
			"	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
			"}\n";

		const char fragment_shader[] =
			"#version 330\n"
			"uniform sampler2D Texture;\n"
			"in vec2 Frag_UV;\n"
			"in vec4 Frag_Color;\n"
			"out vec4 Out_Color;\n"
			"void main()\n"
			"{\n"
			"	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
			"}\n";

		auto vert_block = gl::AllocAndCopy(vertex_shader, sizeof(vertex_shader));
		auto frag_block = gl::AllocAndCopy(fragment_shader, sizeof(fragment_shader));
		imgui.program = gl::CreateProgram(vert_block, frag_block);

		imgui.font_sampler = gl::CreateUniform("Texture", gl::UniformType::Sampler);
		imgui.projection_matrix = gl::CreateUniform("ProjMtx", gl::UniformType::Mat4);

		auto data = reinterpret_cast<CmdType*>(cmd);
		imgui.texture = gl::CreateTexture2D(data->width, data->height, gl::TextureFormat::RGBA8, data->font_data);

		gl::VertexLayout vert_layout;
		vert_layout
			.Add("Position", 2, gl::VertexLayout::AttributeType::Float32)
			.Add("UV", 2, gl::VertexLayout::AttributeType::Float32)
			.Add("Color", 4, gl::VertexLayout::AttributeType::Uint8, true);
		imgui.vertex_buffer = gl::CreateDynamicVertexBuffer(vert_layout);
		imgui.index_buffer = gl::CreateDynamicIndexBuffer(gl::IndexType::UShort);
	};

	render_commands.Push(cmd);
}

void render::UpdateImguiData(const MemoryBlock* vertex_data, const MemoryBlock* index_data, const Vec2& size)
{
	struct CmdType : Cmd
	{
		const MemoryBlock* vert_data;
		const MemoryBlock* index_data;
		Vec2 display_size;
	} cmd;
	cmd.vert_data = vertex_data;
	cmd.index_data = index_data;
	cmd.display_size = size;
	cmd.dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		gl::UpdateDynamicVertexBuffer(imgui.vertex_buffer, data->vert_data);
		gl::UpdateDynamicIndexBuffer(imgui.index_buffer, data->index_data);
		imgui.display_size = data->display_size;
	};

	render_commands.Push(cmd);
}

void rkg::render::DrawImguiCmd(uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, uint32_t scissor_x, uint32_t scissor_y, uint32_t scissor_w, uint32_t scissor_h)
{
	struct CmdType : Cmd
	{
		uint32_t vertex_offset, index_offset, index_count;
		uint32_t x, y, w, h;
	} cmd;


	cmd.dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		constexpr uint64_t raster_state = 0 |
			gl::RenderState::BLEND_EQUATION_ADD |
			gl::RenderState::BLEND_ONE_MINUS_SRC_ALPHA |
			gl::RenderState::CULL_OFF |
			gl::RenderState::DEPTH_TEST_OFF;
		gl::SetState(raster_state);

		const float ortho_projection[4][4] =
		{
			{ 2.0 / imgui.display_size.x, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 2.0f / -imgui.display_size.y, 0.0f, 0.0f },
			{ 0.0f, 0.0f, -1.0f, 0.0f },
			{ -1.0f, 1.0f, 0.0f, 1.0f },
		};

		gl::SetUniform(imgui.projection_matrix, ortho_projection);

		gl::SetTexture(imgui.texture, imgui.font_sampler, 0);
		gl::SetScissor(data->x, data->y, data->w, data->h);
		gl::SetVertexBuffer(imgui.vertex_buffer, data->vertex_offset);
		gl::SetIndexBuffer(imgui.index_buffer, data->index_offset, data->index_count);
		gl::Submit(imgui.render_layer, imgui.program);
	};

	render_commands.Push(cmd);

}





