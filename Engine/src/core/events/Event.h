#pragma once

#include <string>
#include "core/features/Serializable.h"
#include "core/entities/Entity.h"
#include "core/components/MComponent.h"
#include "core/features/Object.h"

enum class EventType
{
	None = 0,
	WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved, WindowUpdate,
	KeyPressed, KeyCombined, KeyReleased, KeyTyped,
	MousePressed, MouseReleased, MouseMoved, MouseScrolled,
	AsyncEvent, ModelLoadEvent, AnimationLoadEvent,
	GuiMessageEvent, GuiFocusedEvent,
	CameraUpdateEvent,
	ContactBegin, ContactEnd, ContactHit
};

class Event : public Object
{
public:
	Event() { setFootprint("Event", sizeof(Event), MemorySystem::Core); }

	bool Handled = false;

	virtual ~Event() = default;

	virtual EventType GetEventType() const = 0;
    virtual const char* GetName() const = 0;
    virtual std::string ToString() const { return GetName(); }

};

class KeyPressedEvent : public Event
{
public:
    int keyCode = 0;
    bool isRepeat = false;

    KeyPressedEvent(int keyCode, bool isRepeat = false) : keyCode(keyCode), isRepeat(isRepeat) { setFootprint("KeyPressedEvent", sizeof(KeyPressedEvent), MemorySystem::Core); }

    EventType GetEventType() const override { return EventType::KeyPressed; }
    const char* GetName() const override { return "KeyPressedEvent"; }
    
    DEFINE_TYPE(KeyPressedEvent,
        member("keyCode", &KeyPressedEvent::keyCode),
        member("isRepeat", &KeyPressedEvent::isRepeat)
    );
};

class MouseMoveEvent : public Event
{
public:
	double m_x = 0;
	double m_y = 0;

	MouseMoveEvent(double x, double y) : m_x(x), m_y(y) { setFootprint("MouseMoveEvent", sizeof(MouseMoveEvent), MemorySystem::Core); }

	EventType GetEventType() const override { return EventType::MouseMoved; }
	const char* GetName() const override { return "MouseMoveEvent"; };
	
	DEFINE_TYPE(MouseMoveEvent,
        member("m_x", &MouseMoveEvent::m_x),
        member("m_y", &MouseMoveEvent::m_y)
    );
};

class MouseScrollEvent : public Event
{
public:
	double m_x = 0;
	double m_y = 0;

	MouseScrollEvent(double x, double y) : m_x(x), m_y(y){ setFootprint("MouseScrollEvent", sizeof(MouseScrollEvent), MemorySystem::Core); }

	EventType GetEventType() const override { return EventType::MouseScrolled; }
	const char* GetName() const override { return "MouseScrollEvent"; };
	
	DEFINE_TYPE(MouseScrollEvent,
        member("m_x", &MouseScrollEvent::m_x),
        member("m_y", &MouseScrollEvent::m_y)
    );
};

class KeyCombinedEvent : public Event
{
public:
	std::vector<int> keyCodes;

	KeyCombinedEvent(std::vector<int> keys) : keyCodes(keys) { setFootprint("KeyCombinedEvent", sizeof(KeyCombinedEvent), MemorySystem::Core); }

	EventType GetEventType() const override { return EventType::KeyCombined; }
	const char* GetName() const override { return "KeyCombinedEvent"; };
	
	DEFINE_TYPE(KeyCombinedEvent,
        member("keyCodes", &KeyCombinedEvent::keyCodes)
    );
};

class WindowCloseEvent : public Event
{
public:
	WindowCloseEvent() { setFootprint("WindowCloseEvent", sizeof(WindowCloseEvent), MemorySystem::Core); };

	EventType GetEventType() const override { return EventType::WindowClose; }
	const char* GetName() const override { return "WindowCloseEvent"; };
	
};

class WindowResizeEvent : public Event
{
public:
	int m_width;
	int m_height;

	WindowResizeEvent(int width, int height) : m_width(width), m_height(height){ setFootprint("WindowResizeEvent", sizeof(WindowResizeEvent), MemorySystem::Core); }

	EventType GetEventType() const override { return EventType::WindowResize; }
	const char* GetName() const override { return "WindowResizeEvent"; };

};

class AsyncEvent : public Event
{
public:
	bool isCompleted;
	std::string id;
	
	AsyncEvent() : isCompleted(false), id("")
	{
		setFootprint("AsyncEvent", sizeof(AsyncEvent), MemorySystem::Core);

	};

	AsyncEvent(const std::string id) : isCompleted(false), id(id)
	{
		setFootprint("AsyncEvent", sizeof(AsyncEvent), MemorySystem::Core);

	};

	virtual EventType GetEventType() const override { return EventType::AsyncEvent; }
	virtual const char* GetName() const override { return "AsyncEvent"; };

};

class ModelLoadAsyncEvent : public AsyncEvent
{
public:
	std::string path = "None";

	ModelLoadAsyncEvent() { setFootprint("ModelLoadAsyncEvent", sizeof(ModelLoadAsyncEvent), MemorySystem::Core); };
	ModelLoadAsyncEvent(const std::string path) : path(path)
	{
		setFootprint("ModelLoadAsyncEvent", sizeof(ModelLoadAsyncEvent), MemorySystem::Core);

	};

	EventType GetEventType() const override { return EventType::AsyncEvent; }
	const char* GetName() const override { return "ModelLoadAsyncEvent"; };
	
};

