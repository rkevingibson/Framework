#pragma once

#include <stdlib.h>

#include "Utilities.h"

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
	static constexpr unsigned int ALIGNMENT = Min(Primary::ALIGNMENT, Fallback::ALIGNMENT);

	FallbackAllocator() = default;
	FallbackAllocator(const FallbackAllocator&) = default;
	FallbackAllocator(FallbackAllocator&&) = default;
	FallbackAllocator& operator=(const FallbackAllocator&) = default;
	FallbackAllocator& operator=(FallbackAllocator&&) = default;

	inline MemoryBlock Allocate(size_t)
	{
		MemoryBlock r = p_.Allocate(n);
		if (!r.ptr) {
			r = f_.Allocate(n);
		}
		return r;
	}

	inline void Reallocate(MemoryBlock& b, size_t new_size)
	{
		if (p_.Owns(b)) {
			return p_.Reallocate(b, new_size);
		} else {
			return f_.Reallocate(b, new_size);
		}
	}

	void Deallocate(MemoryBlock b)
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
	static constexpr unsigned ALIGNMENT = Alignment;

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
	static constexpr unsigned int ALIGNMENT = A::ALIGNMENT;

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
	SmallAllocator small_allocator;
	LargeAllocator large_allocator;

public:
	static constexpr unsigned int ALIGNMENT = Min(SmallAllocator::ALIGNMENT, LargeAllocator::ALIGNMENT);

	MemoryBlock Allocate(size_t n)
	{
		if (n <= threshold) {
			return small_allocator.Allocate(n);
		}
		return large_allocator.Allocate(n);
	}

	void Reallocate(MemoryBlock& b, size_t new_size)
	{
		if (b.length < threshold) {
			if (new_size > threshold) {
				auto new_block = large_allocator.Allocate(new_size);
				if (new_block.ptr) {
					small_allocator.Deallocate(b);
					b = new_block;
				}
			} else {
				small_allocator.Reallocate(b, new_size);
			}
		} else {
			if (new_size < threshold) {
				auto new_block = small_allocator.Allocate(new_size);
				if (new_block.ptr) {
					large_allocator.Deallocate(b);
					b = new_block;
				}
			} else {
				large_allocator.Reallocate(b, new_size);
			}
		}
	}

	bool Expand(MemoryBlock& b, size_t delta)
	{
		if (b.length <= threshold && b.length + delta > threshold) {
			return false;
		}

		if (b.length <= threshold) {
			return small_allocator.Expand(b, delta);
		} else {
			return large_allocator.Expand(b, delta);
		}
	}

	void Deallocate(MemoryBlock b)
	{
		if (b.length <= threshold) {
			small_allocator.Deallocate(b);
		} else {
			large_allocator.Deallocate(b);
		}
	}

	bool Owns(MemoryBlock b)
	{
		if (b.length <= threshold) {
			return small_allocator.Owns(b);
		} else {
			return large_allocator.Owns(b);
		}
	}

	void DeallocateAll()
	{
		small_allocator.DeallocateAll();
		large_allocator.DeallocateAll();
	}
};


/*
	Affix Allocator:
	Allocates using the supplied allocator, but with extra space before and after to make room for a prefix and suffix.
	The returned memory block is to the original allocation, so the prefix/suffix space can be ignored.
	Should be used by a child allocator (using private inheritance) to specify behaviour about the prefix and suffix.

*/
template<class Allocator, class Prefix, class Suffix = void>
class AffixAllocator
{
	Allocator allocator_;
public:
	static constexpr unsigned int ALIGNMENT = Allocator::ALIGNMENT;
	static_assert(ALIGNMENT >= alignof(Prefix), "Invalid alignment for prefix.");

	MemoryBlock Allocate(size_t n)
	{
		//Need to allocate n + sizeof(Prefix) + sizeof(Suffix) at least, plus anything needed for alignment.
		size_t size = RoundToAligned(sizeof(Prefix), ALIGNMENT)
			+ RoundToAligned(n, alignof(Suffix))
			+ sizeof(Suffix);
		MemoryBlock block = allocator_.Allocate(size);

		if (block.length != 0) {
			block.ptr = reinterpret_cast<void*>(reinterpret_cast<char*>(block.ptr) + RoundToAligned(sizeof(Prefix), ALIGNMENT));
			block.length = block.length - RoundToAligned(sizeof(Prefix), ALIGNMENT) - sizeof(Suffix);
		}

		return block;
	}

	void Deallocate(MemoryBlock b)
	{
		b.length += RoundToAligned(sizeof(Prefix), ALIGNMENT) + sizeof(Suffix);
		b.ptr = reinterpret_cast<void*>(reinterpret_cast<char*>(block.ptr) - RoundToAligned(sizeof(Prefix), ALIGNMENT));
		allocator_.Deallocate(b);
	}

