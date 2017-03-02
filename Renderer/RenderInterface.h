#pragma once

#include "Utilities/Utilities.h"
#include "Utilities/Geometry.h"
#include "Utilities/Allocators.h"

#include <string>
#include <unordered_map>
#include <memory>
struct GLFWwindow;

namespace rkg
{
namespace render
{

using RenderResource = uint64_t;

enum class ResourceType : uint8_t
{
	GEOMETRY = 0,
	MATERIAL,
	MESH,
	NUM_HANDLE_TYPES
};

inline ResourceType GetResourceType(RenderResource h)
{
	auto result = static_cast<uint8_t>(h >> 56);
	Ensures(result < static_cast<uint8_t>(ResourceType::NUM_HANDLE_TYPES));
	return static_cast<ResourceType>(result);
}

struct VertexLayout
{

	static constexpr uint8_t MAX_ATTRIBUTES{ 16 };
	enum class AttributeType : uint8_t
	{
		INT8,
		UINT8,

		INT16,
		UINT16,
		FLOAT16,

		INT32,
		UINT32,
		PACKED_2_10_10_10_REV,
		UPACKED_2_10_10_10_REV,
		FLOAT32,

		FLOAT64,

		UNUSED = MAX_ATTRIBUTES,
		COUNT
	};

	inline static size_t SizeOfType(AttributeType t) {
		switch (t)
		{
		case AttributeType::INT8:
		case AttributeType::UINT8:
			return 1;
			break;
		case AttributeType::INT16:
		case AttributeType::UINT16:
		case AttributeType::FLOAT16:
			return 2;
			break;
		case AttributeType::INT32:
		case AttributeType::UINT32:
		case AttributeType::PACKED_2_10_10_10_REV:
		case AttributeType::UPACKED_2_10_10_10_REV:
		case AttributeType::FLOAT32:
			return 4;
			break;
		case AttributeType::FLOAT64:
			return 8;
			break;
		case AttributeType::UNUSED:
		case AttributeType::COUNT:
		default:
			return 0;
			break;
		}
	}
	
	struct AttributeBinding {
		enum Enum : uint8_t
		{
			POSITION = 0,
			NORMAL,
			TANGENT,
			BITANGENT,
			COLOR0,
			COLOR1,
			INDEX,
			WEIGHT,
			TEXCOORD0,
			TEXCOORD1,
			TEXCOORD2
		};
	};

	inline VertexLayout() {
		for (int i = 0; i < MAX_ATTRIBUTES; i++) {
			types[i] = AttributeType::UNUSED;
		}
	}
	VertexLayout(const VertexLayout&) = default;
	VertexLayout(VertexLayout&&) = default;
	VertexLayout& operator=(const VertexLayout&) = default;
	VertexLayout& operator=(VertexLayout&&) = default;

	
	inline VertexLayout& Add(AttributeBinding::Enum binding, uint8_t num, AttributeType type, bool normalized = false)
	{
		Expects(binding < MAX_ATTRIBUTES);
		Expects(num <= 4);
		types[num_attributes] = type;
		//Pack the normalized bit into the high bit of the count.
		counts[num_attributes] = ((num-1) & 0b0000'0011) 
								 | ((binding << 2) & 0b0111'1100)
								 | ((uint8_t)normalized << 7);
		num_attributes++;
		return *this;
	}

	inline size_t SizeOfVertex()
	{
		size_t s = 0;
		for (int i = 0; i < MAX_ATTRIBUTES; i++) {
			s += SizeOfType(types[i])*((counts[i] & 0b0000'0011) + 1);
		}
		return s;
	}

	uint8_t num_attributes{ 0 };
	AttributeType types[MAX_ATTRIBUTES];
	uint8_t counts[MAX_ATTRIBUTES]{ 0 };
	bool interleaved{ false };
};

//Types that can be used for an index buffer
enum class IndexType
{
	UByte,
	UShort,
	UInt,
};

class PropertyBlock
{
	//TODO: Replace the implementation of this with 
	//something I have more memory control over.
	//Also, see if I can replace the string dependency with a hash instead.


public:
	struct Property
	{
		uint32_t offset;
		uint32_t size;
		uint32_t type;
		uint16_t array_stride;
		uint16_t matrix_stride;
	};
	bool dirty = false;
	
	inline void SetProperty(const char* name, const void* value, int size)
	{
		if (properties.count(name) == 1) {
			auto& prop = properties[name];
			char* dest = buffer.get() + prop.offset;
			//TODO: Size checking.
			memcpy(dest, value, size);
			dirty = true;
		}
	}


	std::unordered_map<std::string, Property> properties;
	std::unique_ptr<char[]> buffer;
	size_t buffer_size;
};


void Initialize(GLFWwindow* window);
void ResizeWindow(int w, int h);

//void UpdateMeshData(RenderHandle mesh, const MemoryBlock* vertex_data, const MemoryBlock* index_data);

RenderResource CreateGeometry(const MemoryBlock* vertex_data, const VertexLayout& layout, const MemoryBlock* index_data, IndexType type);
void UpdateGeometry(const RenderResource geometry, const MemoryBlock* vertex_data, const MemoryBlock* index_data);
void DeleteGeometry(RenderResource geometry);

RenderResource CreateMaterial(const MemoryBlock* vertex_shader, const MemoryBlock* frag_shader);
void SetMaterialParameter(const RenderResource material, const char* name, const void* value, size_t size);
void DeleteMaterial(RenderResource material);

RenderResource CreateMesh(const RenderResource  geometry, const RenderResource material);
void DeleteMesh(const RenderResource mesh);

void SetModelTransform(const RenderResource mesh, const Mat4& matrix);
void SetViewTransform(const Mat4& matrix);
void SetProjectionTransform(const Mat4& matrix);

void EndFrame();

//
//IMGUI FUNCTIONS
//

void InitImguiRendering(const MemoryBlock* font_data, int width, int height);
void UpdateImguiData(const MemoryBlock* vertex_data, const MemoryBlock* index_data, const Vec2& display_size);
void DrawImguiCmd(uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, uint32_t scissor_x, uint32_t scissor_y, uint32_t scissor_w, uint32_t scissor_h);



}
}

