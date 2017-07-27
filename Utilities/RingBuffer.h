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
	element_type* buffer_;
	size_t capacity_;
	size_t mask_;
	allocator_type allocator_{};

public:
	RingBuffer(size_t capacity = 0)
	{
		block_ = allocator_.Allocate(rkg::RoundToPow2(capacity * sizeof(element_type)));
		capacity_ = block_.length / sizeof(element_type);
		mask_ = capacity_ - 1;
		buffer_ = static_cast<element_type*>(block_.ptr);
		//Need to initialize buffer_ objects to a valid state.
		for (int i = 0; i < capacity_; i++) {
			new(&buffer_[i]) element_type;
		}
	}

	RingBuffer(const RingBuffer&) = delete;
	RingBuffer(RingBuffer&&) = default;
	RingBuffer& operator=(const RingBuffer&) = delete;
	RingBuffer& operator=(RingBuffer&&) = default;

	bool Push(const element_type& val)
	{
		if (write_ == read_ + capacity_) {
			return false;
		}
		buffer[write & mask_] = val;
		write_++;
		return true;
	}

	bool Pop(element_type* val)
	{
		if (!val || read_ == write_) {
			return false;
		}
		*val = buffer_[read_ & mask_];
		read_++;
		return true;
	}

	//Add an element to the buffer, overwriting the oldest element if the buffer is full. Will never fail.
	bool PushOverwrite(const element_type& val)
	{
		if (write_ == read_ + capacity_) {
			//Buffer is full, we're overwriting.
			read_++;
		}
		buffer_[write_ & mask_] = val;
		write_++;

		return true;
	}

	//Access the elements as if it were a history - element 0 is the most recently pushed object.
	element_type& operator[](size_t i)
	{
		Expects(i < capacity_);
		return buffer_[(write_ - i - 1) & mask_];
	}

	const element_type& operator[](size_t i) const
	{
		Expects(i < capacity_);
		return buffer[(write - i - 1) & mask_];
	}

	//How many elements are currently in the buffer
	size_t Size() const
	{
		return (write_ - read_);
	}

	void Clear()
	{
		read_ = 0;
		write_ = 0;
	}

	void Resize(size_t new_capacity)
	{
		allocator_.Reallocate(block_, RoundToPow2(new_capacity));
		capacity_ = block_.length / sizeof(element_type);
		mask_ = capacity_ - 1;


		for (int i = 0; i < capacity_; i++) {
			new(&buffer_[i]) element_type;
		}
	}
};

}