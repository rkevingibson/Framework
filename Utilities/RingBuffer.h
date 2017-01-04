#pragma once

#include "Utilities.h"

namespace rkg
{

/*
	Simple, single-threaded ring buffer.
*/
template<class ElementType, class Allocator>
class RingBuffer
{
private:
	using allocator_type = Allocator;
	using element_type = ElementType;

	unsigned int read_{ 0 };
	unsigned int write_{ 0 };

	MemoryBlock block_;
	element_type buffer_;
	size_t capacity_;

	allocator_type allocator_{};

public:
	RingBuffer(size_t capacity)
	{
		block_ = allocator_.Allocate(rkg::RoundToPow2(capacity));
		capacity_ = block_.length / sizeof(element_type);
		buffer_ = static_cast<element_type>(block_.ptr);
	}

	bool Push(const element_type& val)
	{
		if (write_ == read_ + capacity_) {
			return false;
		}
		buffer[write & (capacity_ - 1)] = val;
		write_++;
		return true;
	}

	bool Pop(element_type* val)
	{
		if (!val || read == write) {
			return false;
		}
		*val = buffer[read & (capacity_ - 1)];
		read++;
		return true;
	}

	void Resize(size_t new_capacity)
	{
		allocator_.Reallocate(block_, RoundToPow2(new_capacity));
	}
};

}