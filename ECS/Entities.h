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

	//TODO:
	//Also include a transform, other common data.

};

struct Component
{
	EntityID entity_id;
};

Entity* CreateEntity();
Entity* GetEntity(EntityID id);
void DestroyEntity(EntityID id);

template<typename T>
class ComponentContainer
{
private:
	static_assert(std::is_base_of<Component, T>::value || std::is_base_of<Entity, T>::value, "Invalid type for container.");

	HashIndex hash_index_;
	std::vector<T> data_;
public:
	inline T* Get(EntityID id)
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

	inline T* Create(EntityID id)
	{
		data_.emplace_back();
		T* result = &data_.back();
		result->entity_id = id;
		hash_index_.Add(id, data_.size() - 1);
		return result;
	}

	//NOTE: Could make the create function a templated function, 
	//allowing us to pass arguments
	//to construct our components.

	inline void Remove(EntityID id)
	{
		uint32_t num_components = data_.size();
		for (auto i = hash_index_.First(id);
			 i != HashIndex::INVALID_INDEX && i < num_components;
			 i = hash_index_.Next(i)) {
			if (data_[i].entity_id == id) {
				//Remove this entry by swapping it with the last one in the data_ array.
				data_[i] = data_.back();
				//Need to update the appropriate slot in the hash_index.
				//How to do this?
				auto other_index = data_.size() - 1;
				auto other_id = data_[i].entity_id;
				hash_index_.Remove(other_id, other_index);
				hash_index_.Remove(id, i);
				hash_index_.Add(other_id, i);
				data_.pop_back();

				return;
			}
		}

	}


	inline auto Begin()
	{
		return data_.begin();
	}

	inline auto End()
	{
		return data_.end();
	}
private:

};

} //end namespace ecs;
} //end namespace rkg;