#pragma once

#include "MemoryTracker.h"

// base for anything that should show up in the tracker. in debug each instance carries its footprint
// and registers itself for its lifetime; in release the class is empty so it costs nothing as a base.
// derived classes overwrite the default footprint from their constructor with setFootprint
class MYGRAPHICSENGINE_EXPORT MemoryTrackable
{
public:
#if MEMORY_TRACKING_ENABLED
	const char* getMemoryName() const { return m_footprint.name; }
	size_t getMemorySize() const { return m_footprint.size; }
	MemorySystem getMemorySystem() const { return m_footprint.system; }
#endif

protected:
#if MEMORY_TRACKING_ENABLED
	MemoryTrackable() noexcept
	{
		MemoryTracker::getInstance().registerObject(this, m_footprint.size, m_footprint.system, m_footprint.name);
	}

	MemoryTrackable(const MemoryTrackable& other) noexcept
		: m_footprint(other.m_footprint)
	{
		MemoryTracker::getInstance().registerObject(this, m_footprint.size, m_footprint.system, m_footprint.name);
	}

	~MemoryTrackable() noexcept
	{
		MemoryTracker::getInstance().unregisterObject(this);
	}

	// size should be the most derived type, sizeof(Derived)
	void setFootprint(const char* name, size_t size, MemorySystem system)
	{
		m_footprint = { name, size, system };
		MemoryTracker::getInstance().registerObject(this, size, system, name);
	}
#else
	MemoryTrackable() = default;
	MemoryTrackable(const MemoryTrackable&) = default;
	~MemoryTrackable() = default;

	void setFootprint(const char*, size_t, MemorySystem) {}
#endif

	// an instance keeps its own footprint, assignment only copies the derived state
	MemoryTrackable& operator=(const MemoryTrackable&) { return *this; }

private:
#if MEMORY_TRACKING_ENABLED
	MemoryTracker::Footprint m_footprint;
#endif
};
