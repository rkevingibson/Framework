#pragma once

#include <cstdint>
#include <Utilities/Utilities.h>
#include <Utilities/Geometry.h>
#include <Renderer/RenderInterface.h>

#include <External\Eigen\Core>

namespace rkg
{

/*
	Triangle mesh Geometry class. Designed to work well with Eigen and our openGL backend.
	Can get Face and Vertex matrices as Eigen maps, as well as
*/

class Mesh
{
public:

	Mesh() = default;
	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;
	Mesh(Mesh&&) = default;
	Mesh& operator=(Mesh&&) = default;



	//Raw access to the vertex parameters.

	inline Vec3* Positions()
	{
		return static_cast<Vec3*>(vertex_block_.ptr);
	}

	inline Vec3* Normals()
	{
		return static_cast<Vec3*>(vertex_block_.ptr) + num_verts_;
	}

	inline unsigned int* Indices()
	{
		return static_cast<unsigned int*>(index_block_.ptr);
	}

	inline uint32_t NumVertices() { return num_verts_; }
	inline uint32_t NumIndices() { return num_indices_; }

	/*Render helper functions*/
	inline const void* VertexBuffer() { return vertex_block_.ptr; };
	inline size_t VertexBufferSize() { return vertex_block_.length; };
	inline const void* IndexBuffer() { return index_block_.ptr; };
	inline size_t IndexBufferSize() { return index_block_.length; };

	render::VertexLayout GetVertexLayout();


	/*
		Returns a 3xn matrix containing the vertex positions of the mesh.
	*/
	inline Eigen::Map<Eigen::Matrix<float, 3, Eigen::Dynamic>> PositionMatrix()
	{
		return Eigen::Map<Eigen::Matrix<float, 3, Eigen::Dynamic>>((float*)vertex_block_.ptr, 3, num_verts_);
	}

	/*
		Returns a 3x(# tris) matrix containing the indices of the faces of the mesh.
	*/
	inline Eigen::Map<Eigen::Matrix<unsigned int, 3, Eigen::Dynamic>> FaceMatrix()
	{
		return Eigen::Map<Eigen::Matrix<unsigned int, 3, Eigen::Dynamic>>((unsigned int*)index_block_.ptr, 3, num_indices_ / 3);
	}
	void ComputeNormals();

	inline Vec3 ComputeCentroid()
	{
		Vec3 centroid(0,0,0);
		for (int i = 0; i < num_verts_; i++) {
			centroid += Positions()[i];
		}
		centroid /= num_verts_;
		return centroid;
	}

private:
	


	rkg::MemoryBlock vertex_block_{ nullptr,0 };
	rkg::MemoryBlock index_block_{ nullptr, 0 };
	uint32_t num_verts_{ 0 };
	uint32_t num_indices_{ 0 };


	friend Mesh LoadPLY(const char* filename);
	friend Mesh LoadOBJ(const char* filename);

	friend Mesh MakeSquare(int num_div_x, int num_div_y);
};

}