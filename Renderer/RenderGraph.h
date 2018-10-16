#pragma once
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <Utilities/Allocators.h>

namespace rkg {
namespace render {

using LambdaAllocator =
    FallbackAllocator<CollectionOfStacksAllocator<Mallocator, KILO(2), 4>, Mallocator>;
using UserDataAllocator = Mallocator;

using RenderGraphResource = uint64_t;

struct BufferCreationInfo
{
    enum UsageFlagBits {
        TransferSrcBit        = 0x01,
        TransferDstBit        = 0x02,
        UniformTexelBufferBit = 0x04,
        StorageTexelBufferBit = 0x08,
        UniformBufferBit      = 0x10,
        StorageBufferBit      = 0x20,
        IndexBufferBit        = 0x40,
        IndirectBufferBit     = 0x80,
    };
    size_t size;
    uint32_t usage;

};

struct ResourceManager {
	RenderGraphResource CreateBuffer(const BufferCreationInfo& info);
    RenderGraphResource ReadBuffer(RenderGraphResource);
    RenderGraphResource WriteBuffer(RenderGraphResource);

    //Should not be called outside of the rendergraph. Maybe should be friend function?
    inline void NewNode() {
        current_node++;
    }
private:
    struct Resource {
        RenderGraphResource handle;

    };

    struct UsageEntry {
        enum {
            Read = 0x1,
            Write = 0x2,
            Creation = 0x4,
        };
        size_t node_index;
        size_t resource_index;
        uint8_t usage {0};
    };
    std::vector<Resource> resources;
    std::vector<UsageEntry> usages;

    size_t current_node {0};
};

struct RenderGraph;

struct RenderGraphNode {
    using ExecutionFn = void (*)(const ResourceManager&, void* execute_fn_buffer, void* user_data);
    ExecutionFn                   fn;
    RenderGraphNode*              parent;
    std::vector<RenderGraphNode*> children;
    std::string                   name;
    MemoryBlock                   user_data_block;
    MemoryBlock                   executor_block;
};

struct RenderGraph {
    std::vector<RenderGraphNode> nodes;
    ResourceManager              resource_manager;
    LambdaAllocator              executor_allocator;
    UserDataAllocator            user_data_allocator;

    // TODO: Nit. Don't like this way of doing things. Would like to be able to execute the nodes
    // directly, via node->execute(). Preferably no arguments needed, though maybe passing the graph
    // is a good idea. Alternatively, the nodes can contain a pointer to the graph, but that's somewhat circular and has risks with it.

    inline void ExecuteNode(RenderGraphNode* node)
    {
        node->fn(resource_manager, node->executor_block.ptr, node->user_data_block.ptr);
    };
};



struct RenderGraphFactory {
    // TODO: More manual memory management once we're up and running.

    std::unique_ptr<RenderGraph> graph{std::make_unique<RenderGraph>()};
    bool                         compiled{false};

    template <typename T, typename Setup, typename Executor>
    inline void AddNode(const char* name, Setup&& setup_fn, Executor&& execute_fn)
    {
        static_assert(std::is_trivially_copyable_v<T>, "User data type must be trivially copyable");
        static_assert(
            alignof(T) <= UserDataAllocator::ALIGNMENT,
            "User data allocator needs larger alignment for this type");
        static_assert(sizeof(Executor) <= KILO(1), "Executor function is too large.");

        auto execute_fn_wrapper =
            [](const ResourceManager& resource_manager, void* functor, void* user_data) {
                auto fn = reinterpret_cast<Executor*>(functor);
                (*fn)(resource_manager, *reinterpret_cast<T*>(user_data));
            };

        graph->nodes.emplace_back();
        RenderGraphNode& node = graph->nodes.back();
        node.fn               = execute_fn_wrapper;
        MemoryBlock b         = graph->executor_allocator.Allocate(sizeof(Executor));
        new (b.ptr) Executor(std::forward<Executor>(execute_fn));
        node.executor_block  = b;
        node.name            = name;
        node.user_data_block = graph->user_data_allocator.Allocate(sizeof(T));
        new (node.user_data_block.ptr) T;

        // Run Setup function
        graph->resource_manager.NewNode();
        setup_fn(graph->resource_manager, *reinterpret_cast<T*>(node.user_data_block.ptr));
    };
    inline std::unique_ptr<RenderGraph> Compile()
    {
        // TODO: Figure out dependencies;

        Expects(compiled == false);
        compiled = true;
        return std::move(graph);
    }
};

}  // namespace render
}  // namespace rkg
