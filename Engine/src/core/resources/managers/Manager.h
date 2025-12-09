#pragma once

#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <string>

class Manager
{
public:
	~Manager() = default;

	virtual void init() = 0;
	virtual void shutdown() = 0;
	virtual void destroy(uint32_t id) = 0;

protected:
	Manager() = default;

	const uint32_t& _assignID() { return m_ids.fetch_add(1, std::memory_order_relaxed); }

	typedef std::shared_lock<std::shared_mutex> ReadLock;
	typedef std::unique_lock<std::shared_mutex> WriteLock;

	ReadLock _lockRead() { return ReadLock(m_resourceLock); }
	WriteLock _lockWrite() { return WriteLock(m_resourceLock); }

protected:
	std::atomic<uint32_t> m_ids;
	std::shared_mutex m_resourceLock;

};

