#include "Entities.h"

#include <atomic>

namespace
{
using namespace rkg::ecs;
ComponentContainer<Entity> entities;

std::atomic_uint64_t next_id;
}

namespace rkg
{
namespace ecs
{

Entity* CreateEntity()
{
	EntityID id = next_id++;
	Entity* result = entities.Create(id);
	result->entity_id = id;
	return result;
}

Entity* GetEntity(EntityID id)
{
	return entities.Get(id);
}

void DestroyEntity(EntityID id)
{
	entities.Remove(id);
}

}
}