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
	bool two_sided{ false };
	bool draw_wireframe{ false };
};

struct RenderLight
{
	enum Type {
		POINT,
		SPHERE,
		DIRECTIONAL,
		LINE,
	};

	Vec3 color;
	Type type;
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

//
//Debug Draw resources
//
std::vector<uint32_t> debug_index_buffers[2];
std::vector<Vec4> debug_data_buffers[2];

std::vector<uint32_t>* debug_front_index_buffer = &debug_index_buffers[0];
std::vector<Vec4>* debug_front_data_buffer = &debug_data_buffers[0]; 
std::vector<uint32_t>* debug_back_index_buffer = &debug_index_buffers[1];
std::vector<Vec4>* debug_back_data_buffer = &debug_data_buffers[1];

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

				gl::SetState(gl::RenderState::RGB_WRITE 
					| gl::RenderState::ALPHA_WRITE 
					| gl::RenderState::DEPTH_WRITE 
					| gl::RenderState::DEPTH_TEST_LESS 
					| gl::RenderState::PRIMITIVE_TRIANGLES
					| (mesh.two_sided ? gl::RenderState::CULL_OFF : gl::RenderState::CULL_CCW)
					| (mesh.draw_wireframe ? gl::RenderState::POLYGON_MODE_LINE : gl::RenderState::POLYGON_MODE_FILL));
				gl::Submit(0, material.program);
			}
		}
	});
}

void ForwardRendererPrototype()
{
	/*
		Just going to outline the code necessary for a DOOM-like forward+ renderer.
		The rough steps are as follows:
		1. Compute shader(s) is dispatched to compute lighting clusters. We have a buffer of lights, as just a flat array.
			This shader runs for each light and computes a cluster list, where each element of this cluster list has an offset and count into an item list, which stores actual light indices.
			This may not be as easily parallelized as I thought - computing the counts can be easily done using atomics, but offsets needs a prefix scan, and the item list needs to be sorted. So probably better done on the CPU.
			May be easier to just do this computation serially. 
		2. Optional: Depth pre-pass. Avoids overdraw, so probably worthwhile. Also outputs a velocity buffer - probably won't bother with that for a while.
		3. Forward pass: For each mesh, draw it to the frame buffer. Do lighting lookups here, etc.
		For now, this would even be enough for me. I can probably do everything here in a single pass, but maybe not IBL. If I need to, I can do that afterwards.

		3. Debug pass: Anything that needs to get drawn without lighting, draw to a separate buffer, but doing depth testing against the forward pass.
						IMGUI stuff can be drawn here as well, to a fullscreen framebuffer.
		4. Optional: SSAO pass? / Tonemapping and postprocessing goes here.
		5. Composition: Combing the current color buffer with the debug pass output, then blit to screen.
		
		So all this is fairly easy.

		Questions: How to handle multiple light types? Since it's clustered by froxel, 
		there shouldn't be much in terms of warp divergence for branching on light types, 
		so it should be fine to just switch, handle a few types specifically? Maybe to a trace of Doom in renderdoc, and see what the 
		buffers/shaders look like.
	*/

	
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
		//Swap debug draw data.
		std::swap(debug_front_index_buffer, debug_back_index_buffer);
		std::swap(debug_front_data_buffer,  debug_back_data_buffer);
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
		//TODO: Culling, other optimizations.

		frame_graph.Execute();

		//TODO:Make this less hacky
		//Draw debug stuff.


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
	auto cmd = render_commands.Add([=]() {
		glViewport(0, 0, w, h);
	});
}

RenderResource CreateGeometry(const MemoryBlock * vertex_data, const VertexLayout & layout, const MemoryBlock * index_data, IndexType type)
{
	//Need to reserve handle now so I have something to return.
	auto geom = CreateHandle(geometries.ReserveIndex(), ResourceType::GEOMETRY);
	
	auto cmd = render_commands.Add([=]() {
		auto vb = gl::CreateVertexBuffer(vertex_data, layout);
		auto ib = gl::CreateIndexBuffer(index_data, type);

		auto geometry = geometries.Add(geom);

		geometry->index_buffer = ib;
		geometry->vertex_buffer = vb;
	});

	return geom;
}

void UpdateGeometry(const RenderResource geometry_handle, const MemoryBlock * vertex_data, const VertexLayout& layout, const MemoryBlock * index_data)
{
	Expects(GetResourceType(geometry_handle) == ResourceType::GEOMETRY);


	auto cmd = render_commands.Add([=]() {
		auto& geom = geometries[geometry_handle];
		gl::UpdateDynamicVertexBuffer(geom.vertex_buffer, vertex_data, layout);

		if (index_data) {
			gl::UpdateDynamicIndexBuffer(geom.index_buffer, index_data);
		}
	});
}


void DeleteGeometry(RenderResource geometry)
{
	Expects(GetResourceType(geometry) == ResourceType::GEOMETRY);
	auto cmd = postrender_commands.Add([=]() {
		gl::Destroy(geometries[geometry].index_buffer);
		gl::Destroy(geometries[geometry].vertex_buffer);
		geometries.Remove(geometry);
	});
}

