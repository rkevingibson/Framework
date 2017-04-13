#include "RenderInterface.h"
#include "FrameGraph.h"
#include "Utilities/CommandStream.h"
#include "Utilities/HashIndex.h"
#include "External/GLFW/glfw3.h"
#include "Renderer.h"
#include <atomic>
#include <thread>
#include <vector>


namespace rkg
{
namespace render
{


namespace
{

struct RenderGeometry
{
	gl::VertexBufferHandle vertex_buffer;
	gl::IndexBufferHandle index_buffer;
};

struct RenderMaterial
{
	//TODO: Materials!
	//Eventually, support multi-pass rendering through data driven materials.
	//For now, just a default PBR-like material.
	PropertyBlock block;

	gl::BufferHandle uniform_buffer;
	gl::ProgramHandle program;
};

struct RenderMesh
{
	RenderResource geometry;
	RenderResource material;

	//Basic mesh stuff - model transform, etc.
	struct MeshUniforms
	{
		Mat4 M;
		Mat4 V;
		Mat4 MV;
		Mat4 MVP;
	} mesh_uniforms;
	gl::BufferHandle uniform_buffer;

	bool visible{ true };
};

template<typename T>
class ResourceContainer
{
public:
	struct Pair
	{
		T resource;
		uint32_t id;
	};

private:


	HashIndex hash_index_;
	std::vector<Pair> data_;
	std::atomic<uint32_t> next_id_{ 0 };
	constexpr static uint64_t INDEX_MASK = 0x0000'0000'FFFF'FFFFu;
public:


	ResourceContainer() = default;

	/*
		Thread-safe reserving of indices.
	*/
	uint32_t ReserveIndex()
	{
		return next_id_++;
	}

	/*
		Only will be called by the render thread.
	*/
	T* Add(RenderResource id)
	{
		uint32_t key = id & INDEX_MASK;
		uint32_t index = (uint32_t)data_.size();
		data_.emplace_back(Pair{ T{}, key });

		hash_index_.Add(key, index);
		return &data_[index].resource;
	}

	void Remove(RenderResource id)
	{
		uint32_t key = id & INDEX_MASK;

		uint32_t num_components = data_.size();
		for (uint32_t i = hash_index_.First(key);
			 i != HashIndex::INVALID_INDEX && i < num_components;
			 i = hash_index_.Next(i)) {
			if (data_[i].id == key) {
				//Swap with the last entry before removing.
				data_[i] = std::move(data_.back());
				auto other_index = data_.size() - 1;
				auto other_id = data_[i].id;
				hash_index_.Remove(other_id, other_index);
				hash_index_.Remove(key, i);
				hash_index_.Add(other_id, i);
				data_.pop_back();
				return;
			}
		}


	}

	/*Do the hash lookup*/
	T& operator[](RenderResource id)
	{
		uint32_t key = id & INDEX_MASK;
		uint32_t num_components = data_.size();
		for (uint32_t i = hash_index_.First(key);
			 i != HashIndex::INVALID_INDEX && i < num_components;
			 i = hash_index_.Next(i)) {
			if (data_[i].id == key) {
				return data_[i].resource;
			}
		}

		Ensures(false && "This should never happen!");
		static T BAD_HASH = T{};
		return BAD_HASH;
	}


	friend Pair* begin(ResourceContainer& c)
	{
		return c.data_.data();
	}

	friend const Pair* begin(const ResourceContainer& c)
	{
		return c.data_.data();
	}

	friend Pair* end(ResourceContainer& c)
	{
		return c.data_.data() + c.data_.size();
	}

