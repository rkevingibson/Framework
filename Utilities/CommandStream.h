#pragma once

#include <array>

#include "Utilities/Utilities.h"
#include "Utilities/Allocators.h"

namespace rkg
{

struct Cmd
{
	using CmdFn = void(*)(Cmd*);
	CmdFn dispatch;
	int offset;
	int cmd_size;
};

/*
Note: this isn't really thread-safe. The idea is that one thread executes one buffer, 
and one thread pushes to another. The SwapBuffers command switches the internal buffers,
but is not currently thread safe at all, and probably will never be.

*/

class CommandStream
{
private:

	constexpr static unsigned int SIZE{ KILO(4) };
	
	using LinearBuffer = GrowingLinearAllocator<MEGA(2)>;
	
	LinearBuffer buffers_[2];

	LinearBuffer* execute_buffer_{ &buffers_[1] };
	LinearBuffer* write_buffer_{ &buffers_[0] };

public:
	inline void ExecuteAll()
	{
		auto execute_pos_ = execute_buffer_->Begin();
		while (execute_pos_ < execute_buffer_->End()) {
			Cmd* cmd = reinterpret_cast<Cmd*>(execute_pos_);
			cmd->dispatch(cmd);
			execute_pos_ = execute_pos_ + sizeof(Cmd) + cmd->cmd_size;
		}
	}

	template<typename T>
	bool Add(T&& fn) 
	{
		//Todo: alignment
		//Need to align Cmd, and align T.
		//Our allocator has maximum alignment, so Cmd will always be aligned. 
		//Or do I need to do two allocations?
		constexpr size_t CMD_ALIGNMENT = alignof(Cmd);
		constexpr size_t FN_ALIGNMENT = alignof(T);

		auto block = write_buffer_->Allocate(sizeof(Cmd) + sizeof(T));
		if (block.ptr == nullptr) {
			return false;
		}
		
		Cmd* cmd = reinterpret_cast<Cmd*>(block.ptr);
		cmd->dispatch = [](Cmd* cmd) {
			auto f = reinterpret_cast<T*>(((char*)cmd) + sizeof(Cmd));
			f->operator()();
		};
		cmd->cmd_size = RoundToAligned(sizeof(T), write_buffer_->ALIGNMENT);
		//Copy the function.
		new(((char*)cmd) + sizeof(Cmd)) T(std::forward<T>(fn));

		return true;
	}

	inline void SwapBuffers()
	{
		std::swap(execute_buffer_, write_buffer_);
		write_buffer_->DeallocateAll();
		
	}
};

}