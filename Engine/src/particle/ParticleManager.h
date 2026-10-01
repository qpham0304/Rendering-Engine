#include "core/resources/managers/Manager.h"
#include "ParticleContainer.h"

class ParticleManager : public Manager
{
public:
	struct ContainerData {
		uint32_t lifetimeBufferID { 0 };
		uint32_t positionsBufferID { 0 };
		uint32_t scalesBufferID { 0 };
		uint32_t velocitiesBufferID { 0 };
		uint32_t colorsBufferID { 0 };
	};

	ParticleManager();	
	virtual ~ParticleManager();

	virtual bool init(WindowConfig config) override;
    virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
    virtual void onUpdate() override;

	const ContainerData& getContainerData(uint32_t id) const;
	const std::vector<ContainerData>& getAllContainerData() const;
	ParticleContainer getContainer(uint32_t id);
	uint32_t createContainer(uint32_t size, glm::vec3 minSpacing, glm::vec3 maxSpacing);

private:

    const uint32_t MAX_CONTAINERS = 100;
	std::vector<ParticleContainer> m_containers { {} };
	std::vector<ContainerData> m_containerData { {} };
	std::vector<uint32_t> m_toBeDestroyed;

};