	friend const Pair* end(const ResourceContainer& c)
	{
		return c.data_.data() + c.data_.size();
	}
};

render::RenderResource CreateHandle(uint32_t index, render::ResourceType type)
{
	constexpr uint64_t INDEX_MASK = 0x00FF'FFFF'FFFF'FFFFu;
	return (index & INDEX_MASK) | (static_cast<uint64_t>(type) << 56);
}

std::atomic_flag render_fence{ ATOMIC_FLAG_INIT };
std::atomic_flag game_fence{ ATOMIC_FLAG_INIT };
CommandStream render_commands;
CommandStream postrender_commands;

Mat4 view_matrix;
Mat4 projection_matrix;

ResourceContainer<RenderGeometry> geometries;
ResourceContainer<RenderMesh> meshes;
ResourceContainer<RenderMaterial> materials;

void AddForwardPass(FrameGraph& graph)
{
	//Simple example which just draws meshes to the 
	struct PassData
	{

	};
	graph.AddCallbackPass<PassData>(
		"ForwardPass",
		[&](PassData& data) {
		//Setup any buffers we need here.
	},
		[=](const PassData& data) {

		for (auto& pair : meshes) {
			auto& mesh = pair.resource;
			auto& geom = geometries[pair.resource.geometry];
			auto& material = materials[pair.resource.material];
			//Draw this geometry.

			//Update the ModelBlock variables.
			if (mesh.visible) {
				mesh.mesh_uniforms.V = view_matrix;
				mesh.mesh_uniforms.MV = view_matrix*mesh.mesh_uniforms.M;
				mesh.mesh_uniforms.MVP = projection_matrix*mesh.mesh_uniforms.MV;

				auto ref = gl::MakeRef(&mesh.mesh_uniforms, sizeof(RenderMesh::MeshUniforms));
				gl::UpdateBufferObject(mesh.uniform_buffer, ref);

				gl::SetVertexBuffer(geom.vertex_buffer);
				gl::SetIndexBuffer(geom.index_buffer);
				//Set any uniforms.

				gl::SetBufferObject(mesh.uniform_buffer, gl::BufferTarget::UNIFORM, 0);
				gl::SetBufferObject(material.uniform_buffer, gl::BufferTarget::UNIFORM, 1);
				gl::Submit(0, material.program);
			}
		}
	});
}


void RenderLoop(GLFWwindow* window)
{
	gl::InitializeBackend(window);

	//Set up framegraph in here.
	FrameGraph frame_graph;
	AddForwardPass(frame_graph);


	while (true) {
		while (render_fence.test_and_set(std::memory_order_acquire)) {
			std::this_thread::yield();
		} //Spin while this flag hasn't been set by the other thread.
		render_commands.SwapBuffers(); //Need to signal to the other threads that they can now make render calls.
		postrender_commands.SwapBuffers();
		game_fence.clear();

		//Update our state by pumping the command list. This syncs state between the game + render threads.
		render_commands.ExecuteAll();


		//Update all materials that have been changed.
		for (auto& pair : materials) {
			auto& mat = pair.resource;
			if (mat.block.dirty) {
				auto ref = gl::MakeRef(mat.block.buffer.get(), mat.block.buffer_size);
				gl::UpdateBufferObject(mat.uniform_buffer, ref);
			}
		}

		//Draw all the meshes.
		//TODO: Culling.

		frame_graph.Execute();


		gl::Render();
		glfwSwapBuffers(window);


		postrender_commands.ExecuteAll();
	}
}

} //end anonymous namespace

void Initialize(GLFWwindow* window)
{
	//Spawn thread and that's about it.
	glfwMakeContextCurrent(nullptr);
	std::thread render_thread(RenderLoop, window);
	render_thread.detach();

}

void ResizeWindow(int w, int h)
{
	struct CmdType : Cmd
	{
		int w, h;
	};
	auto cmd = render_commands.Add<CmdType>();
	cmd->w = w;
	cmd->h = h;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		glViewport(0, 0, data->w, data->h);
	};
}

RenderResource CreateGeometry(const MemoryBlock * vertex_data, const VertexLayout & layout, const MemoryBlock * index_data, IndexType type)
{

	struct CmdType : Cmd
	{
		const MemoryBlock* vertex_data;
		VertexLayout layout;
		const MemoryBlock* index_data;
		IndexType type;

		RenderResource geom;
	};

	auto cmd = render_commands.Add<CmdType>();
	cmd->vertex_data = vertex_data;
	cmd->layout = layout;
	cmd->index_data = index_data;
	cmd->type = type;

	cmd->geom = CreateHandle(geometries.ReserveIndex(), ResourceType::GEOMETRY);
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		auto vb = gl::CreateVertexBuffer(data->vertex_data, data->layout);
		auto ib = gl::CreateIndexBuffer(data->index_data, data->type);

		auto geom = geometries.Add(data->geom);

		geom->index_buffer = ib;
		geom->vertex_buffer = vb;
	};

	return cmd->geom;
}

