#pragma once
#include "Utilities/Utilities.h"
#include "Utilities/Allocators.h"

namespace rkg
{

class HashIndex
{
public:
	HashIndex();
	~HashIndex();


	void Add(const uint32_t key, const uint32_t index);
	void Remove(const uint32_t key, const uint32_t index);

	uint32_t First(const uint32_t key) const;
	uint32_t Next(const uint32_t index) const;

	void Clear();
	void Free();
	void Allocate(uint32_t front_size, uint32_t back_size);

	static constexpr uint32_t INVALID_INDEX{ UINT32_MAX };
private:
	uint32_t front_size_;
	uint32_t* front_table_{ nullptr };
	uint32_t back_size_;
	uint32_t* back_table_{ nullptr };

	uint32_t front_mask_;

	Mallocator allocator_;

	void ResizeBackTable(uint32_t size);
	
};

}