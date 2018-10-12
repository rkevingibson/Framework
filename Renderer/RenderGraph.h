#pragma once
#include <memory>
#include <string>
#include <type_traits>
#include <vector>


namespace rkg {
namespace render {

struct ResourceManager {
};

struct RenderGraph;
struct RenderGraphNode {
    using ExecutionFn = void (*)(const ResourceManager&, void* execute_fn_buffer, void* user_data);
    ExecutionFn                   fn;
    RenderGraphNode*              parent;
    std::vector<RenderGraphNode*> children;
    const char*                   name;
    std::vector<char>             user_data_buffer;
    std::vector<char>             execute_fn_buffer;
};



struct RenderGraph {
    std::vector<RenderGraphNode> nodes;
    ResourceManager              resource_manager;

    // TODO: Nit. Don't like this way of doing things. Would like to be able to execute the nodes
    // directly, via node->execute(). Preferably no arguments needed, though maybe passing the graph
    // is a good idea.
    inline void ExecuteNode(RenderGraphNode* node)
    {
        node->fn(resource_manager, node->execute_fn_buffer.data(), node->user_data_buffer.data());
    };
};



struct RenderGraphFactory {
    // TODO: More manual memory management once we're up and running.
    struct NodeInternal {
        std::string       name;
        std::vector<char> compile_fn_buffer;
        std::vector<char> execute_fn_buffer;
        std::vector<char> user_data_struct_buffer;
        using TypeErasedFn =
            void (*)(void* functor, void* user_data, ResourceManager& resource_manager);
        using ExecutionFn =
            void (*)(const ResourceManager&, void* execute_fn_buffer, void* user_data);
        TypeErasedFn compile_fn;
        ExecutionFn  execute_fn;
    };

    std::vector<NodeInternal> nodes;

    template <typename T, typename Compiler, typename Executor>
    inline void AddNode(const char* name, Compiler&& compile_fn, Executor&& execute_fn)
    {
        static_assert(std::is_trivially_copyable_v<T>, "User data type must be trivially copyable");
        auto compile_fn_wrapper =
            [](void* functor, void* user_data, ResourceManager& resource_manager) {
                auto fn = reinterpret_cast<Compiler*>(functor);
                (*fn)(resource_manager, *reinterpret_cast<T*>(user_data));
            };

        auto execute_fn_wrapper =
            [](const ResourceManager& resource_manager, void* functor, void* user_data) {
                auto fn = reinterpret_cast<Executor*>(functor);
                (*fn)(resource_manager, *reinterpret_cast<T*>(user_data));
            };

        nodes.emplace_back();
        NodeInternal& node = nodes.back();
        node.compile_fn    = compile_fn_wrapper;
        node.execute_fn    = execute_fn_wrapper;
        node.compile_fn_buffer.resize(sizeof(Compiler));
        new (node.compile_fn_buffer.data()) Compiler(std::forward<Compiler>(compile_fn));
        node.execute_fn_buffer.resize(sizeof(Executor));
        new (node.execute_fn_buffer.data()) Executor(std::forward<Executor>(execute_fn));
        node.name = name;
        node.user_data_struct_buffer.resize(sizeof(T));
        new (node.user_data_struct_buffer.data()) T;  // Construct the object in-place;
    };
    inline std::unique_ptr<RenderGraph> Compile()
    {
        auto result = std::make_unique<RenderGraph>();
        /*
        TODO:
            -dependency resolution
        */
        for (auto& internal_node : nodes) {
            internal_node.compile_fn(
                internal_node.compile_fn_buffer.data(),
                internal_node.user_data_struct_buffer.data(),
                result->resource_manager);

            result->nodes.emplace_back();
            auto& result_node = result->nodes.back();
            result_node.fn    = internal_node.execute_fn;

            // TODO: This will break if the factory goes out of scope. Definitely need proper
            // allocation.
            result_node.execute_fn_buffer = std::move(internal_node.execute_fn_buffer);

            result_node.user_data_buffer = std::move(internal_node.user_data_struct_buffer);
            result_node.name             = internal_node.name.c_str();
        }

        return result;
    }
};

}  // namespace render
}  // namespace rkg