RenderResource CreateMesh(const RenderResource geometry, const RenderResource material)
{
	Expects(GetResourceType(geometry) == ResourceType::GEOMETRY);
	Expects(GetResourceType(material) == ResourceType::MATERIAL);

	auto mesh_handle = CreateHandle(meshes.ReserveIndex(), ResourceType::MESH);
	render_commands.Add([=]() {
		auto mesh = meshes.Add(mesh_handle);
		mesh->geometry = geometry;
		mesh->material = material;
		mesh->uniform_buffer = gl::CreateBufferObject();
	});

	return mesh_handle;
}

void SetMeshVisibility(const RenderResource mesh, bool visible)
{
	Expects(GetResourceType(mesh) == ResourceType::MESH);
	render_commands.Add([=]() {
		meshes[mesh].visible = visible;
	});
}

void SetMeshTwoSided(const RenderResource mesh, bool two_sided)
{
	Expects(GetResourceType(mesh) == ResourceType::MESH);
	render_commands.Add([=]() {
		meshes[mesh].two_sided = two_sided;
	});
}

void SetMeshDrawWireframe(const RenderResource mesh, bool wireframe)
{
	Expects(GetResourceType(mesh) == ResourceType::MESH);
	render_commands.Add([=]() {
		meshes[mesh].draw_wireframe = wireframe;
	});
}

void DeleteMesh(const RenderResource mesh_handle)
{
	Expects(GetResourceType(mesh_handle) == ResourceType::MESH);
	postrender_commands.Add([=]() {
		auto& mesh = meshes[mesh_handle];
		gl::Destroy(mesh.uniform_buffer);
		meshes.Remove(mesh_handle);
	});
}

void SetModelTransform(const RenderResource mesh_handle, const Mat4& matrix)
{
	Expects(GetResourceType(mesh_handle) == ResourceType::MESH);
	render_commands.Add([=]() {
		auto& mesh = meshes[mesh_handle];
		mesh.mesh_uniforms.M = matrix;
	});
}

RenderResource CreateMaterial(const MemoryBlock* vertex_shader, const MemoryBlock* frag_shader)
{
	auto mat = CreateHandle(materials.ReserveIndex(), ResourceType::MATERIAL);
	render_commands.Add([=]() {
		auto material = materials.Add(mat);
		material->uniform_buffer = gl::CreateBufferObject();
		material->program = gl::CreateProgram(vertex_shader, frag_shader);
		gl::GetUniformBlockInfo(material->program, "MaterialBlock", &material->block);
	});

	return mat;
}

void SetMaterialParameter(const RenderResource mat, const char* name, const void* value, size_t size)
{
	Expects(GetResourceType(mat) == ResourceType::MATERIAL);
	auto block = gl::AllocAndCopy(value, size);
	render_commands.Add([=]() {
		auto& material = materials[mat];
		material.block.SetProperty(name, block->ptr, block->length);
	});
}

void DeleteMaterial(RenderResource mat)
{
	Expects(GetResourceType(mat) == ResourceType::MATERIAL);

	postrender_commands.Add([=]() {
		gl::Destroy(materials[mat].program);
		gl::Destroy(materials[mat].uniform_buffer);
		materials.Remove(mat);
	});
}

void SetViewTransform(const Mat4& matrix)
{
	render_commands.Add([=]() {
		view_matrix = matrix;
	});
}

void SetProjectionTransform(const Mat4& matrix)
{
	render_commands.Add([=]() {
		projection_matrix = matrix;
	});
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
	render_commands.Add([=]() {
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

		imgui.texture = gl::CreateTexture2D(width, height, gl::TextureFormat::RGBA8, font_data);

		render::VertexLayout vert_layout;
		vert_layout
			.Add(VertexLayout::AttributeBinding::POSITION, 2, VertexLayout::AttributeType::FLOAT32)
			.Add(VertexLayout::AttributeBinding::TEXCOORD0, 2, VertexLayout::AttributeType::FLOAT32)
			.Add(VertexLayout::AttributeBinding::COLOR0, 4, VertexLayout::AttributeType::UINT8, true);
		vert_layout.interleaved = true;
		imgui.vertex_buffer = gl::CreateDynamicVertexBuffer(vert_layout);
		imgui.index_buffer = gl::CreateDynamicIndexBuffer(IndexType::UShort);
	});


}

void UpdateImguiData(const MemoryBlock* vertex_data, const MemoryBlock* index_data, const Vec2& size)
{
	
	render_commands.Add([=]() {
		render::VertexLayout vert_layout;
		vert_layout
			.Add(VertexLayout::AttributeBinding::POSITION, 2, VertexLayout::AttributeType::FLOAT32)
			.Add(VertexLayout::AttributeBinding::TEXCOORD0, 2, VertexLayout::AttributeType::FLOAT32)
			.Add(VertexLayout::AttributeBinding::COLOR0, 4, VertexLayout::AttributeType::UINT8, true);
		vert_layout.interleaved = true;
		gl::UpdateDynamicVertexBuffer(imgui.vertex_buffer, vertex_data, vert_layout);
		gl::UpdateDynamicIndexBuffer(imgui.index_buffer, index_data);
		imgui.display_size = size;
	});
}

