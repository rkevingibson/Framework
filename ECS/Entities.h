#pragma once
#include <vector>

#include "Utilities/Utilities.h"
#include "Utilities/HashIndex.h"


namespace rkg
{
namespace ecs
{

using EntityID = uint32_t;

struct Entity
{
	EntityID entity_id;

	//Also include a transform, other common data.

};

struct Component
{
	EntityID entity_id;
};

Entity* CreateEntity();
Entity* GetEntity(EntityID id);


template<typename T>
class ComponentContainer
{
private:
	static_assert(std::is_base_of<Component, T> || std::is_base_of<Entity, T>, "Invalid type for container.");

	HashIndex hash_index_;
	std::vector<T> data_;
public:
	T* Get(EntityID id)
	{
		uint32_t num_components = data_.size();
		for (auto i = hash_index_.First(id); 
			 i != HashIndex::INVALID_INDEX && i < num_components; 
			 i = hash_index_.Next(i)) {
			if (data_[i].entity_id == id) {
				return &data_[i];
			}
		}

		return nullptr;
	}

	T* Create(EntityID id)
	{

	}

	//NOTE: Could make the create function a templated function, 
	//allowing us to pass arguments
	//to construct our components.
private:

};

} //end namespace ecs;
} //end namespace rkg;