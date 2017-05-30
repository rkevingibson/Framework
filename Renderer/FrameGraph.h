#pragma once
#include "Utilities/Utilities.h"
#include <vector>
#include "RenderInterface.h"
#include "Utilities/Allocators.h"
/*
	This is a greatly simplified version of the framegraph stuff presented by Frostbite at GDC 2017.
	Even then, it's quite different right now. Their system was hinging on a virtual memory system which I do not want to implement right now.
	So rather than generating a frame graph for every frame, we generate once at startup. But the general idea is to be able to declare render passes and make
	them easy to execute + write. Later on, I'll grow this and maybe add the virtual memory system.

	Right now, it's not even really a graph - just a list. I've kept the name, even if it is misleading.
*/
namespace rkg
{

namespace render
{

class FrameGraph
{
public:
	struct RenderPassHeader
	{
		//Any other info we need?
		int size;
		using ExecuteFn = void(*)();
		virtual void Execute() = 0;
	};

	template<typename UserData, typename ExecuteFunction>
	struct RenderPass : RenderPassHeader
	{
		UserData user_data;
		ExecuteFunction execute_fn;

		RenderPass(ExecuteFunction&& e) : execute_fn(std::forward<decltype(e)>(e))
		{
		}
		
		inline virtual void Execute() override
		{
			execute_fn(user_data);
		}
	};

	template<typename PassData, typename SetupFn, typename ExecuteFn>
	void AddCallbackPass(const char* pass_name, SetupFn&& setup, ExecuteFn&& exec)
	{
		//Add the pass to some buffer that I can later execute...
		//Call the setup function
		using Pass = RenderPass<PassData, decltype(exec)>;

		auto pass_block = allocator_.Allocate(sizeof(Pass));
		ASSERT(pass_block.ptr && "Out of memory for Render Passes");
		Pass* pass = new(pass_block.ptr) Pass(std::forward<decltype(exec)>(exec));
		//All a bit hacky to get around using virtual functions.
		pass->size = pass_block.length;
		//TODO: Check alignment issues here. If PassData has alignment requirements > int, then could be trouble.
		setup(pass->user_data);

	}

	inline void Execute()
	{
		//For each render pass, need to call execute function.
		auto buffer = allocator_.Begin();
		while (buffer != allocator_.End()) {
			auto pass = reinterpret_cast<RenderPassHeader*>(buffer);
			pass->Execute();
			buffer += pass->size;
		}
	}


private:
	//std::vector<RenderPass> render_passes_;
	//Need something like a vector, but each render pass can be of a different size.
	GrowingLinearAllocator<MEGA(16)> allocator_;

};

}
}