#pragma once

#include "Engine/EngineCoreAPI.h"
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <mutex>
#include <string>
#include <unordered_map>

// debug records a footprint (name, size, system) for every tracked object and allocation.
// release drops all of that: MEMORY_ALLOC / MEMORY_FREE still allocate through the system's
// allocator hook (or the global heap), they just stop recording anything
#ifndef NDEBUG
	#define MEMORY_TRACKING_ENABLED 1
#else
	#define MEMORY_TRACKING_ENABLED 0
#endif

enum class MemorySystem : uint8_t
{
	General,
	Core,
	Window,
	Logging,
	Resource,
	Graphics,
	Scene,
	Animation,
	Physics,
	Particle,
	Scripting,
	Gui,
	Count,
};

// hook for custom allocators (pool, linear, ...), set per system on the tracker.
// systems without one fall back to the global heap
class MYGRAPHICSENGINE_EXPORT Allocator
{
public:
	virtual ~Allocator() = default;

	virtual void* allocate(size_t size, size_t alignment) = 0;
	virtual void deallocate(void* ptr, size_t size, size_t alignment) = 0;
};

class MYGRAPHICSENGINE_EXPORT MemoryTracker
{
public:
	static constexpr size_t DEFAULT_ALIGNMENT = alignof(std::max_align_t);
	static constexpr const char* EXIT_DUMP_PATH = "memory_stats.json";

#if MEMORY_TRACKING_ENABLED
	struct Footprint {
		const char* name{ "unnamed" };	// expected to outlive the record, string literals
		size_t size{ 0 };
		MemorySystem system{ MemorySystem::General };
	};

	struct SystemStats {
		size_t allocatedBytes { 0 };
		size_t peakAllocatedBytes { 0 };
		size_t allocationCount { 0 };
		size_t objectBytes { 0 };
		size_t peakObjectBytes { 0 };
		size_t objectCount { 0 };
	};
#endif

public:
	~MemoryTracker();

	static MemoryTracker& getInstance();
	static const char* systemName(MemorySystem system);

	void setAllocator(MemorySystem system, Allocator* allocator);
	Allocator* getAllocator(MemorySystem system) const;

	// prefer MEMORY_ALLOC / MEMORY_FREE so the name argument compiles out in release
#if MEMORY_TRACKING_ENABLED
	void* allocate(size_t size, MemorySystem system, const char* name, size_t alignment = DEFAULT_ALIGNMENT);
#else
	void* allocate(size_t size, MemorySystem system, size_t alignment = DEFAULT_ALIGNMENT);
#endif
	void deallocate(void* ptr, size_t size, MemorySystem system, size_t alignment = DEFAULT_ALIGNMENT);

#if MEMORY_TRACKING_ENABLED
	// record memory owned elsewhere (VMA, third party, new/delete) without routing it through the tracker
	void track(const void* ptr, size_t size, MemorySystem system, const char* name);
	void untrack(const void* ptr);

	// live object footprints, kept apart from raw allocations since an object can sit inside one
	void registerObject(const void* object, size_t size, MemorySystem system, const char* name);
	void unregisterObject(const void* object);

	SystemStats getStats(MemorySystem system) const;
	size_t getTotalAllocatedBytes() const;
	std::vector<Footprint> getAllocations() const;
	std::vector<Footprint> getObjects() const;
	void reportUsage() const;

	// per-system stats plus every live allocation and object, same idea as vma_stats.json.
	// runs automatically at exit, where the live entries are the leaks
	bool dumpJson(const std::string& path = EXIT_DUMP_PATH) const;
#endif

private:
	MemoryTracker() = default;

private:
	std::array<Allocator*, static_cast<size_t>(MemorySystem::Count)> allocators {};

#if MEMORY_TRACKING_ENABLED
	mutable std::mutex footprintsLock;
	std::unordered_map<const void*, Footprint> allocations;
	std::unordered_map<const void*, Footprint> objects;
	std::array<SystemStats, static_cast<size_t>(MemorySystem::Count)> stats {};
#endif
};

#if MEMORY_TRACKING_ENABLED
	#define MEMORY_ALLOC(size, system, name)		MemoryTracker::getInstance().allocate((size), (system), (name))
	#define MEMORY_TRACK(ptr, size, system, name)	MemoryTracker::getInstance().track((ptr), (size), (system), (name))
	#define MEMORY_UNTRACK(ptr)						MemoryTracker::getInstance().untrack((ptr))
#else
	#define MEMORY_ALLOC(size, system, name)		MemoryTracker::getInstance().allocate((size), (system))
	#define MEMORY_TRACK(ptr, size, system, name)	((void)0)
	#define MEMORY_UNTRACK(ptr)						((void)0)
#endif

#define MEMORY_FREE(ptr, size, system)				MemoryTracker::getInstance().deallocate((ptr), (size), (system))
