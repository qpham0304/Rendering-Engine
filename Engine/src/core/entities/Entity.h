#pragma once
#include "entt.hpp"

class Entity
{
private:
	entt::entity entity;
	entt::registry* registry;

public:
	Entity() = default;
	Entity(const entt::entity& entity, entt::registry& registry);
	Entity(const Entity& other) = default;
	~Entity() = default;
	bool operator==(const Entity& other) const;
	bool operator!=(const Entity& other) const;
	operator entt::entity();

	template<typename T, typename ...Args>
	T& addComponent(Args&& ...args) {
		T& component = registry->emplace<T>(entity, std::forward<Args>(args)...);
		return component;
	}

	template<typename... T>
	bool hasComponent() const {
		return registry->all_of<T...>(entity);
	}

	template<typename T>
	bool removeComponent() {
		if (hasComponent<T>()) {
			T& component = registry->remove<T>(entity);
			return true;
		}
		return false;
	}

	template<typename T>
	T& getComponent() const {
		if (hasComponent<T>()) {
			return registry->get<T>(entity);
		}
		throw std::runtime_error("Component does not exist");
	}

	template<typename ...T>
	std::tuple<T&...> getComponents() const {
		if (registry->all_of<T...>(entity)) {
			return registry->get<T...>(entity);
		}
		throw std::runtime_error("Component does not exist");
	}

	uint32_t getID() const;

	void onCameraComponentAdded();
	void onModelComponentAdded();
	void onMeshComponentAdded();
};
