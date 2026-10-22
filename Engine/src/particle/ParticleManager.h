#pragma once

#include "core/resources/managers/Manager.h"
#include "ParticleContainer.h"

class BufferManager;

class MYGRAPHICSENGINE_EXPORT ParticleManager : public Manager
{
public:
	struct ContainerData {
		uint32_t particlesBufferID { 0 };	// ParticleGPU[capacity]
		uint32_t aliveListBufferID { 0 };	// ParticleDrawEntry[particleSortSize(capacity)], rebuilt by the simulate pass
		uint32_t deadListBufferID { 0 };	// stack of free slots
		uint32_t countersBufferID { 0 };	// ParticleCounters
	};

	ParticleManager();
	virtual ~ParticleManager();

	virtual bool init(WindowConfig config) override;
    virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
    virtual void onUpdate(const FrameTime& frameTime) override;

	const ContainerData& getContainerData(uint32_t id) const;
	const std::vector<ContainerData>& getAllContainerData() const;
	ParticleContainer getContainer(uint32_t id);
	uint32_t createContainer(uint32_t capacity);

	// both rewrite buffers the gpu may be reading, the caller has to make sure the device is idle
	void resizeContainer(uint32_t id, uint32_t capacity);
	void resetContainer(uint32_t id);

	// bumped whenever a container's buffers are created or replaced so renderers can refresh their address tables
	uint32_t getVersion() const { return m_version; }

private:
	void _allocateBuffers(ContainerData& data, uint32_t capacity);
	void _releaseBuffers(ContainerData& data);
	void _uploadInitialState(const ContainerData& data, uint32_t capacity);

    const uint32_t MAX_CONTAINERS = 100;
	std::vector<ParticleContainer> m_containers { {} };
	std::vector<ContainerData> m_containerData { {} };
	std::vector<uint32_t> m_toBeDestroyed;
	BufferManager* m_bufferManager { nullptr };
	uint32_t m_version { 0 };

};
