#pragma once

#include "Engine/EngineCoreAPI.h"
#include <cstdint>
#include <vector>

enum class EventType;

class MYGRAPHICSENGINE_EXPORT EventSubscription
{
public:
	EventSubscription() = default;
	~EventSubscription();

	EventSubscription(EventSubscription&& other) noexcept;
	EventSubscription& operator=(EventSubscription&& other) noexcept;
	EventSubscription(const EventSubscription&) = delete;
	EventSubscription& operator=(const EventSubscription&) = delete;

	void reset();
	bool isActive() const { return m_id != 0; }
	uint32_t getID() const { return m_id; }
	EventType getEventType() const { return m_eventType; }

private:
	friend class EventManager;
	EventSubscription(EventType eventType, uint32_t id) : m_eventType(eventType), m_id(id) {}

	EventType m_eventType {};
	uint32_t m_id { 0 };
};

