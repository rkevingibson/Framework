#pragma once
#include <cstring>

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
	uint32_t back_mask_;

	Mallocator allocator_;

	void ResizeBackTable(uint32_t size);
	
};

inline void HashIndex::Add(const uint32_t key, const uint32_t index)
{

	if (index >= back_size_) {
		ResizeBackTable(index + 1);
	}

	uint32_t h = key & front_mask_;
	back_table_[index] = front_table_[h];
	front_table_[h] = index;
}

inline uint32_t HashIndex::First(const uint32_t key) const
{
	return front_table_[key & front_mask_];
}

inline uint32_t HashIndex::Next(const uint32_t index) const
{
	Expects(index < back_size_);
	return back_table_[index];
}

inline void HashIndex::Clear()
{
	memset(front_table_, 0xff, front_size_ * sizeof(uint32_t));
	memset(back_table_, 0xff, back_size_ * sizeof(uint32_t));
}

}