#pragma once

#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <future>
#include <thread>
#include "Event.h"
#include <atomic>
#include "EventListener.h"
#include "src/core/features/Timer.h"

class EventManager
{
public:
	using EventCallback = std::function<void(Event&)>;
	using AsyncCallback = std::function<void(AsyncEvent&)>;

	~EventManager() = default;
	
	static EventManager& getInstance();
	

	template<typename... Args>
	void publish(const std::string& event, Args... args) {
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
	void subscribe(const std::string& event, EventListener& listener);
	uint32_t subscribe(EventType eventType, EventCallback callback);
	void unsubscribe(EventType eventType, uint32_t cbID);
	void publish(Event& event);
	void queue(AsyncEvent event, AsyncCallback callback);
	void onUpdate();
	void onClose();

private:
	EventManager() = default;
	void publishAsync(EventListener& eventListener);
	void cleanUpThread();

private:
	std::atomic<int> runningTasks{ 0 };

	std::atomic<uint32_t> callbackID{ 0 };
	std::unordered_map<std::string, std::vector<EventListener>> listeners;
	std::unordered_map<EventType, std::vector<std::pair<uint32_t, EventCallback>>> callbacks;

	std::queue<std::pair<AsyncEvent, AsyncCallback>> eventQueue;
	std::mutex queueMutex;
	std::vector<std::future<void>> futures;
	std::vector<std::pair<std::thread, bool*>> threads;
};