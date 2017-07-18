#include "Profiler.h"
#include "Profiler.h"

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "External/imgui/imgui.h"
#include "Utilities/Allocators.h"
#include "Utilities/Utilities.h"



#ifdef WIN32
#include <Windows.h>
#else
#include <chrono>
#endif


namespace rkg {
namespace profiler {

	struct alignas(rkg::GrowingLinearAllocator<1>::ALIGNMENT) BlockDescriptor 
	{
		unsigned int id;
		const char* name;
		const char* file;
		int line;

	};

	namespace
	{
		rkg::GrowingLinearAllocator<KILO(32)*sizeof(BlockDescriptor)> descriptor_allocator;
		std::atomic<unsigned int> num_descriptors = 0;

		struct Event
		{
			unsigned int id;
			uint64_t timestamp;
			bool block_end;
		};

		std::vector<Event> block_list;
		bool capture_enabled{ false };
		//TODO: Right now, non-windows platforms only have second precision, which sucks.

		uint64_t GetTimestamp()
		{
#ifdef WIN32
			LARGE_INTEGER counter;
			QueryPerformanceCounter(&counter);
			return (uint64_t)counter.QuadPart;
#else
			time_t current_time = time(nullptr);
			return (uint64_t)current_time;
#endif
		}


		uint64_t timer_frequency = 0;
		float TimestampToMilliseconds(uint64_t duration)
		{
#ifdef WIN32
			return (1000.f*duration) / timer_frequency;
#else
			return 1000.f*duration;
#endif
		}
	}


	Block::Block(const BlockDescriptor* descriptor) :
		id{descriptor->id}
	{
		if (capture_enabled) {
			block_list.emplace_back(Event{ id, GetTimestamp(), false });
		}
	}

	Block::~Block()
	{
		if (capture_enabled) {
			block_list.emplace_back(Event{ id, GetTimestamp(), true });
		}
	}

	const BlockDescriptor * rkg::profiler::RegisterBlock(const char * name, const char * file, int line)
	{
		//Allocate a block, and fill it with relivant info.
		auto mem = descriptor_allocator.Allocate(sizeof(BlockDescriptor));
		BlockDescriptor* block = nullptr;
		if (mem.ptr) {
			block= new(mem.ptr) BlockDescriptor;
			block->name = name;
			block->file = file;
			block->line = line;
			block->id = num_descriptors++;
		}
		return block;
	}



	void ProfilingSystem::Initialize()
	{
#ifdef WIN32
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		timer_frequency = frequency.QuadPart;
#endif
	}

	void ProfilingSystem::Update(double delta_time)
	{
		struct BlockUIData{
			unsigned int id;
			uint64_t start_time;
			uint64_t end_time;
			uint64_t duration;
			float display_length;
			bool is_root{ false };
		};

		static std::vector<BlockUIData> ui_data;


		if (capture_enabled) {
			capture_enabled = false; //Stop capturing.
			ui_data.clear();
			std::vector<BlockUIData> working_stack;
			for (const auto& b : block_list) {
				if (b.block_end) {
					//Pop off our working stack.
					working_stack.back().end_time = b.timestamp;
					working_stack.back().duration = b.timestamp - working_stack.back().start_time;
					working_stack.back().display_length = TimestampToMilliseconds(working_stack.back().duration);
					ui_data.emplace_back(working_stack.back());
					working_stack.pop_back();

				}
				else {
					//Push onto our working stack.
					BlockUIData data;
					data.start_time = b.timestamp;
					data.id = b.id;
					data.is_root = working_stack.size() == 0;
					working_stack.emplace_back(std::move(data));
				}
			}

			std::stable_sort(ui_data.begin(), ui_data.end(), 
				[](const BlockUIData& a, const BlockUIData& b) {
				return a.start_time < b.start_time;
			});

			block_list.clear();
		}


		//Draw the current events.
		ImGui::Begin("Profiler");

		//Right now, we'll just draw a tree view showing times and names.
		static int(*add_children)(const std::vector<BlockUIData>&, int, uint64_t)
			= [](const std::vector<BlockUIData>& ui, int index, uint64_t end_time) -> int {
			auto blocks = reinterpret_cast<BlockDescriptor*>(descriptor_allocator.Begin());
			while(index < ui.size() && ui[index].start_time < end_time) {
				ImGui::PushID(index);
				if (ImGui::TreeNode("profiler_node", "%s : %f ms", blocks[ui[index].id].name, ui[index].display_length)) {
					index = add_children(ui, index + 1, ui[index].end_time);
					ImGui::TreePop();
				}
				else {
					//If the tree is not open, need to find the next node at this level.
					int next_index = index + 1;
					while (next_index < ui.size() && ui[next_index].start_time < ui[index].end_time) {
						next_index++;
					}
					index = next_index;
				}
				ImGui::PopID();
			}
			return index;
		};

		if (ui_data.size() != 0) {
			uint64_t end_time = ui_data.back().start_time + ui_data.back().duration;
			add_children(ui_data, 0, end_time);

		}

		if (ImGui::Button("Capture Frame")) {
			CaptureFrame();
		}
		ImGui::End();

		if (capture_next_frame_) {
			capture_enabled = true;
			capture_next_frame_ = false;
		}
		
	}

	void ProfilingSystem::CaptureFrame()
	{
		//Set things up so that the next full frame will be captured.
		capture_next_frame_ = true;
	}

}
}