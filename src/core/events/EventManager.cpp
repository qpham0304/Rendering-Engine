#include "EventManager.h"

EventManager& EventManager::getInstance()
{
	static EventManager instance;
	return instance;
}

uint32_t EventManager::subscribe(EventType eventType, EventCallback callback)
{
	uint32_t id = callbackID.fetch_add(1, std::memory_order_relaxed);
	auto& callbackList = callbacks[eventType];

	callbackList.emplace_back(id, std::move(callback));
	return id;
}

//TODO: check what happen if an event is removed mid iteration? 
// also what about synchronization?
void EventManager::unsubscribe(EventType eventType, uint32_t cbID)
{
	auto& vector = callbacks[eventType];
	int index = 0;
	for (auto& [id, callback] : vector) {
		if (id == cbID) {
			vector.erase(vector.begin() + index);
		}
		index++;
	}

	if (vector.empty()) {
		callbacks.erase(eventType);
	}
}

void EventManager::publish(Event& event)
{
	if (callbacks.find(event.GetEventType()) != callbacks.end()) {
		for (const auto& [id, callback] : callbacks[event.GetEventType()]) {
			callback(event);
			if (event.Handled) {
				break;
			}
		}
	}
}

void EventManager::publishAsync(EventListener& eventListener)
{
	std::scoped_lock<std::mutex> lock(queueMutex);
	eventListener.onEvent();
}

void EventManager::cleanUpThread()
{
	int counter = 0;

	for (auto& [thread, status] : threads) {
		if (status != nullptr && *status) {
			counter++;
			Console::println("...threads joined...");
			thread.join();
		}
	}
	if (!threads.empty() && counter == threads.size()) {
		threads.clear();
		Console::println("All event threads are cleaned up");
	}
}

void EventManager::queue(AsyncEvent event, AsyncCallback callback)
{
	std::scoped_lock<std::mutex> lock(queueMutex);
	eventQueue.push(std::make_pair(std::move(event), std::move(callback)));
	runningTasks.fetch_add(1, std::memory_order_relaxed);
}

void EventManager::subscribe(const std::string& event, EventListener& listener) {
	if (listeners.find(event) != listeners.end()) {
		listeners[event].emplace_back(std::move(listener));
	}
	else {
		listeners[event] = { std::move(listener) };
	}
}

//TODO: use semaphore instead for running tasks instead of manual primitive
//instead of joining threads per completed task, just keep them alive then clean all up on close
//tldr: just use async instead
void EventManager::onUpdate()
{
	Timer timer("thread queue", true);

	while (!eventQueue.empty()) {
		auto& [event, callback] = eventQueue.front();
		
		Console::println("num Tasks: ", runningTasks);
		if (runningTasks <= 5) {
			AsyncEvent mutableEvent(event);
			std::thread thread = std::thread([this, &event, callback, mutableEvent]() mutable {
				callback(mutableEvent);
				event.isCompleted = true;
				runningTasks.fetch_sub(1, std::memory_order_relaxed);
			});
			threads.push_back(std::make_pair(std::move(thread), &event.isCompleted));
			eventQueue.pop();
		}
	}

	if (eventQueue.empty()) {
		cleanUpThread();
	}
}

void EventManager::onClose()
{
	cleanUpThread();
}