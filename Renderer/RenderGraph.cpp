#include "RenderGraph.h"

namespace rkg {
namespace render {
RenderGraphResource ResourceManager::CreateBuffer(const BufferCreationInfo & info)
{
    RenderGraphResource handle;
    handle.resource_index = resources.size();
    handle.usage_id = 0;
    resources.emplace_back();
    auto& resource = resources.back();
    resource.first_node_index = resource.last_node_index = current_node;
    //TODO: Definitely need to store other stuff in the resource here.

    usages.emplace_back();
    auto& usage = usages.back();
    usage.usage = UsageEntry::Creation;
    usage.resource_index = handle.resource_index;
    usage.node_index = current_node;
    return handle;
}

RenderGraphResource ResourceManager::ReadBuffer(RenderGraphResource handle)
{
    //TODO: Need to handle case where read/write are in the same buffer. Should handle as if it was a call to UpdateBuffer.
    Expects(handle.resource_index < resources.size());
    handle.usage_id++;
    
    usages.emplace_back();
    auto& usage = usages.back();
    usage.usage = UsageEntry::Read;
    usage.resource_index = handle.resource_index;
    usage.node_index     = current_node;
    return handle;
}

RenderGraphResource ResourceManager::UpdateBuffer(RenderGraphResource handle)
{
    Expects(handle.resource_index < resources.size());
    handle.usage_id++;
    usages.emplace_back();
    auto& usage = usages.back();
    usage.usage = UsageEntry::ReadWrite;
    usage.resource_index = handle.resource_index;
    usage.node_index = current_node;
    return handle;
}

RenderGraphResource ResourceManager::WriteBuffer(RenderGraphResource handle)
{
    Expects(handle.resource_index < resources.size());
    handle.usage_id++;

    usages.emplace_back();
    auto& usage = usages.back();
    usage.usage = UsageEntry::Write;
    usage.resource_index = handle.resource_index;
    usage.node_index = current_node;
    return handle;
}


}//namespace render;
}//namespace rkg

