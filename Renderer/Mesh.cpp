
#define _USE_MATH_DEFINES
#include <cmath>

#include "Mesh.h"

#include <Utilities/Allocators.h>
#include <External/rply/rply.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <External/tiny_obj_loader.h>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>

namespace rkg
{
namespace
{
Mallocator mesh_allocator;
}

Mesh::Mesh(Mesh&& src) noexcept
{
	vertex_block_ = src.vertex_block_;
	src.vertex_block_ = { nullptr, 0 };
	index_block_ = src.index_block_;
	src.index_block_ = { nullptr, 0 };
	num_verts_ = src.num_verts_;
	num_indices_ = src.num_indices_;
	active_attributes_ = src.active_attributes_;
	attribute_offset_ = std::move(src.attribute_offset_);
	vertex_size_ = src.vertex_size_;
}

Mesh& Mesh::operator=(Mesh&& src) noexcept
{
	vertex_block_ = src.vertex_block_;
	src.vertex_block_ = { nullptr, 0 };
	index_block_ = src.index_block_;
	src.index_block_ = { nullptr, 0 };
	num_verts_ = src.num_verts_;
	num_indices_ = src.num_indices_;
	active_attributes_ = src.active_attributes_;
	attribute_offset_ = std::move(src.attribute_offset_);
	vertex_size_ = src.vertex_size_;

	return *this;
}

Mesh::~Mesh()
{
	if (vertex_block_.ptr) {
		mesh_allocator.Deallocate(vertex_block_);
	}
	if (index_block_.ptr) {
		mesh_allocator.Deallocate(index_block_);
	}

}

render::VertexLayout Mesh::GetVertexLayout()
{
	//TODO: Support other vertex layouts. Should be configurable.
	render::VertexLayout layout;

	if (active_attributes_ & MeshAttributes::POSITION) {
		layout.Add(render::VertexLayout::AttributeBinding::POSITION, 3, render::VertexLayout::AttributeType::FLOAT32);
	}
	if (active_attributes_ & MeshAttributes::NORMAL) {
		layout.Add(render::VertexLayout::AttributeBinding::NORMAL, 3, render::VertexLayout::AttributeType::FLOAT32);
	}
	if (active_attributes_ & MeshAttributes::COLOR0) {
		layout.Add(render::VertexLayout::AttributeBinding::COLOR0, 4, render::VertexLayout::AttributeType::FLOAT32);
	}


	return layout;
}

void Mesh::ComputeNormals()
{
	auto indices = Indices();

	//TODO: Replace with memset.
	auto normals = Normals();
	for (unsigned int v = 0; v < num_verts_; v++) {
		normals[v] = Vec3(0, 0, 0);
	}

	auto positions = Positions();
	//TODO: Resize vertex block if needed.
	for (int i = 0; i < num_indices_; i += 3) {
		Vec3& A = positions[indices[i]];
		Vec3& B = positions[indices[i + 1]];
		Vec3& C = positions[indices[i + 2]];

		const Vec3 AB = B - A;
		const Vec3 AC = C - A;
		const Vec3 BC = AC - AB;
		const Vec3 Normal = Normalize(Cross(AB, AC));

		const float thetaA = std::acosf(Dot(AB, AC) / (AB.Length()*AC.Length()));
		const float thetaB = std::acosf(-Dot(AB, BC) / (AB.Length()*BC.Length()));
		const float thetaC = M_PI - thetaA - thetaB;

		Clamp(thetaA, 0, M_PI);
		Clamp(thetaB, 0, M_PI);
		Clamp(thetaC, 0, M_PI);

		Vec3& normA = normals[indices[i]];
		Vec3& normB = normals[indices[i + 1]];
		Vec3& normC = normals[indices[i + 2]];
		normA += thetaA*Normal;
		normB += thetaB*Normal;
		normC += thetaC*Normal;
	}

	for (unsigned int v = 0; v < num_verts_; v++) {
		
		normals[v] = Normalize(normals[v]);
		if (isnan(normals[v].x)) {
			//printf("Bad normal computation!\n");
			normals[v] = Vec3(1, 0, 0);
		}
	}
}

void Mesh::ComputeBounds(Vec3* min_, Vec3* max_)
{
	constexpr auto LOWEST = std::numeric_limits<float>::lowest();
	constexpr auto LARGEST = std::numeric_limits<float>::max();
	*max_ = Vec3(LOWEST, LOWEST, LOWEST);
	*min_ = Vec3(LARGEST, LARGEST, LARGEST);
	for (int i = 0; i < num_verts_; i++) {
		*max_ = Max(*max_, Positions()[i]);
		*min_ = Min(*min_, Positions()[i]);
	}
}

void Mesh::SetMeshAttributes(uint16_t attributes)
{
	uint16_t old_layout = active_attributes_;
	active_attributes_ = attributes;
	int old_vert_size = vertex_size_;
	decltype(attribute_offset_) existing_offsets = attribute_offset_;
	ComputeAttributeOffsets();


	if (vertex_block_.ptr) {
		//Need to reallocate and shuffle stuff around.
		//I'm doing this kind of naively. But you shouldn't change mesh attributes very often!!
		auto new_vertex_block_ = mesh_allocator.Allocate(vertex_size_ * sizeof(float) * num_verts_);

		auto dst = static_cast<float*>(new_vertex_block_.ptr);
		auto src = static_cast<float*>(vertex_block_.ptr);
		uint16_t mask = 1;
		for (int i = 0; i < MeshAttributes::COUNT - 1; i++) {
			if (active_attributes_ & mask) {
				if (old_layout & mask) {
					memcpy(dst, src, sizeof(float) * (existing_offsets[i + 1] - existing_offsets[i]));
					src += existing_offsets[i + 1] - existing_offsets[i];
				}

				dst += attribute_offset_[i + 1] - attribute_offset_[i];
			}
			mask <<= 1;
		}

		//Last entry - slightly different bounds.
		if (active_attributes_ & mask) {
			if (old_layout & mask) {
				memcpy(dst, src, sizeof(float) * ((old_vert_size * num_verts_) - existing_offsets[MeshAttributes::COUNT - 1]));
				src += ((old_vert_size * num_verts_) - existing_offsets[MeshAttributes::COUNT - 1]);
			}

			dst += (vertex_size_*num_verts_) - attribute_offset_[MeshAttributes::COUNT - 1];
		}
		mesh_allocator.Deallocate(vertex_block_);
		vertex_block_ = new_vertex_block_;
	}


}

void Mesh::ComputeAttributeOffsets()
{
	vertex_size_ = (active_attributes_ &  MeshAttributes::POSITION) ? 3 : 0;
	attribute_offset_[0] = 0;
	uint16_t mask = 2;
	int i = 1;
	for (; mask <= static_cast<int>(MeshAttributes::BITANGENT); i++, mask <<= 1) {
		if (active_attributes_ & mask) {
			attribute_offset_[i] = vertex_size_ * num_verts_;
			vertex_size_ += 3;
		} else {
			attribute_offset_[i] = attribute_offset_[i - 1];
		}
	}

	for (; mask <= static_cast<int>(MeshAttributes::WEIGHT); i++, mask <<= 1) {
		if (active_attributes_ & mask) {
			attribute_offset_[i] = vertex_size_ * num_verts_;
			vertex_size_ += 4;
		} else {
			attribute_offset_[i] = attribute_offset_[i - 1];
		}
	}

	for (; mask <= static_cast<int>(MeshAttributes::TEXCOORD2); i++, mask <<= 1) {
		if (active_attributes_ & mask) {
			attribute_offset_[i] = vertex_size_ * num_verts_;
			vertex_size_ += 2;
		} else {
			attribute_offset_[i] = attribute_offset_[i - 1];
		}
		mask <<= 1;
	}
}

void Mesh::AllocateVertexMemory(uint32_t num_vertices)
{
	//must be called after ComputeAttributeOffsets is called.
	if (vertex_block_.ptr) {
		mesh_allocator.Deallocate(vertex_block_);
	}

	num_verts_ = num_vertices;
	vertex_block_ = mesh_allocator.Allocate(num_verts_ * sizeof(float)*vertex_size_);
}


namespace
{
int VertexPLYCallback(p_ply_argument arg)
{
	long index;
	Mesh* mesh;
	ply_get_argument_user_data(arg, (void**)&mesh, &index);
	double val = ply_get_argument_value(arg);
	long vert_num;
	ply_get_argument_element(arg, nullptr, &vert_num);
	float* vertex = (float*)&mesh->Positions()[vert_num];

	vertex[index] = (float)val;
	return 1;
}


struct FacePLYData
{
	int* block;
	uint32_t index;
};

int FacePLYCallback(p_ply_argument arg)
{
	FacePLYData* mesh;
	ply_get_argument_user_data(arg, (void**)&mesh, nullptr);
	long value_index;
	ply_get_argument_property(arg, nullptr, nullptr, &value_index);
	if (value_index >= 0) {
		mesh->block[mesh->index++] = ply_get_argument_value(arg);
	}
	return 1;
}


}

std::unique_ptr<Mesh> LoadPLY(const char* filename)
{
	auto mesh = std::make_unique<Mesh>();
	auto ply = ply_open(filename, nullptr, 0, nullptr);
	if (!ply) {
		return nullptr;
	}
	if (!ply_read_header(ply)) {
		return nullptr;
	}


	auto nverts = ply_set_read_cb(ply, "vertex", "x", VertexPLYCallback, &mesh, 0);
	ply_set_read_cb(ply, "vertex", "y", VertexPLYCallback, &mesh, 1);
	ply_set_read_cb(ply, "vertex", "z", VertexPLYCallback, &mesh, 2);

	FacePLYData data;
	auto nfaces = ply_set_read_cb(ply, "face", "vertex_indices", FacePLYCallback, &mesh, 0);
	mesh->index_block_ = mesh_allocator.Allocate(nfaces * 3 * sizeof(float));
	data.block = (int*)mesh->index_block_.ptr;
	data.index = 0;
	mesh->SetMeshAttributes(MeshAttributes::POSITION | MeshAttributes::NORMAL);

	mesh->AllocateVertexMemory(nverts);

	mesh->num_indices_ = 3 * nfaces;

	if (!ply_read(ply)) {
		return mesh;
	}
	ply_close(ply);

	mesh->ComputeNormals();
	return mesh;
}

std::unique_ptr<Mesh> LoadOBJ(const char* filename)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;
	tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename, nullptr, true);
	//Want to separate out these meshes...

	auto mesh = std::make_unique<Mesh>();


	mesh->num_verts_ = attrib.vertices.size() / 3;
	//Copy the vertex data. normal data has to be copied seperately, since it may be in a different order.

	mesh->SetMeshAttributes(MeshAttributes::POSITION | MeshAttributes::NORMAL);
	mesh->AllocateVertexMemory(attrib.vertices.size() / 3);
	memcpy(mesh->vertex_block_.ptr, attrib.vertices.data(), attrib.vertices.size() * sizeof(float));

	uint32_t num_indices = 0;
	for (auto& shape : shapes) {
		num_indices += shape.mesh.indices.size();
	}
	mesh->num_indices_ = num_indices;

	mesh->index_block_ = mesh_allocator.Allocate(num_indices * sizeof(unsigned int));
	uint32_t index_offset = 0;


	auto normals = mesh->Normals();
	auto indices = mesh->Indices();
	bool has_normals = false;
	for (auto& shape : shapes) {
		auto& mesh_indices = shape.mesh.indices;
		for (auto& index : mesh_indices) {
			if (index.normal_index != -1) {
				has_normals = true;
				normals[index.vertex_index] = Vec3(attrib.normals[3 * index.normal_index + 0],
												   attrib.normals[3 * index.normal_index + 1],
												   attrib.normals[3 * index.normal_index + 2]);
			}
			indices[index_offset] = index.vertex_index;
			index_offset++;
		}
	}

	if (!has_normals) {
		mesh->ComputeNormals();
	}

	return mesh;
}

