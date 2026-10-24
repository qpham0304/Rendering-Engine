#include "MemoryTracker.h"
#include <new>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>

MemoryTracker::~MemoryTracker()
{
}

// never destroyed: statics that outlive it may still free tracked memory during shutdown.
// the exit dump runs after every static constructed later than the tracker is gone,
// so anything still listed under live entries at that point is a leak
MemoryTracker& MemoryTracker::getInstance()
{
	static MemoryTracker* instance = [] {
		MemoryTracker* tracker = new MemoryTracker();
#if MEMORY_TRACKING_ENABLED
		std::atexit([] {
			MemoryTracker::getInstance().dumpJson(EXIT_DUMP_PATH);
		});
#endif
		return tracker;
	}();
	return *instance;
}

const char* MemoryTracker::systemName(MemorySystem system)
{
	switch (system) {
		case MemorySystem::General:		return "General";
		case MemorySystem::Core:		return "Core";
		case MemorySystem::Window:		return "Window";
		case MemorySystem::Logging:		return "Logging";
		case MemorySystem::Resource:	return "Resource";
		case MemorySystem::Graphics:	return "Graphics";
		case MemorySystem::Scene:		return "Scene";
		case MemorySystem::Animation:	return "Animation";
		case MemorySystem::Physics:		return "Physics";
		case MemorySystem::Particle:	return "Particle";
		case MemorySystem::Scripting:	return "Scripting";
		case MemorySystem::Gui:			return "Gui";
		default:						return "Unknown";
	}
}

void MemoryTracker::setAllocator(MemorySystem system, Allocator* allocator)
{
	allocators[static_cast<size_t>(system)] = allocator;
}

Allocator* MemoryTracker::getAllocator(MemorySystem system) const
{
	return allocators[static_cast<size_t>(system)];
}

#if MEMORY_TRACKING_ENABLED
void* MemoryTracker::allocate(size_t size, MemorySystem system, const char* name, size_t alignment)
#else
void* MemoryTracker::allocate(size_t size, MemorySystem system, size_t alignment)
#endif
{
	Allocator* allocator = getAllocator(system);
	void* ptr = allocator
		? allocator->allocate(size, alignment)
		: ::operator new(size, std::align_val_t{ alignment });

#if MEMORY_TRACKING_ENABLED
	track(ptr, size, system, name);
#endif
	return ptr;
}

void MemoryTracker::deallocate(void* ptr, size_t size, MemorySystem system, size_t alignment)
{
	if (!ptr) {
		return;
	}

#if MEMORY_TRACKING_ENABLED
	untrack(ptr);
#endif

	if (Allocator* allocator = getAllocator(system)) {
		allocator->deallocate(ptr, size, alignment);
	}
	else {
		::operator delete(ptr, size, std::align_val_t{ alignment });
	}
}

#if MEMORY_TRACKING_ENABLED
namespace {
	using FootprintMap = std::unordered_map<const void*, MemoryTracker::Footprint>;

	void collect(const FootprintMap& map, std::vector<MemoryTracker::Footprint>& result)
	{
		result.reserve(map.size());
		for (const auto& [ptr, footprint] : map) {
			result.push_back(footprint);
		}
	}

	nlohmann::ordered_json toJson(const MemoryTracker::SystemStats& stats)
	{
		return {
			{ "allocatedBytes", stats.allocatedBytes },
			{ "peakAllocatedBytes", stats.peakAllocatedBytes },
			{ "allocationCount", stats.allocationCount },
			{ "objectBytes", stats.objectBytes },
			{ "peakObjectBytes", stats.peakObjectBytes },
			{ "objectCount", stats.objectCount },
		};
	}

	// entries sorted by system then size so the biggest consumers read first
	nlohmann::ordered_json toJson(const FootprintMap& map)
	{
		std::vector<std::pair<const void*, MemoryTracker::Footprint>> entries(map.begin(), map.end());
		std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
			if (a.second.system != b.second.system) {
				return a.second.system < b.second.system;
			}
			return a.second.size > b.second.size;
		});

		nlohmann::ordered_json array = nlohmann::ordered_json::array();
		for (const auto& [ptr, footprint] : entries) {
			array.push_back({
				{ "name", footprint.name },
				{ "system", MemoryTracker::systemName(footprint.system) },
				{ "size", footprint.size },
				{ "address", std::format("{}", ptr) },
			});
		}
		return array;
	}
}

void MemoryTracker::track(const void* ptr, size_t size, MemorySystem system, const char* name)
{
	if (!ptr) {
		return;
	}

	std::lock_guard<std::mutex> lock(footprintsLock);

	Footprint footprint { name ? name : "unnamed", size, system };
	auto [it, inserted] = allocations.try_emplace(ptr, footprint);
	if (!inserted) {
		// address reused without an untrack, drop the stale record first
		SystemStats& stale = stats[static_cast<size_t>(it->second.system)];
		stale.allocatedBytes -= it->second.size;
		stale.allocationCount--;
		it->second = footprint;
	}

	SystemStats& systemStats = stats[static_cast<size_t>(system)];
	systemStats.allocatedBytes += size;
	systemStats.allocationCount++;
	if (systemStats.allocatedBytes > systemStats.peakAllocatedBytes) {
		systemStats.peakAllocatedBytes = systemStats.allocatedBytes;
	}
}

