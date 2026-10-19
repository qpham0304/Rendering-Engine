#pragma once

#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <future>
#include <thread>
#include <atomic>
#include "Event.h"
#include "EventListener.h"
#include "EventSubscription.h"
#include "core/features/Timer.h"
#include "core/resources/managers/Manager.h"

class MYGRAPHICSENGINE_EXPORT EventManager : public Manager
{
public:
	using EventCallback = std::function<void(Event&)>;
	using AsyncCallback = std::function<void(AsyncEvent&)>;

	~EventManager();
	
	static EventManager& getInstance();
	static bool isAlive();
	
	[[nodiscard]] EventSubscription subscribe(EventType eventType, EventCallback callback);
	void publish(Event& event);
	void queue(AsyncEvent event, AsyncCallback callback);
	
	bool init(WindowConfig config) override;
	bool onClose() override;
	void onUpdate(const FrameTime& frameTime) override;

private:
	friend class EventSubscription;

	struct Listener {
		uint32_t id;
		EventCallback callback;
		bool removed { false };
	};

	EventManager();
	void unsubscribe(EventType eventType, uint32_t id);
	void _flushPendingChanges();
	void cleanUpThread();

	virtual std::vector<uint32_t> listIDs() const { return {}; };
	virtual void destroy(uint32_t id) override { };

private:
	std::atomic<int> runningTasks{ 0 };
	std::atomic<uint32_t> callbackID{ 1 };
	std::unordered_map<std::string, std::vector<EventListener>> listeners;
	std::unordered_map<EventType, std::vector<Listener>> callbacks;


	int publishDepth { 0 };	// publish inside another publish, unsubscribe register to be removed but only removed at depth = 0
	bool hasRemovedListeners { false };
	std::vector<std::pair<EventType, Listener>> pendingListeners;

	std::queue<std::pair<AsyncEvent, AsyncCallback>> eventQueue;
	std::mutex queueMutex;
	std::vector<std::future<void>> futures;
	std::vector<std::pair<std::thread, bool*>> threads;
};