Mesh MakeSquare(int num_div_x, int num_div_y)
{
	Mesh mesh;
	mesh.SetMeshAttributes(MeshAttributes::POSITION | MeshAttributes::NORMAL);
	mesh.AllocateVertexMemory(num_div_x*num_div_y);
	mesh.ComputeAttributeOffsets();

	mesh.vertex_block_ = mesh_allocator.Allocate(mesh.num_verts_ * 6 * sizeof(float));
	for (int x = 0; x < num_div_x; x++) {
		for (int y = 0; y < num_div_y; y++) {
			mesh.Positions()[x*num_div_y + y] = Vec3(x / (float)num_div_x, y / (float)num_div_y, 0.0);
		}
	}

	mesh.num_indices_ = 3 * 2 * (num_div_x - 1)*(num_div_y - 1);
	mesh.index_block_ = mesh_allocator.Allocate(mesh.num_indices_ * sizeof(unsigned int));
	int index = 0;
	for (int x = 0; x < num_div_x - 1; x++) {
		for (int y = 0; y < num_div_y - 1; y++) {
			int i = x + y * num_div_x;
			mesh.Indices()[index++] = i;
			mesh.Indices()[index++] = i + num_div_x;
			mesh.Indices()[index++] = i + 1;

			mesh.Indices()[index++] = i + 1;
			mesh.Indices()[index++] = i + num_div_x;
			mesh.Indices()[index++] = i + num_div_x + 1;
		}
	}

	mesh.ComputeNormals();

	return mesh;
}