class ModelLoadEvent : public Event
{
public:
	std::string path = "None";
	Entity entity;

	ModelLoadEvent() { setFootprint("ModelLoadEvent", sizeof(ModelLoadEvent), MemorySystem::Core); };
	ModelLoadEvent(const std::string path, const Entity entity) : path(path), entity(entity)
	{
		setFootprint("ModelLoadEvent", sizeof(ModelLoadEvent), MemorySystem::Core);

	};

	EventType GetEventType() const override { return EventType::ModelLoadEvent; }
	const char* GetName() const override { return "ModelLoadEvent"; };
	
};

class AnimationLoadEvent : public Event
{
public:
	std::string path = "None";
	Entity entity = {};

	AnimationLoadEvent() { setFootprint("AnimationLoadEvent", sizeof(AnimationLoadEvent), MemorySystem::Core); };
	AnimationLoadEvent(const std::string path, const Entity entity) : path(path), entity(entity)
	{
		setFootprint("AnimationLoadEvent", sizeof(AnimationLoadEvent), MemorySystem::Core);

	};

	EventType GetEventType() const override { return EventType::AnimationLoadEvent; }
	const char* GetName() const override { return "AnimationLoadEvent"; };
	
};

class GuiMessageEvent : public Event
{
public:
	std::string message = "None";

	GuiMessageEvent() { setFootprint("GuiMessageEvent", sizeof(GuiMessageEvent), MemorySystem::Core); };
	GuiMessageEvent(std::string_view msg) : message(msg)
	{
		setFootprint("GuiMessageEvent", sizeof(GuiMessageEvent), MemorySystem::Core);

	};

	EventType GetEventType() const override { return EventType::GuiMessageEvent; }
	const char* GetName() const override { return "GuiMessageEvent"; };
	
};

class GuiFocusEvent : public Event
{
public:
	bool isFocused = false;

	GuiFocusEvent() { setFootprint("GuiFocusEvent", sizeof(GuiFocusEvent), MemorySystem::Core); };
	GuiFocusEvent(bool focus) : isFocused(focus)
	{
		setFootprint("GuiFocusEvent", sizeof(GuiFocusEvent), MemorySystem::Core);

	};

	EventType GetEventType() const override { return EventType::GuiFocusedEvent; }
	const char* GetName() const override { return "GuiFocusEvent"; };
	
};

class CameraUpdateEvent : public Event
{
public:
	Entity entity;

	CameraUpdateEvent() { setFootprint("CameraUpdateEvent", sizeof(CameraUpdateEvent), MemorySystem::Core); };
	CameraUpdateEvent(Entity entt) : entity(entt) { setFootprint("CameraUpdateEvent", sizeof(CameraUpdateEvent), MemorySystem::Core); };

	EventType GetEventType() const override { return EventType::CameraUpdateEvent; }
	const char* GetName() const override { return "CameraUpdateEvent"; };

	DEFINE_TYPE(CameraUpdateEvent,
		member("entity", &CameraUpdateEvent::entity)
	);
};

// published after each physics step, an entity id is 0 when the shape has no owning entity
class ContactBeginEvent : public Event
{
public:
	uint32_t entityA = 0;
	uint32_t entityB = 0;

	ContactBeginEvent(uint32_t a, uint32_t b) : entityA(a), entityB(b) { setFootprint("ContactBeginEvent", sizeof(ContactBeginEvent), MemorySystem::Core); }

	EventType GetEventType() const override { return EventType::ContactBegin; }
	const char* GetName() const override { return "ContactBeginEvent"; };

	DEFINE_TYPE(ContactBeginEvent,
		member("entityA", &ContactBeginEvent::entityA),
		member("entityB", &ContactBeginEvent::entityB)
	);
};

class ContactEndEvent : public Event
{
public:
	uint32_t entityA = 0;
	uint32_t entityB = 0;

	ContactEndEvent(uint32_t a, uint32_t b) : entityA(a), entityB(b) { setFootprint("ContactEndEvent", sizeof(ContactEndEvent), MemorySystem::Core); }

	EventType GetEventType() const override { return EventType::ContactEnd; }
	const char* GetName() const override { return "ContactEndEvent"; };

	DEFINE_TYPE(ContactEndEvent,
		member("entityA", &ContactEndEvent::entityA),
		member("entityB", &ContactEndEvent::entityB)
	);
};

// shapes that collide faster than the world hit threshold
class ContactHitEvent : public Event
{
public:
	uint32_t entityA = 0;
	uint32_t entityB = 0;
	glm::vec3 point { 0.0f };
	glm::vec3 normal { 0.0f };		// from A to B
	float approachSpeed = 0.0f;

	ContactHitEvent(uint32_t a, uint32_t b, const glm::vec3& point, const glm::vec3& normal, float approachSpeed)
		: entityA(a), entityB(b), point(point), normal(normal), approachSpeed(approachSpeed)
	{
		setFootprint("ContactHitEvent", sizeof(ContactHitEvent), MemorySystem::Core);
	}

	EventType GetEventType() const override { return EventType::ContactHit; }
	const char* GetName() const override { return "ContactHitEvent"; };

	DEFINE_TYPE(ContactHitEvent,
		member("entityA", &ContactHitEvent::entityA),
		member("entityB", &ContactHitEvent::entityB),
		member("point", &ContactHitEvent::point),
		member("normal", &ContactHitEvent::normal),
		member("approachSpeed", &ContactHitEvent::approachSpeed)
	);
};