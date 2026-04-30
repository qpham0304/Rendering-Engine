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

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
            
    void create();

    // Initializing the allocator and querying the raytracing properties
    void setup(RenderDeviceVulkan* renderDeviceRef, BufferManagerVulkan* bufferManagerRef);

    // Destroying all allocations
    void destroy();

    // Returning the constructed top-level acceleration structure
    VkAccelerationStructureKHR getAccelerationStructure() const;

    // Return the Acceleration Structure Device Address of a BLAS Id
    VkDeviceAddress getBlasDeviceAddress(uint32_t blasId);

    // Create all the BLAS from the vector of BlasInput
    void buildBlas(const std::vector<BlasInput>&        input,
                   VkBuildAccelerationStructureFlagsKHR flags
                       = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    // Refit BLAS number blasIdx from updated buffer contents.
    void updateBlas(uint32_t blasIdx, BlasInput& blas, VkBuildAccelerationStructureFlagsKHR flags);

    // Build TLAS from an array of VkAccelerationStructureInstanceKHR
    // - Use motion=true with VkAccelerationStructureMotionInstanceNV
    // - The resulting TLAS will be stored in m_tlas
    // - update is to rebuild the Tlas with updated matrices, flag must have the 'allow_update'
    void buildTlas(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
                   VkBuildAccelerationStructureFlagsKHR flags
                       = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                   bool                                 update = false,
                   bool                                 motion = false);

    // Creating the TLAS, called by buildTlas
    void cmdCreateTlas(VkCommandBuffer                      cmdBuf,          // Command buffer
                       uint32_t                             countInstance,   // number of instances
                       VkDeviceAddress                      instBufferAddr,  // Buffer address of instances
                       VkBuildAccelerationStructureFlagsKHR flags,           // Build creation flag
                       bool                                 update,          // Update == animation
                       bool                                 motion           // Motion Blur
                       );


protected:
    std::vector<AccelStructureBufferVulkan*> m_blas;  // Bottom-level acceleration structure
    AccelStructureBufferVulkan*              m_tlas;  // Top-level acceleration structure
    

    struct BuildAccelerationStructure
    {
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo
            {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo
            {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
        AccelStructureBufferVulkan* as;  // result acceleration structure
        VkAccelerationStructureKHR cleanupAS;
    };


    void cmdCreateBlas(VkCommandBuffer                          cmdBuf,
                       std::vector<uint32_t>                    indices,
                       std::vector<BuildAccelerationStructure>& buildAs,
                       VkDeviceAddress                          scratchAddress,
                       VkQueryPool                              queryPool);
    void cmdCompactBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
                        std::vector<BuildAccelerationStructure>& buildAs, VkQueryPool queryPool);
    void destroyNonCompacted(std::vector<uint32_t> indices,
                             std::vector<BuildAccelerationStructure>& buildAs);
    bool hasFlag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }
};