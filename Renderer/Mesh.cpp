
#define _USE_MATH_DEFINES
#include <cmath>

#include "Mesh.h"

#include <Utilities/Allocators.h>
#include <External/rply/rply.h>


namespace rkg
{

render::VertexLayout Mesh::GetVertexLayout()
{
	//TODO: Support other vertex layouts. Should be configurable.
	return render::VertexLayout()
		.Add(render::VertexLayout::AttributeBinding::POSITION, 3, render::VertexLayout::AttributeType::FLOAT32)
		.Add(render::VertexLayout::AttributeBinding::NORMAL, 3, render::VertexLayout::AttributeType::FLOAT32);
}

void Mesh::ComputeNormals()
{
	auto indices = Indices();

	//TODO: Replace with memset.
	for (unsigned int v = 0; v < num_verts_; v++) {
		Normals()[v] = Vec3(0, 0, 0);
	}

	//TODO: Resize vertex block if needed.
	for (int i = 0; i < num_indices_; i += 3) {
		Vec3& A = Positions()[indices[i]];
		Vec3& B = Positions()[indices[i + 1]];
		Vec3& C = Positions()[indices[i + 2]];

		const Vec3 AB = B - A;
		const Vec3 AC = C - A;
		const Vec3 BC = AC - AB;
		const Vec3 Normal = Normalize(Cross(AB, AC));

		const float thetaA = std::acosf(Dot(AB, AC) / (AB.Length()*AC.Length()));
		const float thetaB = std::acosf(-Dot(AB, BC) / (AB.Length()*BC.Length()));
		const float thetaC = M_PI - thetaA - thetaB;
		ASSERT(thetaA >= 0 && thetaB >= 0 && thetaC >= 0);

		Vec3& normA = Normals()[indices[i]];
		Vec3& normB = Normals()[indices[i + 1]];
		Vec3& normC = Normals()[indices[i + 2]];
		normA += thetaA*Normal;
		normB += thetaB*Normal;
		normC += thetaC*Normal;
	}

	for (unsigned int v = 0; v < num_verts_; v++) {
		Vec3& normal = Normals()[v];
		normal = Normalize(normal);
	}
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

Mesh LoadPLY(const char* filename)
{
	Mesh mesh;
	auto ply = ply_open(filename, nullptr, 0, nullptr);
	if (!ply) {
		return mesh;
	}
	if (!ply_read_header(ply)) {
		return mesh;
	}

	Mallocator allocator;

	auto nverts = ply_set_read_cb(ply, "vertex", "x", VertexPLYCallback, &mesh, 0);
	ply_set_read_cb(ply, "vertex", "y", VertexPLYCallback, &mesh, 1);
	ply_set_read_cb(ply, "vertex", "z", VertexPLYCallback, &mesh, 2);

	FacePLYData data;
	auto nfaces = ply_set_read_cb(ply, "face", "vertex_indices", FacePLYCallback, &mesh, 0);
	mesh.index_block_ = allocator.Allocate(nfaces * 3 * sizeof(float));
	data.block = (int*)mesh.index_block_.ptr;
	data.index = 0;
	mesh.vertex_block_ = allocator.Allocate(nverts * 6 * sizeof(float));

	mesh.num_indices_ = 3 * nfaces;
	mesh.num_verts_ = nverts;
	if (!ply_read(ply)) {
		return mesh;
	}
	ply_close(ply);

	mesh.ComputeNormals();
	return mesh;
}

Mesh MakeSquare(int num_div_x, int num_div_y)
{
	Mesh mesh;
	mesh.num_verts_ = num_div_x*num_div_y;

	Mallocator allocator;

	mesh.vertex_block_ = allocator.Allocate(mesh.num_verts_ * 6 * sizeof(float));
	for (int x = 0; x < num_div_x; x++) {
		for (int y = 0; y < num_div_y; y++) {
			mesh.Positions()[x*num_div_y + y] = Vec3(x / (float)num_div_x, y / (float)num_div_y, 0.0);
		}
	}

	mesh.num_indices_ = 3 * 2 * (num_div_x - 1)*(num_div_y - 1);
	mesh.index_block_ = allocator.Allocate(mesh.num_indices_ * sizeof(unsigned int));
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

}