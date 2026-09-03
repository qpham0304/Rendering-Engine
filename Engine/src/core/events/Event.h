#pragma once

#include <string>
#include "core/scene/Serializer.h"

enum class EventType
{
	None = 0,
	WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved, WindowUpdate,
	KeyPressed, KeyCombined, KeyReleased, KeyTyped,
	MousePressed, MouseReleased, MouseMoved, MouseScrolled,
	AsyncEvent, ModelLoadEvent, AnimationLoadEvent,
	GuiMessageEvent, GuiFocusedEvent,
	CameraUpdateEvent
};

class Event
{
public:
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

    KeyPressedEvent(int keyCode, bool isRepeat = false) : keyCode(keyCode), isRepeat(isRepeat) {}

    EventType GetEventType() const override { return EventType::KeyPressed; }
    const char* GetName() const override { return "KeyPressedEvent"; }
    
    REFELECT_TYPE(KeyPressedEvent,
        visitor("keyCode", &KeyPressedEvent::keyCode),
        visitor("isRepeat", &KeyPressedEvent::isRepeat)
    );
};

class MouseMoveEvent : public Event
{
public:
	double m_x = 0;
	double m_y = 0;

	MouseMoveEvent(double x, double y) : m_x(x), m_y(y) {}

	EventType GetEventType() const override { return EventType::MouseMoved; }
	const char* GetName() const override { return "MouseMoveEvent"; };
	
	REFELECT_TYPE(MouseMoveEvent,
        visitor("m_x", &MouseMoveEvent::m_x),
        visitor("m_y", &MouseMoveEvent::m_y)
    );
};

class MouseScrollEvent : public Event
{
public:
	double m_x = 0;
	double m_y = 0;

	MouseScrollEvent(double x, double y) : m_x(x), m_y(y){}

	EventType GetEventType() const override { return EventType::MouseScrolled; }
	const char* GetName() const override { return "MouseScrollEvent"; };
	
	REFELECT_TYPE(MouseScrollEvent,
        visitor("m_x", &MouseScrollEvent::m_x),
        visitor("m_y", &MouseScrollEvent::m_y)
    );
};

class KeyCombinedEvent : public Event
{
public:
	std::vector<int> keyCodes;

	KeyCombinedEvent(std::vector<int> keys) : keyCodes(keys) {}

	EventType GetEventType() const override { return EventType::KeyCombined; }
	const char* GetName() const override { return "KeyCombinedEvent"; };
	
	REFELECT_TYPE(KeyCombinedEvent,
        visitor("keyCodes", &KeyCombinedEvent::keyCodes)
    );
};

class WindowCloseEvent : public Event
{
public:
	WindowCloseEvent() = default;

	EventType GetEventType() const override { return EventType::WindowClose; }
	const char* GetName() const override { return "WindowCloseEvent"; };
	
};

class WindowResizeEvent : public Event
{
public:
	int m_width;
	int m_height;

	WindowResizeEvent(int width, int height) : m_width(width), m_height(height){}

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

	};

	AsyncEvent(const std::string id) : isCompleted(false), id(id)
	{

	};

	virtual EventType GetEventType() const override { return EventType::AsyncEvent; }
	virtual const char* GetName() const override { return "AsyncEvent"; };

};

class ModelLoadAsyncEvent : public AsyncEvent
{
public:
	std::string path = "None";

	ModelLoadAsyncEvent() = default;
	ModelLoadAsyncEvent(const std::string path) : path(path)
	{

	};

	EventType GetEventType() const override { return EventType::AsyncEvent; }
	const char* GetName() const override { return "ModelLoadAsyncEvent"; };
	
};

class ModelLoadEvent : public Event
{
public:
	std::string path = "None";
	Entity entity;

	ModelLoadEvent() = default;
	ModelLoadEvent(const std::string path, const Entity entity) : path(path), entity(entity)
	{

	};

	EventType GetEventType() const override { return EventType::ModelLoadEvent; }
	const char* GetName() const override { return "ModelLoadEvent"; };
	
};

class AnimationLoadEvent : public Event
{
public:
	std::string path = "None";
	Entity entity = {};

	AnimationLoadEvent() = default;
	AnimationLoadEvent(const std::string path, const Entity entity) : path(path), entity(entity)
	{

	};

	EventType GetEventType() const override { return EventType::AnimationLoadEvent; }
	const char* GetName() const override { return "AnimationLoadEvent"; };
	
};

class GuiMessageEvent : public Event
{
public:
	std::string message = "None";

	GuiMessageEvent() = default;
	GuiMessageEvent(std::string_view msg) : message(msg)
	{

	};

	EventType GetEventType() const override { return EventType::GuiMessageEvent; }
	const char* GetName() const override { return "GuiMessageEvent"; };
	
};

class GuiFocusEvent : public Event
{
public:
	bool isFocused = false;

	GuiFocusEvent() = default;
	GuiFocusEvent(bool focus) : isFocused(focus)
	{

	};

	EventType GetEventType() const override { return EventType::GuiFocusedEvent; }
	const char* GetName() const override { return "GuiFocusEvent"; };
	
};

class CameraUpdateEvent : public Event
{
public:
	Entity entity;

	CameraUpdateEvent() = default;
	CameraUpdateEvent(Entity entt) : entity(entt) {};

	EventType GetEventType() const override { return EventType::CameraUpdateEvent; }
	const char* GetName() const override { return "CameraUpdateEvent"; };

	REFELECT_TYPE(CameraUpdateEvent,
		visitor("entity", &CameraUpdateEvent::entity);
	);
};