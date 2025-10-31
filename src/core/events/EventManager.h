#pragma once

#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <future>
#include <thread>
#include "Event.h"
#include "EventListener.h"
#include "../../src/core/features/Timer.h"

class EventManager
{
public:
	using EventCallback = std::function<void(Event&)>;
	using AsyncCallback = std::function<void(AsyncEvent&)>;

	~EventManager() = default;
	
	static EventManager& getInstance();
	
	void Subscribe(const std::string& event, EventListener& listener);

	template<typename... Args>
	void Publish(const std::string& event, Args... args) {
		if (listeners.find(event) != listeners.end()) {
			std::vector<EventListener>& events = listeners[event];
			for (EventListener& e : events) {
				e.onEvent(args...);
			}
		}
		else {
			Console::error("Event not found\n");
		}
	}

	uint32_t Subscribe(EventType eventType, EventCallback callback);
	void Unsubscribe(EventType eventType, uint32_t cbID);
	void Publish(Event& event);
	void Queue(AsyncEvent event, AsyncCallback callback);
	void OnUpdate();
	std::vector<std::pair<std::thread, bool*>> threads;


private:
	EventManager() = default;
	void PublishAsync(EventListener& eventListener);
	void CleanUpThread();
	int runningTasks = 0;

	uint32_t callbackID;
	std::unordered_map<std::string, std::vector<EventListener>> listeners;
	std::unordered_map<EventType, std::vector<std::pair<uint32_t, EventCallback>>> callbacks;

	std::queue<std::pair<AsyncEvent, AsyncCallback>> eventQueue;
	std::mutex queueMutex;
	std::vector<std::future<void>> futures;
};