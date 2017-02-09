#include "RenderInterface.h"

using namespace rkg;


namespace
{

enum class RenderObjectType
{
	MESH,
	TEXTURE,
	PARTICLE,
	CAMERA,
	LIGHT
};

//Everything that exists on the render thread will inherit from this object
struct RenderThreadObject
{
	RenderObjectType type;
};


enum class MaterialType
{
	IMGUI,
	STANDARD
};

struct ImguiMaterial
{
	//Don't think ImGui needs any material parameters really...

};

struct StandardMaterial
{
	//Standard, PBR material parameters
	Vec3 albedo;
	Vec3 roughness;

	bool casts_shadows;
};


struct RenderMesh : public RenderThreadObject
{
	render::VertexBufferHandle vertex_buffer;
	render::IndexBufferHandle index_buffer;
	Mat4 transform;
	MaterialType material_type;

	union
	{
		ImguiMaterial imgui_material;
		StandardMaterial standard_material;
	};
};

struct RenderLight : public RenderThreadObject
{
	Mat4 transform;
	Vec3 color;
};

struct RenderCamera : public RenderThreadObject
{

};




}

void render::Create(render::MeshObject* mesh_object)
{



}


void render::RenderLoop()
{
	//Eventually, will be a forward+ renderer, but for now, just a simple forward one.


	//Update any global uniform buffers here - lights, etc.



	for (auto mesh : mesh_list) {
		if (mesh.material_type == MaterialType::STANDARD) {

			if (mesh.standard_material.casts_shadows) {
				//Draw into shadow buffer
			}

			//Bind textures

			//Draw


		} else if (mesh.material_type == MaterialType::IMGUI) {
			//Draw the mesh with the appropriate shader.
		}
	}
}


