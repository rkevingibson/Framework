#pragma once
#include <type_traits>
#include <vector>


namespace rkg {
namespace render {

struct RenderGraphNode {
	using ExecutionFn = void (*)(const RenderGraph& graph);
	ExecutionFn                   fn;
	RenderGraphNode*              parent;
	std::vector<RenderGraphNode*> children;
};

struct RenderGraph {
	std::vector<RenderGraphNode> nodes;
};

struct RenderGraphFactory {
	template <typename T, typename Compiler, typename Executor>
	inline void AddNode(const char* name, Compiler&& compile_fn, Executor&& execute_fn){
		static_assert(std::is_trivially_copyable_v<T>, "User data type must be trivially copyable");


	};
	RenderGraph* Compile();
};

struct ResourceManager {
};


}  // namespace render
}  // namespace rkg
