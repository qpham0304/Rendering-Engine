#pragma once
#include <cstdint>

class Buffer
{
public:
	virtual ~Buffer() = default;

	virtual void bind(void* commandBuffer = nullptr) = 0;
	
	uint32_t id() { return m_id; }
	void* getHandle() { return m_handle; }

protected:
	Buffer(uint32_t id) : m_id(id) {}

protected:
	uint32_t m_id{ 0 };
	void* m_handle{ nullptr };
};

