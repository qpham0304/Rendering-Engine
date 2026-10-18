#include "EventSubscription.h"
#include "EventManager.h"

EventSubscription::~EventSubscription()
{
	reset();
}

EventSubscription::EventSubscription(EventSubscription&& other) noexcept
	: m_eventType(other.m_eventType), m_id(other.m_id)
{
	other.m_id = 0;
}

EventSubscription& EventSubscription::operator=(EventSubscription&& other) noexcept
{
	if (this != &other) {
		reset();
		m_eventType = other.m_eventType;
		m_id = other.m_id;
		other.m_id = 0;
	}
	return *this;
}

void EventSubscription::reset()
{
	if (m_id == 0) {
		return;
	}

	// static subscribers can outlive the EventManager singleton at exit
	if (EventManager::isAlive()) {
		EventManager::getInstance().unsubscribe(m_eventType, m_id);
	}
	m_id = 0;
}
