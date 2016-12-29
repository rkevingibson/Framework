#pragma once

#include <stdlib.h>

/*
	Memory allocators.
	Designed to be composable, and relatively simple.
	Based largely on Andrei Alexandrescu's 2015 talk at CppCon -
	"allocator is to Allocation what vector is to Vexation"

	Ideal, Complete Api for each allocator is as follows:
	--Always available--
	static constexpr unsigned alignment;
	MemoryBlock Allocate(size_t);
	void Reallocate(MemoryBlock&, size_t);
	void Deallocate(MemoryBlock);


	--Implemented when possible--
	MemoryBlock AllocateAll();
	void DeallocateAll();
	bool Owns(MemoryBlock);
	bool Expand(MemoryBlock&, size_t delta);

*/
namespace rkg
{

struct MemoryBlock
{
	void* ptr;
	size_t length;
};

inline size_t RoundToAligned(size_t n, size_t alignment)
{
	//Todo: need to figure out alignment. 
	//Not every allocator should be aligned the same.
	return n;
}

constexpr inline unsigned int Min(unsigned int a, unsigned int b)
{
	return (a < b) ? a : b;
}

template<class Primary, class Fallback>
class FallbackAllocator
{
	Primary p_;
	Fallback f_;
	/*
		FallbackAllocator: attempts to allocate using Primary,
		and uses Fallback if that fails.
	*/
public:
	static constexpr unsigned int alignment = Min(Primary::alignment, Fallback::alignment);

	inline MemoryBlock Allocate(size_t)
	{
		MemoryBlock r = p_.Allocate(n);
		if (!r.ptr) {
			r = f_.Allocate(n);
		}
		return r;
	}

	void Deallocate(MemoryBlock)
	{
		if (p_.Owns(b)) {
			p_.Deallocate(b);
		} else {
			F::Deallocate(b);
		}
	}

	bool Owns(MemoryBlock)
	{
		return p_.Owns(b) || f_.Owns(b);
	}

	void DeallocateAll()
	{
		p_.DeallocateAll();
		f_.DeallocateAll();
	}
};

template<size_t Size, size_t Alignment = 4> class StackAllocator
{
private:
	char stack_[Size];
	char* head_;


public:
	static constexpr unsigned alignment = Alignment;

	StackAllocator() : head_(stack_)
	{
	}

	MemoryBlock Allocate(size_t n)
	{
		auto n1 = RoundToAligned(n, Alignment);
		if (n1 > stack_ + Size - head_) {
			return{ nullptr, 0 };
		}
		MemoryBlock result = { head_, n };
		head_ += n1;
		return result;
	}

	bool Expand(MemoryBlock& b, size_t delta)
	{
		//If b is at the head of the stack, I might be able to grow it.
		if (b.ptr + RoundToAligned(b.length, Alignment) == head_) {
			auto n1 = RoundToAligned(delta, Alignment);
			if (n1 > stack_ + Size - head_) {
				return false;
			}
			head_ += n1;
			b.length += delta;
			return true;
		}

		return false;
	}

	void Reallocate(MemoryBlock& b, size_t new_size)
	{
		if (b.ptr + RoundToAligned(b.length, Alignment) == head_) {
			//At the head, so just check if we have space for the new one.
			auto n1 = RoundToAligned(new_size, Alignment);
			if (n1 <= stack_ + Size - b.ptr) {
				head_ = b.ptr + n1;
				b.length = n1;
			}
		} else {
			if (new_size > b.length) {
				//Reallocate at the head.
				auto new_block = Allocate(new_size);
				if (new_block.ptr) {
					b = new_block;
				}
			}
			//If shrinking while not at the head of the stack, do nothing.
		}
	}

	void Deallocate(MemoryBlock b)
	{
		if (b.ptr + RoundToAligned(b.length, Alignment) == head_) {
			head_ = b.ptr;
		}
	}

	bool Owns(MemoryBlock b)
	{
		return b.ptr >= stack_ && b.ptr < stack_ + s;
	}

	MemoryBlock AllocateAll()
	{
		size_t n = stack_ + Size - head_;
		if (n == 0) {
			return{ nullptr, 0 };
		}
		MemoryBlock result = { head_, n };
		head_ = stack_ + Size;
		return result;
	}

	void DeallocateAll()
	{
		head_ = stack_;
	}
};

template<class A, size_t Size> class Freelist
{
private:
	A parent_;
	struct Node
	{
		Node* next{ nullptr };
	};

	Node* root_;
public:
	static constexpr unsigned int alignment = A::alignment;

	MemoryBlock Allocate(size_t n)
	{
		if (n == Size && root_) {
			MemoryBlock b = { root_, n };
			root_ = root_->next;
			return b;
		}
		return parent_.Allocate(n);
	}

	void Deallocate(MemoryBlock b)
	{
		if (b.length != Size) {
			return parent_.Deallocate(b);
		}
		//Append to the intrusive linked list - don't free it from the parent.
		auto p = (Node*) b.ptr;
		p->next = root_;
		root_ = p;
	}

	void DeallocateAll()
	{
		parent_.DeallocateAll();
		root_ = nullptr;
	}
};

template<size_t threshold, class SmallAllocator, class LargeAllocator>
class Segregator
{
	SmallAllocator small;
	LargeAllocator large;

public:
	static constexpr unsigned int alignment = Min(SmallAllocator::alignment, LargeAllocator::alignment);

	MemoryBlock Allocate(size_t n)
	{
		if (n <= threshold) {
			return small.Allocate(n);
		}
		return large.Allocate(n);
	}

	void Reallocate(MemoryBlock& b, size_t new_size)
	{
		if (b.length < threshold) {
			if (new_size > threshold) {
				auto new_block = large.Allocate(new_size);
				if (new_block.ptr) {
					small.Deallocate(b);
					b = new_block;
				}
			} else {
				small.Reallocate(b, new_size);
			}
		} else {
			if (new_size < threshold) {
				auto new_block = small.Allocate(new_size);
				if (new_block.ptr) {
					large.Deallocate(b);
					b = new_block;
				}
			} else {
				large.Reallocate(b, new_size);
			}
		}
	}

	bool Expand(MemoryBlock& b, size_t delta)
	{
		if (b.length <= threshold && b.length + delta > threshold) {
			return false;
		}

		if (b.length <= threshold) {
			return small.Expand(b, delta);
		}
		else {
			return large.Expand(b, delta);
		}
	}

	void Deallocate(MemoryBlock b)
	{
		if (b.length <= threshold) {
			small.Deallocate(b);
		} else {
			large.Deallocate(b);
		}
	}

	bool Owns(MemoryBlock b)
	{
		if (b.length <= threshold) {
			return small.Owns(b);
		} else {
			return large.Owns(b);
		}
	}

	void DeallocateAll()
	{
		small.DeallocateAll();
		large.DeallocateAll();
	}
};

/*
	Allocator which uses malloc to create its memory blocks.
	Good to use as a final fallback.
*/
class Mallocator
{
public:
	static constexpr unsigned int alignment = 1;

	MemoryBlock Allocate(size_t n)
	{
		void* ptr = malloc(n);
		if (!ptr) {
			return{ nullptr, 0 };
		}
		return{ ptr, n };
	}

	void Deallocate(MemoryBlock b)
	{
		free(b.ptr);
	}

	void Reallocate(MemoryBlock b, size_t new_size)
	{
		auto ptr = realloc(b.ptr, new_size);
		if (ptr) {
			b.ptr = ptr;
			b.length = new_size;
		}
	}
};

}//end namespace rkg