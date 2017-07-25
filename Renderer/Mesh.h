#pragma once

#include <cstdint>
#include <vector>
#include <array>

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

struct MeshAttributes 
{
	enum Enum : uint16_t
	{
	//These are Vec3 attributes
		POSITION  = 0b0000'0000'0001,
		NORMAL    = 0b0000'0000'0010,
		TANGENT   = 0b0000'0000'0100,
		BITANGENT = 0b0000'0000'1000,

		//These are Vec4 attributes.
		COLOR0 = 0b0000'0001'0000,
		COLOR1 = 0b0000'0010'0000,
		INDEX  = 0b0000'0100'0000,
		WEIGHT = 0b0000'1000'0000,

		//Texcoords are Vec2 attributes.
		TEXCOORD0 = 0b0001'0000'0000,
		TEXCOORD1 = 0b0010'0000'0000,
		TEXCOORD2 = 0b0100'0000'0000,

		COUNT = 11,
	};
};

class Mesh
{
public:

	Mesh() = default;
	Mesh(const Mesh&);
	Mesh(const Eigen::Matrix3Xf& V, const Eigen::Matrix3Xi& F);

	Mesh& operator=(const Mesh&);
	Mesh(Mesh&&) noexcept;
	Mesh& operator=(Mesh&&) noexcept;
	~Mesh();


	//Raw access to the vertex parameters.
	inline Vec3* Positions()
	{
		return GetVec3Attribute(MeshAttributes::POSITION);
	}

	inline const Vec3* Positions() const
	{
		return GetVec3Attribute(MeshAttributes::POSITION);
	}

	inline Vec3* Normals()
	{
		return GetVec3Attribute(MeshAttributes::NORMAL);
	}

	inline const Vec3* Normals() const
	{
		return GetVec3Attribute(MeshAttributes::NORMAL);
	}

	inline bool HasColors() const 
	{
		return (active_attributes_ & MeshAttributes::COLOR0) != 0;
	}

	inline Vec4* Colors()
	{
		return GetVec4Attribute(MeshAttributes::COLOR0);
	}

	inline const Vec4* Colors() const
	{
		return GetVec4Attribute(MeshAttributes::COLOR0);
	}

	inline unsigned int* Indices()
	{
		return static_cast<unsigned int*>(index_block_.ptr);
	}

	inline const unsigned int* Indices() const
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

	void ComputeBounds(Vec3* min, Vec3* max);

	inline int VertexSize() const
	{
		return vertex_size_;
	}

private:
	void SetMeshAttributes(uint16_t attributes);
	void ComputeAttributeOffsets();
	inline Vec3* GetVec3Attribute(MeshAttributes::Enum attr)
	{
		int index = log2(static_cast<size_t>(attr));
		return reinterpret_cast<Vec3*>(static_cast<float*>(vertex_block_.ptr) +
									   attribute_offset_[index]);
	}
	inline const Vec3* GetVec3Attribute(MeshAttributes::Enum attr) const
	{
		int index = log2(static_cast<size_t>(attr));
		return reinterpret_cast<Vec3*>(static_cast<float*>(vertex_block_.ptr) +
									   attribute_offset_[index]);
	}

	inline Vec4* GetVec4Attribute(MeshAttributes::Enum attr)
	{
		int index = log2(static_cast<size_t>(attr));
		return reinterpret_cast<Vec4*>(static_cast<float*>(vertex_block_.ptr) +
									   attribute_offset_[index]);
	}
	inline const Vec4* GetVec4Attribute(MeshAttributes::Enum attr) const
	{
		int index = log2(static_cast<size_t>(attr));
		return reinterpret_cast<Vec4*>(static_cast<float*>(vertex_block_.ptr) +
									   attribute_offset_[index]);
	}

	inline Vec2* GetVec2Attribute(MeshAttributes::Enum attr)
	{
		int index = log2(static_cast<size_t>(attr));
		return reinterpret_cast<Vec2*>(static_cast<float*>(vertex_block_.ptr) +
									   attribute_offset_[index]);
	}
	inline const Vec2* GetVec2Attribute(MeshAttributes::Enum attr) const
	{
		int index = log2(static_cast<size_t>(attr));
		return reinterpret_cast<Vec2*>(static_cast<float*>(vertex_block_.ptr) +
									   attribute_offset_[index]);
	}

	void AllocateVertexMemory(uint32_t num_vertices);

	rkg::MemoryBlock vertex_block_{ nullptr,0 };
	rkg::MemoryBlock index_block_{ nullptr, 0 };
	uint32_t num_verts_{ 0 };
	uint32_t num_indices_{ 0 };
	uint16_t active_attributes_{ 1 }; //By default, have positions enabled
	std::array<uint32_t, MeshAttributes::COUNT> attribute_offset_{ 0 };
	int vertex_size_;



	friend std::unique_ptr<Mesh> LoadPLY(const char* filename);
	friend std::unique_ptr<Mesh> LoadOBJ(const char* filename);
	friend Mesh MakeSquare(int num_div_x, int num_div_y);
	friend Mesh MakeIcosphere(int num_divisions);

	/*
		Returns a new mesh where each face is made of 3 unique vertices - duplicating shared verts.
		Useful for adding per-face colors. This has the effect of no longer needing an index list, really - the indices are just 1..n
	*/
	friend Mesh SplitFaces(const Mesh& mesh);
	
	/*
		
	*/
	friend Mesh ApplyPerFaceColor(const Mesh& mesh, const std::vector<Vec4>& colors);
	friend Mesh AddPerVertexColor(const Mesh& mesh, const std::vector<Vec4>& colors);


	//TODO: Fix this crappy attribute code. Let us set how large each attribute is, and treat as a byte array. 
	//Use template functions to get attributes if I don't feel like dealing with floats. E.g. GetAttribute<Vec3>(MeshAttribute::Position);
	//Can optionally specialize on the standard ones, asserting to enforce convention.
};

std::unique_ptr<Mesh> LoadPLY(const char* filename);
std::unique_ptr<Mesh> LoadOBJ(const char* filename);
Mesh MakeSquare(int num_div_x, int num_div_y);
Mesh MakeIcosphere(int num_divisions);
Mesh ApplyPerFaceColor(const Mesh& mesh, const std::vector<Vec4>& colors);
Mesh AddPerVertexColor(const Mesh& mesh, const std::vector<Vec4>& colors);
}