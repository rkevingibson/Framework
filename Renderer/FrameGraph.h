#pragma once
#include "Utilities/Utilities.h"
#include <vector>

/*
*/
namespace rkg
{

using FrameGraphResource = uint64_t;

class RenderPassBuilder
{
public:
	RenderPassBuilder() = default;

	FrameGraphResource Read(FrameGraphResource input)
	{
		//Add a graph edge between the node of the input resource and this node.
		return input;
	}

	FrameGraphResource& Write(FrameGraphResource target)
	{
		//Add a graph edge between the node of the input resource and this node.

		//Add an outward graph edge between this node and a renamed input resource.

	}
private:

};

class FrameGraph
{
public:
	class RenderPass;

	template<typename PassData, typename SetupFn, typename ExecuteFn>
	void AddCallbackPass<PassData>(const char* pass_name, SetupFn&& setup, ExecuteFn&& exec)
	{
		//Add the pass to some buffer that I can later execute...
		
		//Call the setup function
		RenderPassBuilder builder;
		PassData data;
		setup(builder, data);
	}
	inline void Compile()
	{
		//Walk through the render passes, compute reference counts for resources, compute first and last users for resources.

		//cull unreferenced passes.
	}

	inline void Execute()
	{
		//For each render pass, need to call execute function.
		//
	}


private:
	std::vector<FrameGraphResource> resource_handles_;
	std::vector<RenderPass> render_passes_;


};

}