	void Reallocate(MemoryBlock& b, size_t new_size)
	{
		b.length += RoundToAligned(sizeof(Prefix), ALIGNMENT) + sizeof(Suffix);
		b.ptr = reinterpret_cast<void*>(reinterpret_cast<char*>(block.ptr) - RoundToAligned(sizeof(Prefix), ALIGNMENT));
		size_t size = RoundToAligned(sizeof(Prefix), ALIGNMENT)
			+ RoundToAligned(n, alignof(Suffix))
			+ sizeof(Suffix);

		allocator_.Reallocate(b, size);

		if (block.length != 0) {
			block.ptr = reinterpret_cast<void*>(reinterpret_cast<char*>(block.ptr) + RoundToAligned(sizeof(Prefix), ALIGNMENT));
			block.length = block.length - RoundToAligned(sizeof(Prefix), ALIGNMENT) - sizeof(Suffix);
		}
	}

	bool Owns(MemoryBlock b)
	{
		b.length += RoundToAligned(sizeof(Prefix), ALIGNMENT) + sizeof(Suffix);
		b.ptr = reinterpret_cast<void*>(reinterpret_cast<char*>(block.ptr) - RoundToAligned(sizeof(Prefix), ALIGNMENT));
		return allocator_.Owns(b);
	}
};

/*
	Allocator which uses malloc to create its memory blocks.
	Good to use as a final fallback.
*/
class Mallocator
{
public:
	static constexpr unsigned int ALIGNMENT = 8; //Actually is 16, on 64bit windows.

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

	void Reallocate(MemoryBlock& b, size_t new_size)
	{
		auto ptr = realloc(b.ptr, new_size);
		if (ptr) {
			b.ptr = ptr;
			b.length = new_size;
		}
	}
};

template<size_t MaximumSize>
class GrowingLinearAllocator
{
private:
	char* virtual_memory_start_;
	char* virtual_memory_end_;
	char* physical_memory_current_;
	char* physical_memory_end_;

public:
	static constexpr unsigned int ALIGNMENT = { alignof(std::max_align_t) };

	GrowingLinearAllocator() :
		virtual_memory_start_{ static_cast<char*>(virtual_memory::ReserveAddressSpace(MaximumSize)) },
		virtual_memory_end_{ virtual_memory_start_ + MaximumSize },
		physical_memory_current_{ virtual_memory_start_ },
		physical_memory_end_{ virtual_memory_start_ }
	{}

	//This is useful for debugging - can try and allocate in the same spot every time.
	GrowingLinearAllocator(void* location) :
		virtual_memory_start_{ virtual_memory::ReserveAddressSpace(MaximumSize, location) },
		virtual_memory_end_{ virtual_memory_start_ + MaximumSize },
		physical_memory_current_{ virtual_memory_start_ },
		physical_memory_end_{ virtual_memory_start_ }
	{}

	GrowingLinearAllocator(GrowingLinearAllocator&&) = default;
	GrowingLinearAllocator(const GrowingLinearAllocator&) = delete;
	GrowingLinearAllocator& operator=(GrowingLinearAllocator&&) = default;
	GrowingLinearAllocator& operator=(const GrowingLinearAllocator&) = delete;


	~GrowingLinearAllocator()
	{
		virtual_memory::ReleaseAddressSpace(virtual_memory_start_);
	}

	inline MemoryBlock Allocate(size_t size)
	{
		//TODO: Worry about alignment stuff.
		size_t size_to_allocate = RoundToAligned(size, ALIGNMENT);

		if (physical_memory_current_ + size_to_allocate > physical_memory_end_) {
			//Allocate a new page. 

			if (physical_memory_end_ + virtual_memory::PAGE_SIZE > virtual_memory_end_) {
				return MemoryBlock{ nullptr, 0 };
			}

			virtual_memory::AllocatePhysicalMemory(physical_memory_end_, virtual_memory::PAGE_SIZE);
			physical_memory_end_ += virtual_memory::PAGE_SIZE;
		}
		

		MemoryBlock result{ physical_memory_current_, size_to_allocate };
		physical_memory_current_ += size_to_allocate;
		return result;
	}

	inline void Reallocate(MemoryBlock&, size_t)
	{
		//TODO:
		ASSERT(false);
	}

	inline void Deallocate(MemoryBlock)
	{
		//TODO:
		ASSERT(false);
	}

	inline void DeallocateAll()
	{
		virtual_memory::DeallocatePhysicalMemory(virtual_memory_start_, static_cast<char*>(physical_memory_current_) - static_cast<char*>(virtual_memory_start_));
		physical_memory_current_ = virtual_memory_start_;
		physical_memory_end_ = virtual_memory_start_;
	}

	inline bool Owns(MemoryBlock b)
	{
		return b.ptr > physical_memory_start_ && b.ptr < physical_memory_end_;
	}

	inline char* Begin()
	{
		return static_cast<char*>(virtual_memory_start_);
	}

	inline char* End()
	{
		return static_cast<char*>(physical_memory_current_);
	}
};



/*
Virtual memory:
Here is a simple wrapper for some virtual memory functionality that I will eventually want to make cross-platform.
*/

namespace virtual_memory
{
static constexpr size_t PAGE_SIZE{ 4096 };

void* ReserveAddressSpace(size_t size, void* location = nullptr);
void* AllocatePhysicalMemory(void* location, size_t size);
void ReleaseAddressSpace(void* location);
void DeallocatePhysicalMemory(void* ptr, size_t size);
}


}//end namespace rkg