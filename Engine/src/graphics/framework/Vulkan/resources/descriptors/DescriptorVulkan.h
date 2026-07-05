#include <vector>

class BufferVulkan;
class BufferManagerVulkan;
class DescriptorManagerVulkan;

class DescriptorVulkan {
public:
	DescriptorVulkan();
	~DescriptorVulkan() = default;

	void addBuffer(BufferVulkan* buffer);
	void updateDescriptor();

private:
	uint32_t descriptorLayoutID;
	uint32_t descriptorPoolID;
	uint32_t descriptorSetID;

	std::vector<BufferVulkan*> buffers;
	BufferManagerVulkan* bufferManager{ nullptr };
	DescriptorManagerVulkan* descriptorManager{ nullptr };
};
