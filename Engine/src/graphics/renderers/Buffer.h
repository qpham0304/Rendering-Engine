#pragma once
#include <cstdint>
#include "core/features/Object.h"

class Buffer : public Object
{
public:
	enum class Type {
		None, Staging, Uniform, IndexGPU, IndexDynamic, VertexGPU, VertexDynamic, 
		StorageGPU, StorageDynamic, IndirectGPU, IndirectDynamic,
		BufferDeviceAddressGPU, BufferDeviceAddressDynamic, AcclerationStructure
	};

	virtual ~Buffer() = default;
	virtual void bind(void* commandBuffer = nullptr) = 0;
	virtual void update(const void* data, size_t size) = 0;
	
	Type type() { return m_bufferType; }
	uint64_t getVirtualMemory() { return m_virtualMemory; } 
		
	uint32_t id() { return m_id; }

protected:
	Buffer(uint32_t id = 0) : m_id(id) { setFootprint("Buffer", sizeof(Buffer), MemorySystem::Graphics); }

protected:
	uint32_t m_id{ 0 };
	Type m_bufferType { Type::None };
	uint64_t m_virtualMemory { 0 };
};

