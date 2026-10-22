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

	// an isolated view of one emitter for editor tools, asked for every frame it should be drawn
	struct PreviewRequest {
		bool enabled { false };
		uint32_t containerID { 0 };
		uint32_t width { 0 };
		uint32_t height { 0 };
		glm::mat4 view { 1.0f };
		glm::mat4 projection { 1.0f };		// renderer needs to flip it
		glm::vec3 cameraPosition { 0.0f };
		glm::vec4 clearColor { 0.07f, 0.07f, 0.08f, 1.0f };
		bool simulate { false };			// emitter still runs on the editor while the game stops
		float timeScale { 1.0f };
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

	// the request lasts one frame, the renderer consumes it and reports the texture it drew into
	void requestPreview(const PreviewRequest& request) { m_previewRequest = request; m_previewRequest.enabled = true; }
	PreviewRequest consumePreview() { PreviewRequest request = m_previewRequest; m_previewRequest.enabled = false; return request; }
	void setPreviewTextureID(uint32_t textureID) { m_previewTextureID = textureID; }
	uint32_t getPreviewTextureID() const { return m_previewTextureID; }

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
	PreviewRequest m_previewRequest {};
	uint32_t m_previewTextureID { 0 };

};
