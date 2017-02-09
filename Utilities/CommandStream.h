#pragma once

#include <array>

#include "Utilities\Utilities.h"



struct Cmd
{
	using DispatchFn = void(*)(Cmd*);
	DispatchFn dispatch;
	size_t command_size;
};

class CommandBuffer
{
private:
	unsigned int write_pos_{ 0 };
	unsigned int read_pos_{ 0 };

	constexpr static unsigned int SIZE{ KILO(4) };
	std::array<rkg::byte, SIZE> buffer_[2];//Allocate a 4k static buffer

public:

	void Execute(unsigned int end)
	{
		for (unsigned int i = read_pos_; i < end; i++, read_pos_++) {
			Cmd* cmd = reinterpret_cast<Cmd*>(&buffer_[read_pos_ % SIZE]);
			cmd->dispatch(cmd);
			read_pos_ += cmd->command_size;
		}
	}

	template<typename T>
	inline bool Push(const T& t)
	{
		static_assert(std::is_base_of<Cmd, T>::value, "Adding invalid command");
		//We're strictly single-reader/singe-consumer right now.
		//This lock makes us safe to have multiple-readers/single-consumer.
		unsigned int wpos = write_pos_;

		//Check to make sure this command fits before the end, 
		//since I'm doing a dumb memcpy.
		if ((wpos % SIZE) + sizeof(T) > SIZE) {
			wpos = wpos + SIZE - (wpos % SIZE);
		}

		//Check that there's room in the queue.
		if (wpos + sizeof(T) > read_pos_ + SIZE) {//NB: read_pos_ does an atomic read - should be okay.
			return false;
		}

		//There's room, copy the data. 
		memcpy(&buffer_[wpos % SIZE], &t, sizeof(T));
		auto cmd = reinterpret_cast<Cmd*>(&buffer_[wpos % SIZE]);
		cmd->dispatch = T::DISPATCH;
		cmd->command_size = sizeof(T);
		
		//Finally, increment the buffer pointer, indicating that an item has been added.
		write_pos_ = wpos + sizeof(T);
		return true;
	}
};