void UpdateGeometry(const RenderResource geometry_handle, const MemoryBlock * vertex_data, const MemoryBlock * index_data)
{
	Expects(GetResourceType(geometry_handle) == ResourceType::GEOMETRY);

	struct CmdType : Cmd
	{
		RenderResource geometry;
		const MemoryBlock* vertex_data;
		const MemoryBlock* index_data;
	};

	auto cmd = render_commands.Add<CmdType>();
	cmd->geometry = geometry_handle;
	cmd->vertex_data = vertex_data;
	cmd->index_data = index_data;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		auto& geom = geometries[data->geometry];
		gl::UpdateDynamicVertexBuffer(geom.vertex_buffer, data->vertex_data);
		if (data->index_data) {
			gl::UpdateDynamicIndexBuffer(geom.index_buffer, data->index_data);
		}
	};
}


void DeleteGeometry(RenderResource geometry)
{
	Expects(GetResourceType(geometry) == ResourceType::GEOMETRY);
	struct CmdType : Cmd
	{
		RenderResource geom;
	};

	auto cmd = postrender_commands.Add<CmdType>();
	cmd->geom = geometry;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		gl::Destroy(geometries[data->geom].index_buffer);
		gl::Destroy(geometries[data->geom].vertex_buffer);
		geometries.Remove(data->geom);
	};
}

RenderResource CreateMesh(const RenderResource geometry, const RenderResource material)
{
	Expects(GetResourceType(geometry) == ResourceType::GEOMETRY);
	Expects(GetResourceType(material) == ResourceType::MATERIAL);

	struct CmdType :Cmd
	{
		RenderResource geometry;
		RenderResource material;
		RenderResource mesh;
	};

	auto cmd = render_commands.Add<CmdType>();
	cmd->geometry = geometry;
	cmd->material = material;
	cmd->mesh = CreateHandle(meshes.ReserveIndex(), ResourceType::MESH);
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		auto mesh = meshes.Add(data->mesh);
		mesh->geometry = data->geometry;
		mesh->material = data->material;
		mesh->uniform_buffer = gl::CreateBufferObject();
	};

	return cmd->mesh;
}

void SetMeshVisibility(const RenderResource mesh, bool visible)
{
	Expects(GetResourceType(mesh) == ResourceType::MESH);
	struct CmdType : Cmd
	{
		RenderResource mesh;
		bool visible;
	};

	auto cmd = render_commands.Add<CmdType>();
	cmd->mesh = mesh;
	cmd->visible = visible;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		meshes[data->mesh].visible = data->visible;
	};
}

void DeleteMesh(const RenderResource mesh)
{
	Expects(GetResourceType(mesh) == ResourceType::MESH);
	struct CmdType : Cmd
	{
		RenderResource mesh;
	};

	auto cmd = postrender_commands.Add<CmdType>();
	cmd->mesh = mesh;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		auto& mesh = meshes[data->mesh];
		gl::Destroy(mesh.uniform_buffer);
		meshes.Remove(data->mesh);
	};
}

void SetModelTransform(const RenderResource mesh, const Mat4& matrix)
{
	Expects(GetResourceType(mesh) == ResourceType::MESH);

	struct CmdType : Cmd
	{
		RenderResource mesh;
		Mat4 mat;
	};

	auto cmd = render_commands.Add<CmdType>();
	cmd->mat = matrix;
	cmd->mesh = mesh;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		auto& mesh = meshes[data->mesh];
		mesh.mesh_uniforms.M = data->mat;
	};
}

RenderResource CreateMaterial(const MemoryBlock* vertex_shader, const MemoryBlock* frag_shader)
{
	struct CmdType : Cmd
	{
		const MemoryBlock* vert_shader;
		const MemoryBlock* frag_shader;
		RenderResource mat;
	};

	auto cmd = render_commands.Add<CmdType>();
	cmd->vert_shader = vertex_shader;
	cmd->frag_shader = frag_shader;
	cmd->mat = CreateHandle(materials.ReserveIndex(), ResourceType::MATERIAL);
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		auto material = materials.Add(data->mat);
		material->uniform_buffer = gl::CreateBufferObject();
		material->program = gl::CreateProgram(data->vert_shader, data->frag_shader);
		gl::GetUniformBlockInfo(material->program, "MaterialBlock", &material->block);
	};

	return cmd->mat;
}

void SetMaterialParameter(const RenderResource material, const char* name, const void* value, size_t size)
{
	struct CmdType :Cmd
	{
		RenderResource mat;
		const char* name;
		const MemoryBlock* block;
	};

	auto cmd = render_commands.Add<CmdType>();
	cmd->block = gl::AllocAndCopy(value, size);
	cmd->mat = material;
	cmd->name = name; //TODO: THIS IS WRONG!!!!
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		auto& material = materials[data->mat];
		material.block.SetProperty(data->name, data->block->ptr, data->block->length);
	};
}