Mesh MakeIcosphere(int num_divisions)
{
	//TODO: Right now, this doesn't support division, only plain icosohedrons.

	Mesh mesh;
	mesh.SetMeshAttributes(MeshAttributes::POSITION | MeshAttributes::NORMAL);

	int num_faces = 20;
	int num_verts = 12;
	mesh.AllocateVertexMemory(num_verts);
	mesh.ComputeAttributeOffsets();


	//Seems like there should be a closed form equation for the number of points/faces.
	//There is one for the number of faces, but the verts is less obvious.
	/*for (int i = 0; i < num_divisions; i++) {
		num_verts = num_verts + 3 * num_faces / 2;
		num_faces = 4 * num_faces;
	}*/

	//We hard-code in the initial icosohedron positions.
	float t = 0.5f * (1.f + std::sqrt(5.f));
	auto position = mesh.Positions();
	position[0] = Normalize(rkg::Vec3(-1, t, 0));
	position[1] = Normalize(rkg::Vec3(1, t, 0));
	position[2] = Normalize(rkg::Vec3(-1, -t, 0));
	position[3] = Normalize(rkg::Vec3(1, -t, 0));

	position[4] = Normalize(rkg::Vec3(0, -1, t));
	position[5] = Normalize(rkg::Vec3(0, 1, t));
	position[6] = Normalize(rkg::Vec3(0, -1, -t));
	position[7] = Normalize(rkg::Vec3(0, 1, -t));

	position[8] = Normalize(rkg::Vec3(t, 0, -1));
	position[9] = Normalize(rkg::Vec3(t, 0, 1));
	position[10] = Normalize(rkg::Vec3(-t, 0, -1));
	position[11] = Normalize(rkg::Vec3(-t, 0, 1));

	unsigned int faces[] = {
		0,11,5
		,0,5,1,
		0,1,7,
		0,7,10,
		0,10,11,
		1,5,9,
		5,11,4,
		11,10,2,
		10,7,6,
		7,1,8,
		3,9,4,
		3,4,2,
		3,2,6,
		3,6,8,
		3,8,9,
		4,9,5,
		2,4,11,
		6,2,10,
		8,6,7,
		9,8,1,
	};

	mesh.index_block_ = mesh_allocator.Allocate(num_faces * 3 * sizeof(unsigned int));
	memcpy(mesh.index_block_.ptr, faces, sizeof(faces));

	mesh.num_indices_ = num_verts * 3;
	mesh.ComputeNormals();
	return mesh;
}

