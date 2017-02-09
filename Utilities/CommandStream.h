#pragma once

#include <array>

#include "Utilities/Utilities.h"
#include "Utilities/Allocators.h"

namespace rkg
{

struct Cmd
{
	using DispatchFn = void(*)(Cmd*);
	DispatchFn dispatch;
	size_t command_size;
};

/*
Note: this isn't really thread-safe. The idea is that one thread executes one buffer, 
and one thread pushes to another. The SwapBuffers command switches the internal buffers,
but is not currently thread safe at all, and probably will never be.

*/

class CommandBuffer
{
private:
	char* read_pos_{ 0 };

	constexpr static unsigned int SIZE{ KILO(4) };
	
	using LinearBuffer = GrowingLinearAllocator<MEGA(2)>;
	
	LinearBuffer buffers_[2];

	LinearBuffer* execute_buffer_{ &buffers_[1] };
	LinearBuffer* write_buffer_{ &buffers_[0] };

public:

	inline void Execute(char* end)
	{
		while( read_pos_ < end && read_pos_ < execute_buffer_->End()) {
			Cmd* cmd = reinterpret_cast<Cmd*>(read_pos_);
			cmd->dispatch(cmd);
			read_pos_ = read_pos_ + cmd->command_size;
		}
	}

	template<typename T>
	inline bool Push(const T& t)
	{
		static_assert(std::is_base_of<Cmd, T>::value, "Adding invalid command");
		//We're strictly single-reader/singe-consumer right now.
		//This lock makes us safe to have multiple-readers/single-consumer.

		auto block = write_buffer_->Allocate(sizeof(T));
		if (block.ptr == nullptr) {
			return false;
		}
		//There's room, copy the data. 
		memcpy(block.ptr, &t, sizeof(T));
		auto cmd = reinterpret_cast<Cmd*>(&block.ptr);
		cmd->dispatch = T::DISPATCH;
		cmd->command_size = block.length;
		return true;
	}

	inline void SwapBuffers()
	{
		std::swap(execute_buffer_, write_buffer_);
		write_buffer_->DeallocateAll();
		read_pos_ = execute_buffer_->Begin();
	}
};

}