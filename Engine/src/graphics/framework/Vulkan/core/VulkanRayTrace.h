#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vector>

class VulkanDevice;
class RenderDeviceVulkan;
class BufferManagerVulkan;
class AccelStructureBufferVulkan;


struct BlasInput
{
    // Data used to build acceleration structure geometry
    std::vector<VkAccelerationStructureGeometryKHR>       asGeometry;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
    VkBuildAccelerationStructureFlagsKHR                  flags{0};
};

class RaytracingBuilderKHR
{
public:
    RenderDeviceVulkan* renderDevice;
    BufferManagerVulkan* bufferManager;
    VulkanDevice* m_device;
    uint32_t m_queueIndex{0};
    uint32_t m_scratch1_ID{0};
    uint32_t m_scratch2_ID{0};
    AccelStructureBufferVulkan* m_scratch1;
    AccelStructureBufferVulkan* m_scratch2;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties{};
        
    // Initializing the allocator and querying the raytracing properties
    void setup(RenderDeviceVulkan* renderDeviceRef, BufferManagerVulkan* bufferManagerRef);
    void create();
    void destroy();
    void destroyBlas();
    void destroyTlas();

    // Returning the constructed top-level acceleration structure
    VkAccelerationStructureKHR getAccelerationStructure() const;

    // Return the Acceleration Structure Device Address of a BLAS Id
    VkDeviceAddress getBlasDeviceAddress(uint32_t blasId);

    // Create all the BLAS from the vector of BlasInput
    void buildBlas(
        const std::vector<BlasInput>& input,
        VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
    );

    // Build TLAS from an array of VkAccelerationStructureInstanceKHR
    // - Use motion=true with VkAccelerationStructureMotionInstanceNV
    // - The resulting TLAS will be stored in m_tlas
    // - update is to rebuild the Tlas with updated matrices, flag must have the 'allow_update'
    void buildTlas(
        const std::vector<VkAccelerationStructureInstanceKHR>& instances,
        VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        bool update = false,
        bool motion = false
    );

protected:
    std::vector<AccelStructureBufferVulkan*> m_blas;  // Bottom-level acceleration structure
    AccelStructureBufferVulkan*              m_tlas;  // Top-level acceleration structure

    struct BuildAccelerationStructure
    {
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo {};
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo {};
        const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
        AccelStructureBufferVulkan* as;  // result acceleration structure
        VkAccelerationStructureKHR cleanupAS;
    };


    void _cmdCreateBlas(
        VkCommandBuffer cmdBuf,
        std::vector<uint32_t> indices,
        std::vector<BuildAccelerationStructure>& buildAs,
        VkDeviceAddress scratchAddress,
        VkQueryPool queryPool
    );
                       
    // Creating the TLAS, called by buildTlas
    void _cmdCreateTlas(
        VkCommandBuffer cmdBuf,                         // Command buffer
        uint32_t countInstance,                         // number of instances
        VkDeviceAddress instBufferAddr,                 // Buffer address of instances
        VkBuildAccelerationStructureFlagsKHR flags,     // Build creation flag
        bool update,                                    // Update == animation
        bool motion                                     // Motion Blur
    );

    void _cmdCompactBlas(
        VkCommandBuffer cmdBuf,
        std::vector<uint32_t> indices,
        std::vector<BuildAccelerationStructure>& buildAs,
        VkQueryPool queryPool
    );
    
    void _destroyNonCompacted(
        std::vector<uint32_t> indices,
        std::vector<BuildAccelerationStructure>& buildAs
    );

    bool _hasFlag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }

    
    // Refit BLAS number blasIdx from updated buffer contents.
    // not currently maintained so set it as private
    void _updateBlas(uint32_t blasIdx, BlasInput& blas, VkBuildAccelerationStructureFlagsKHR flags);

};