void MemoryTracker::untrack(const void* ptr)
{
	std::lock_guard<std::mutex> lock(footprintsLock);

	auto it = allocations.find(ptr);
	if (it == allocations.end()) {
		return;
	}

	SystemStats& systemStats = stats[static_cast<size_t>(it->second.system)];
	systemStats.allocatedBytes -= it->second.size;
	systemStats.allocationCount--;
	allocations.erase(it);
}

void MemoryTracker::registerObject(const void* object, size_t size, MemorySystem system, const char* name)
{
	std::lock_guard<std::mutex> lock(footprintsLock);

	Footprint footprint { name ? name : "unnamed", size, system };
	auto [it, inserted] = objects.try_emplace(object, footprint);
	if (!inserted) {
		SystemStats& stale = stats[static_cast<size_t>(it->second.system)];
		stale.objectBytes -= it->second.size;
		stale.objectCount--;
		it->second = footprint;
	}

	SystemStats& systemStats = stats[static_cast<size_t>(system)];
	systemStats.objectBytes += size;
	systemStats.objectCount++;
	if (systemStats.objectBytes > systemStats.peakObjectBytes) {
		systemStats.peakObjectBytes = systemStats.objectBytes;
	}
}

void MemoryTracker::unregisterObject(const void* object)
{
	std::lock_guard<std::mutex> lock(footprintsLock);

	auto it = objects.find(object);
	if (it == objects.end()) {
		return;
	}

	SystemStats& systemStats = stats[static_cast<size_t>(it->second.system)];
	systemStats.objectBytes -= it->second.size;
	systemStats.objectCount--;
	objects.erase(it);
}

MemoryTracker::SystemStats MemoryTracker::getStats(MemorySystem system) const
{
	std::lock_guard<std::mutex> lock(footprintsLock);
	return stats[static_cast<size_t>(system)];
}

size_t MemoryTracker::getTotalAllocatedBytes() const
{
	std::lock_guard<std::mutex> lock(footprintsLock);

	size_t total = 0;
	for (const SystemStats& systemStats : stats) {
		total += systemStats.allocatedBytes;
	}
	return total;
}

std::vector<MemoryTracker::Footprint> MemoryTracker::getAllocations() const
{
	std::lock_guard<std::mutex> lock(footprintsLock);

	std::vector<Footprint> result;
	collect(allocations, result);
	return result;
}

std::vector<MemoryTracker::Footprint> MemoryTracker::getObjects() const
{
	std::lock_guard<std::mutex> lock(footprintsLock);

	std::vector<Footprint> result;
	collect(objects, result);
	return result;
}

void MemoryTracker::reportUsage() const
{
	std::lock_guard<std::mutex> lock(footprintsLock);

	std::printf("[MemoryTracker] usage per system (bytes)\n");
	std::printf("  %-10s %12s %12s %8s %12s %12s %8s\n", "system", "allocated", "peak", "allocs", "objects", "peak", "count");
	for (size_t i = 0; i < stats.size(); i++) {
		const SystemStats& systemStats = stats[i];
		std::printf("  %-10s %12zu %12zu %8zu %12zu %12zu %8zu\n",
			systemName(static_cast<MemorySystem>(i)),
			systemStats.allocatedBytes, systemStats.peakAllocatedBytes, systemStats.allocationCount,
			systemStats.objectBytes, systemStats.peakObjectBytes, systemStats.objectCount);
	}
}

// written straight to a file since the logger may already be gone during static destruction
bool MemoryTracker::dumpJson(const std::string& path) const
{
	std::lock_guard<std::mutex> lock(footprintsLock);

	nlohmann::ordered_json json;
	nlohmann::ordered_json& systems = json["systems"];
	SystemStats total {};
	for (size_t i = 0; i < stats.size(); i++) {
		const SystemStats& systemStats = stats[i];
		systems[systemName(static_cast<MemorySystem>(i))] = toJson(systemStats);

		total.allocatedBytes += systemStats.allocatedBytes;
		total.peakAllocatedBytes += systemStats.peakAllocatedBytes;
		total.allocationCount += systemStats.allocationCount;
		total.objectBytes += systemStats.objectBytes;
		total.peakObjectBytes += systemStats.peakObjectBytes;
		total.objectCount += systemStats.objectCount;
	}
	json["total"] = toJson(total);
	json["liveAllocations"] = toJson(allocations);
	json["liveObjects"] = toJson(objects);

	std::ofstream file(path);
	if (!file) {
		return false;
	}
	file << json.dump(4);
	return true;
}
#endif