Mesh SplitFaces(const Mesh & mesh)
{
	Mesh result;
	result.num_indices_ = result.num_verts_ = mesh.num_indices_;
	result.active_attributes_ = mesh.active_attributes_;
	result.ComputeAttributeOffsets();
	result.AllocateVertexMemory(result.num_verts_);

	result.index_block_ = mesh_allocator.Allocate(result.num_indices_ * sizeof(unsigned int));

	auto dst_indices = result.Indices();
	auto dst_positions = result.Positions();
	auto src_indices = mesh.Indices();
	auto src_positions = mesh.Positions();
	for (int i = 0; i < result.num_indices_; i++, src_indices++, dst_indices++, dst_positions++) {
		*dst_indices = i;
		*dst_positions = src_positions[*src_indices];
	}

	if (result.active_attributes_ & static_cast<uint16_t>(MeshAttributes::NORMAL)) {
		auto dst_normals = result.Normals();
		auto src_normals = mesh.Normals();
		src_indices = mesh.Indices();
		for (int i = 0; i < result.num_indices_; i++, dst_normals++, src_indices++) {
			*dst_normals = src_normals[*src_indices];
		}
	}

	if (result.active_attributes_ & static_cast<uint16_t>(MeshAttributes::COLOR0)) {
		auto dst_normals = result.GetVec4Attribute(MeshAttributes::COLOR0);
		auto src_normals = mesh.GetVec4Attribute(MeshAttributes::COLOR0);
		src_indices = mesh.Indices();
		for (int i = 0; i < result.num_indices_; i++, dst_normals++, src_indices++) {
			*dst_normals = src_normals[*src_indices];
		}
	}
	//TODO: Support general vertex layouts.

	return result;
}

Mesh ApplyPerFaceColor(const Mesh& mesh, const std::vector<Vec4>& colors)
{
	Mesh result = SplitFaces(mesh);
	result.SetMeshAttributes(MeshAttributes::POSITION | MeshAttributes::NORMAL | MeshAttributes::COLOR0);
	auto dst_colors = result.GetVec4Attribute(MeshAttributes::COLOR0);
	Expects(result.num_indices_ / 3 == colors.size());
	for (int f = 0; f < result.num_indices_; f += 3) {
		dst_colors[f] = colors[f / 3];
		dst_colors[f + 1] = colors[f / 3];
		dst_colors[f + 2] = colors[f / 3];
	}
	//TODO: Support general vertex layouts.

	result.ComputeNormals();
	return result;
}



}