void DeleteMaterial(RenderResource material)
{
	Expects(GetResourceType(material) == ResourceType::MATERIAL);

	struct CmdType : Cmd
	{
		RenderResource mat;
	};

	auto cmd = postrender_commands.Add<CmdType>();
	cmd->mat = material;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		gl::Destroy(materials[data->mat].program);
		gl::Destroy(materials[data->mat].uniform_buffer);
		materials.Remove(data->mat);
	};
}

void SetViewTransform(const Mat4& matrix)
{
	struct CmdType : Cmd
	{
		Mat4 mat;
	};
	auto cmd = render_commands.Add<CmdType>();
	cmd->mat = matrix;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		view_matrix = data->mat;
	};

}

void SetProjectionTransform(const Mat4& matrix)
{
	struct CmdType : Cmd
	{
		Mat4 mat;
	};
	auto cmd = render_commands.Add<CmdType>();
	cmd->mat = matrix;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		projection_matrix = data->mat;
	};
}

void EndFrame()
{
	render_fence.clear();
	while (game_fence.test_and_set(std::memory_order_acquire)) {
		std::this_thread::yield();
	}
}

//
//IMGUI FUNCTIONS - Will be phased out once the other stuff works well enough to support it.
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

void InitImguiRendering(const MemoryBlock* font_data, int width, int height)
{
	struct CmdType :Cmd
	{
		const MemoryBlock* font_data;
		int width, height;

	};

	auto cmd = render_commands.Add<CmdType>();
	cmd->font_data = font_data;
	cmd->width = width;
	cmd->height = height;
	cmd->dispatch = [](Cmd* cmd) {
		const char vertex_shader[] =
			"#version 330\n"
			"uniform mat4 ProjMtx;\n"
			"layout(location = 0) in vec2 Position;\n"
			"layout(location = 8) in vec2 UV;\n"
			"layout(location = 4) in vec4 Color;\n"
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

		render::VertexLayout vert_layout;
		vert_layout
			.Add(VertexLayout::AttributeBinding::POSITION, 2, VertexLayout::AttributeType::FLOAT32)
			.Add(VertexLayout::AttributeBinding::TEXCOORD0, 2, VertexLayout::AttributeType::FLOAT32)
			.Add(VertexLayout::AttributeBinding::COLOR0, 4, VertexLayout::AttributeType::UINT8, true);
		vert_layout.interleaved = true;
		imgui.vertex_buffer = gl::CreateDynamicVertexBuffer(vert_layout);
		imgui.index_buffer = gl::CreateDynamicIndexBuffer(IndexType::UShort);
	};


}

void UpdateImguiData(const MemoryBlock* vertex_data, const MemoryBlock* index_data, const Vec2& size)
{
	struct CmdType : Cmd
	{
		const MemoryBlock* vert_data;
		const MemoryBlock* index_data;
		Vec2 display_size;
	};
	auto cmd = render_commands.Add<CmdType>();
	cmd->vert_data = vertex_data;
	cmd->index_data = index_data;
	cmd->display_size = size;
	cmd->dispatch = [](Cmd* cmd) {
		auto data = reinterpret_cast<CmdType*>(cmd);
		gl::UpdateDynamicVertexBuffer(imgui.vertex_buffer, data->vert_data);
		gl::UpdateDynamicIndexBuffer(imgui.index_buffer, data->index_data);
		imgui.display_size = data->display_size;
	};


}

void DrawImguiCmd(uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, uint32_t scissor_x, uint32_t scissor_y, uint32_t scissor_w, uint32_t scissor_h)
{
	struct CmdType : Cmd
	{
		uint32_t vertex_offset, index_offset, index_count;
		uint32_t x, y, w, h;
	};
	auto cmd = render_commands.Add<CmdType>();
	cmd->vertex_offset = vertex_offset;
	cmd->index_offset = index_offset;
	cmd->index_count = index_count;
	cmd->x = scissor_x;
	cmd->y = scissor_y;
	cmd->w = scissor_w;
	cmd->h = scissor_h;

	cmd->dispatch = [](Cmd* cmd) {
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
}




}//end namespace render
}//end namespace rkg