#pragma once
#include "ECS/Systems.h"

namespace rkg
{
namespace profiler
{
	struct BlockDescriptor;
	
	struct Block
	{
		Block(const BlockDescriptor*);
		~Block();
		Block(const Block&) = delete;
		Block(Block&&) =delete;
		Block& operator=(const Block&) = delete;
		Block& operator=(Block&&) = delete;

		unsigned int id;

	};
	
	const BlockDescriptor* RegisterBlock(const char* name, const char* file, int line);
	

#define PROFILE_CONCATENATE_TOKENS(x, y) x##y
#define PROFILE_BLOCK_DESCRIPTOR_NAME(line) PROFILE_CONCATENATE_TOKENS(profile_block_descriptor_, line)
#define PROFILE_BLOCK_NAME(line) PROFILE_CONCATENATE_TOKENS(profile_block, line)

	//Public API:

#define PROFILE_BLOCK(name) \
	static auto PROFILE_BLOCK_DESCRIPTOR_NAME(__LINE__) = rkg::profiler::RegisterBlock(name, __FILE__, __LINE__);\
	rkg::profiler::Block PROFILE_BLOCK_NAME(__LINE__)(PROFILE_BLOCK_DESCRIPTOR_NAME(__LINE__))


	
	class ProfilingSystem : public ecs::System
	{
	public:
		ProfilingSystem() = default;
		virtual ~ProfilingSystem() = default;
		ProfilingSystem(const ProfilingSystem&) = delete;
		ProfilingSystem(ProfilingSystem&&) = default;
		ProfilingSystem& operator=(const ProfilingSystem&) = delete;
		ProfilingSystem& operator=(ProfilingSystem&&) = default;

		virtual void Initialize() override;
		virtual void Update(double delta_time) override;

		void CaptureFrame();

	private:
		bool capture_next_frame_{ false };
	};
	
}
}