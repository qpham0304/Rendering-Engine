#include "EventManager.h"
#include <algorithm>

namespace {
	bool s_eventManagerAlive = false;
}

EventManager::EventManager() : Manager("EventManager")
{
	s_eventManagerAlive = true;
}

EventManager::~EventManager()
{
	s_eventManagerAlive = false;
}

EventManager& EventManager::getInstance()
{
	static EventManager instance;
	return instance;
}

bool EventManager::isAlive()
{
	return s_eventManagerAlive;
}

EventSubscription EventManager::subscribe(EventType eventType, EventCallback callback)
{
	uint32_t id = callbackID.fetch_add(1, std::memory_order_relaxed);
	Listener listener { id, std::move(callback) };

	if (publishDepth > 0) {
		pendingListeners.emplace_back(eventType, std::move(listener));	// joins after the current publish
	}
	else {
		callbacks[eventType].push_back(std::move(listener));
	}
	return EventSubscription(eventType, id);
}

void EventManager::unsubscribe(EventType eventType, uint32_t id)
{
	for (auto& [type, listener] : pendingListeners) {
		if (listener.id == id) {
			listener.removed = true;
			hasRemovedListeners = true;
			return;
		}
	}

	auto it = callbacks.find(eventType);
	if (it == callbacks.end()) {
		return;
	}

	auto& listeners = it->second;
	auto listenerIt = std::find_if(listeners.begin(), listeners.end(), [id](const Listener& listener) {
		return listener.id == id;
	});
	if (listenerIt == listeners.end()) {
		return;
	}

	if (publishDepth > 0) {
		listenerIt->removed = true;
		hasRemovedListeners = true;
		return;
	}

	listeners.erase(listenerIt);
	if (listeners.empty()) {
		callbacks.erase(it);
	}
}

void EventManager::publish(Event& event)
{
	auto it = callbacks.find(event.GetEventType());
	if (it == callbacks.end()) {
		return;
	}

	publishDepth++;

	// while publishDepth > 0 the vector is never resized, changes wait in _flushPendingChanges
	for (Listener& listener : it->second | std::views::reverse) {
		if (listener.removed) {
			continue;
		}
		listener.callback(event);
		if (event.Handled) {
			break;
		}
	}

	publishDepth--;
	if (publishDepth == 0) {
		_flushPendingChanges();
	}
}

void EventManager::_flushPendingChanges()
{
	for (auto& [eventType, listener] : pendingListeners) {
		if (!listener.removed) {
			callbacks[eventType].push_back(std::move(listener));
		}
	}
	pendingListeners.clear();

	if (!hasRemovedListeners) {
		return;
	}
	hasRemovedListeners = false;

	for (auto it = callbacks.begin(); it != callbacks.end();) {
		std::erase_if(it->second, [](const Listener& listener) { return listener.removed; });
		it = it->second.empty() ? callbacks.erase(it) : std::next(it);
	}
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

bool EventManager::init(WindowConfig config)
{
	Service::init(config);

	return true;
}

//TODO: use semaphore instead for running tasks instead of manual primitive
//instead of joining threads per completed task, just keep them alive then clean all up on close
//tldr: just use async instead
void EventManager::onUpdate(const FrameTime& frameTime)
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

bool EventManager::onClose()
{
	cleanUpThread();

	return true;
}