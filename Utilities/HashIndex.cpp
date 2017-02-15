#include "HashIndex.h"
#include <cstring>

namespace rkg
{
HashIndex::HashIndex()
{
	
}

HashIndex::~HashIndex()
{
	Free();

}

void HashIndex::Add(const uint32_t key, const uint32_t index)
{

	if (index > back_size_) {
		ResizeBackTable(index + 1);
	}

	uint32_t h = key & front_mask_;
	back_table_[index] = front_table_[h];
	front_table_[h] = index;
}

void HashIndex::Remove(const uint32_t key, const uint32_t index)
{
	//First, need to find the item, then remove it.
	auto k = key & front_mask_;
	if (front_table_[k] == index) {
		front_table_[k] = back_table_[index];
	}
	else {
		for (int i = front_table_[k]; i != INVALID_INDEX; i = back_table_[i]) {
			if (back_table_[i] == index) {
				back_table_[i] = back_table_[index];
				break;
			}
		}
	}
	back_table_[index] = -1;
}

uint32_t HashIndex::First(const uint32_t key) const
{
	return front_table_[key & front_mask_];
}

uint32_t HashIndex::Next(const uint32_t index) const
{
	Expects(index < back_size_);
	return back_table_[index];
}

void HashIndex::Clear()
{
	memset(front_table_, 0xff, front_size_ * sizeof(uint32_t));
	memset(back_table_, 0xff, back_size_ * sizeof(uint32_t));
}

void HashIndex::Free()
{
	if (front_table_) {
		MemoryBlock front_block;
		front_block.ptr = front_table_;
		front_block.length = sizeof(uint32_t)*front_size_;
		allocator_.Deallocate(front_block);
	}
	if (back_table_) {
		MemoryBlock back_block;
		back_block.ptr = back_table_;
		back_block.length = sizeof(uint32_t)*back_size_;
		allocator_.Deallocate(back_block);
	}
}

void HashIndex::ResizeBackTable(uint32_t size)
{
	size_t new_size = RoundToPow2(size);
	MemoryBlock b;
	b.ptr = back_table_;
	b.length = back_size_ * sizeof(uint32_t);
	allocator_.Reallocate(b, new_size * sizeof(uint32_t));

	back_table_ = static_cast<uint32_t*>(b.ptr);
	back_size_ = b.length / sizeof(uint32_t);
}

void HashIndex::Allocate(uint32_t front_size, uint32_t back_size)
{
	Free();
	front_size_ = front_size;
	auto front = allocator_.Allocate(front_size_ * sizeof(uint32_t));
	front_table_ = static_cast<uint32_t*>(front.ptr);

	back_size_ = back_size;
	auto back = allocator_.Allocate(back_size_ * sizeof(uint32_t));
	back_table_ = static_cast<uint32_t*>(back.ptr);
	
	front_mask_ = front_size - 1;
}

}