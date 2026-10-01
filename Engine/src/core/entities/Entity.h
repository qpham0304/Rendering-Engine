#pragma once
#include "entt.hpp"
#include <format>
#include <stdexcept>
#include <typeinfo>

class Entity
{
public:
	Entity() = default;
	Entity(const entt::entity& entity, entt::registry& registry);
	Entity(const Entity& other) = default;
	~Entity() = default;
	bool operator==(const Entity& other) const;
	bool operator!=(const Entity& other) const;
	operator entt::entity();
	operator uint32_t();

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
			registry->remove<T>(entity);
			return true;
		}
		return false;
	}

	template<typename T>
	T& getComponent() const {
		if (hasComponent<T>()) {
			return registry->get<T>(entity);
		}
		std::string msg = std::format("entity getComponent(): Component: {} does not exist", typeid(T).name());
		throw std::runtime_error(msg);
	}

	template<typename ...T>
	std::tuple<T&...> getComponents() const {
		if (registry->all_of<T...>(entity)) {
			return registry->get<T...>(entity);
		}
		std::string msg = std::format("entity getComponents(): Component: {} does not exist", typeid(T).name());
		throw std::runtime_error(msg);
	}

	uint32_t getID() const;

	entt::registry* getRegistry();

	void onModelComponentAdded();
	void onMeshComponentAdded();
	void onSpriteComponentAdded();
	void onAnimationComponentAdded();
	void onAnimationStateComponentAdded();
	void onScriptComponentAdded();
	void onColliderComponentAdded();
	void onCameraComponentAdded();
	void onParticleEmitterAdded();
	
private:
	entt::entity entity;
	entt::registry* registry;
};