void DrawImguiCmd(uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, uint32_t scissor_x, uint32_t scissor_y, uint32_t scissor_w, uint32_t scissor_h)
{
	render_commands.Add([=]() {
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
		gl::SetScissor(scissor_x, scissor_y, scissor_w, scissor_h);
		gl::SetVertexBuffer(imgui.vertex_buffer, vertex_offset);
		gl::SetIndexBuffer(imgui.index_buffer, index_offset, index_count);
		gl::Submit(imgui.render_layer, imgui.program);
	});
}


namespace
{
	enum DebugPrimitive_
	{
		DebugPrimitive_Sphere = 0,
		DebugPrimitive_Disc,
		DebugPrimitive_Cylinder,
		DebugPrimitive_Cone,
	};
}


void DebugDrawSphere(const Vec3 & position, float radius, const Vec4 & color)
{
	struct Sphere
	{
		Vec4 pos;
		Vec4 color;
	} sphere;

	sphere.pos = Vec4(position.x, position.y, position.z, radius);
	sphere.color = color;
	
	//How many verts does a sphere need? 
	uint32_t primitive_offset = debug_back_data_buffer->size();
	debug_back_data_buffer->push_back(sphere.pos);
	debug_back_data_buffer->push_back(sphere.color);
	
	constexpr int NUM_SPHERE_VERTS = 32;
	for (int i = 0; i < NUM_SPHERE_VERTS; i++) {
		uint32_t index = primitive_offset 
			| (i << 20)
			| (DebugPrimitive_Sphere << 29);
		debug_back_index_buffer->push_back(index);
	}
}

void DebugDrawDisc(const Vec3 & center, const Vec3 & normal, float radius, const Vec4 & color)
{
	struct Disc
	{
		Vec4 center;
		Vec4 normal;
		Vec4 color;
	} disc;

	disc.center = Vec4(center.x, center.y, center.z, radius);
	disc.normal = Vec4(normal);
	disc.color = color;

	uint32_t primitive_offset = debug_back_data_buffer->size();
	debug_back_data_buffer->push_back(disc.center);
	debug_back_data_buffer->push_back(disc.normal);
	debug_back_data_buffer->push_back(disc.color);

	constexpr int NUM_CYLINDER_VERTS = 16;
	for (int i = 0; i < NUM_CYLINDER_VERTS; i++) {
		uint32_t index = primitive_offset
			| (i << 20)
			| (DebugPrimitive_Cylinder << 29);
		debug_back_index_buffer->push_back(index);
	}
}

void DebugDrawCylinder(const Vec3 & start, const Vec3 & end, float radius, const Vec4 & color)
{
	struct Cylinder
	{
		Vec4 bottom;
		Vec4 top;
		Vec4 color;
	} cylinder;
	cylinder.bottom = Vec4(start.x, start.y, start.z, radius);
	cylinder.top = Vec4(end.x, end.y, end.z, radius);
	cylinder.color = color;

	uint32_t primitive_offset = debug_back_data_buffer->size();
	debug_back_data_buffer->push_back(cylinder.bottom);
	debug_back_data_buffer->push_back(cylinder.top);
	debug_back_data_buffer->push_back(cylinder.color);

	constexpr int NUM_CYLINDER_VERTS = 32;
	for (int i = 0; i < NUM_CYLINDER_VERTS; i++) {
		uint32_t index = primitive_offset
			| (i<<20)
			| (DebugPrimitive_Cylinder << 29);
		debug_back_index_buffer->push_back(index);
	}
}

void DebugDrawCone(const Vec3 & bottom, const Vec3 & top, float radius, const Vec4 & color)
{
	struct Cone
	{
		Vec4 bottom;
		Vec4 top;
		Vec4 color;
	} cone;

	cone.bottom = bottom;
	cone.bottom.w = radius;
	cone.top = top;
	cone.color = color;

	uint32_t primitive_offset = debug_back_data_buffer->size();
	debug_back_data_buffer->push_back(cone.bottom);
	debug_back_data_buffer->push_back(cone.top);
	debug_back_data_buffer->push_back(cone.color);

	constexpr int NUM_CONE_VERTS = 32;
	for (int i = 0; i < NUM_CONE_VERTS; i++) {
		uint32_t index = primitive_offset
			| (i << 20)
			| (DebugPrimitive_Cone << 29);
		debug_back_index_buffer->push_back(index);
	}
}

void DebugDrawArrow(const Vec3 & tail, const Vec3 & tip, float tail_radius, float head_radius, float head_length, const Vec4 & color)
{
	//So simple!!
	Vec3 tail_end = tail + head_length*(tip-tail);
	DebugDrawCylinder(tail, tail_end, tail_radius, color);
	DebugDrawCone(tail_end, tip, head_radius, color);
}





}//end namespace render
}//end namespace rkg