#include "VulkanRayTrace.h"
#include "../renderers/RenderDeviceVulkan.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include <graphics/framework/Vulkan/resources/buffers/AccelStructureBufferVulkan.h>

void RaytracingBuilderKHR::setup(RenderDeviceVulkan *renderDeviceRef, BufferManagerVulkan *bufferManagerRef)
{
	RenderDevice::DeviceInfo deviceInfo = renderDeviceRef->getDeviceInfo();

    renderDevice = renderDeviceRef;
    bufferManager = bufferManagerRef;
    m_device     = &renderDevice->device;
    m_queueIndex = deviceInfo.queueFamilyIndex.value();
}

void RaytracingBuilderKHR::create()
{
    m_rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    m_asProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    prop2.pNext = &m_rtProperties;
    m_rtProperties.pNext = &m_asProperties;
    m_asProperties.pNext = nullptr;

    vkGetPhysicalDeviceProperties2(renderDevice->device.getPhysicalDevice(), &prop2);
}

void RaytracingBuilderKHR::destroy()
{
    destroyBlas();
    destroyTlas();
}

void RaytracingBuilderKHR::destroyBlas()
{
    for(auto& blas : m_blas)  {
        bufferManager->destroy(blas->id());
        blas = nullptr;
    }
    
    m_blas.clear();
}

void RaytracingBuilderKHR::destroyTlas()
{
    bufferManager->destroy(m_tlas->id());
    m_tlas = nullptr;
}

//--------------------------------------------------------------------------------------------------
// Returning the constructed top-level acceleration structure
//
VkAccelerationStructureKHR RaytracingBuilderKHR::getAccelerationStructure() const
{
    return m_tlas->getAccelStr();
}

//--------------------------------------------------------------------------------------------------
// Return the device address of a BLAS previously created.
//
VkDeviceAddress RaytracingBuilderKHR::getBlasDeviceAddress(uint32_t blasId)
{
    assert(size_t(blasId) < m_blas.size());
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.accelerationStructure = m_blas[blasId]->getAccelStr();
    // printf("vkGetAccelerationStructureDeviceAddressKHR\n");
    return vkGetAccelerationStructureDeviceAddressKHR(*m_device, &addressInfo);
}

//--------------------------------------------------------------------------------------------------
// Create all the BLAS from the vector of BlasInput
// - There will be one BLAS per input-vector entry
// - There will be as many BLAS as input.size()
// - The resulting BLAS (along with the inputs used to build) are stored in m_blas,
//   and can be referenced by index.
// - if flag has the 'Compact' flag, the BLAS will be compacted
void RaytracingBuilderKHR::buildBlas(const std::vector<BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags)
{
    auto         nbBlas = static_cast<uint32_t>(input.size());
    VkDeviceSize asTotalSize{0};     // Memory size of all allocated BLAS
    uint32_t     nbCompactions{0};   // Nb of BLAS requesting compaction
    VkDeviceSize maxScratchSize{0};  // Largest scratch size

    // Preparing the information for the acceleration build commands.
    std::vector<BuildAccelerationStructure> buildAs(nbBlas);
    for(uint32_t idx = 0; idx < nbBlas; idx++) {
        printf("    For BlasInput %d (of %d).\n", idx, nbBlas);
        // Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
        // Other information will be filled in the createBlas (see #2)
        buildAs[idx].buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildAs[idx].buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildAs[idx].buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildAs[idx].buildInfo.flags = input[idx].flags | flags;
        buildAs[idx].buildInfo.geometryCount = static_cast<uint32_t>(input[idx].asGeometry.size());
        buildAs[idx].buildInfo.pGeometries = input[idx].asGeometry.data();
        
        buildAs[idx].sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        buildAs[idx].sizeInfo.pNext = nullptr;
        
        // Build range information
        buildAs[idx].rangeInfo = input[idx].asBuildOffsetInfo.data();

        // Finding sizes to create acceleration structures and scratch
        std::vector<uint32_t> maxPrimCount(input[idx].asBuildOffsetInfo.size());
        for(auto tt = 0; tt < input[idx].asBuildOffsetInfo.size(); tt++)
            maxPrimCount[tt] = input[idx].asBuildOffsetInfo[tt].primitiveCount; //# of triangles
        printf("      vkGetAccelerationStructureBuildSizesKHR to request needed BLAS size\n");
        vkGetAccelerationStructureBuildSizesKHR(
            *m_device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildAs[idx].buildInfo, 
            maxPrimCount.data(),
            &buildAs[idx].sizeInfo
        );

        // Extra info
        asTotalSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        maxScratchSize = std::max(maxScratchSize, buildAs[idx].sizeInfo.buildScratchSize);
        nbCompactions += _hasFlag(buildAs[idx].buildInfo.flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
    }

    // Allocate the scratch buffers holding the temporary data of the acceleration structure builder
    // printf("Create scratch buffer for blas of max size %d\n", maxScratchSize);
    m_scratch1_ID = bufferManager->createBufferObject(maxScratchSize);
    auto scratch_1 = dynamic_cast<BufferVulkan*>(bufferManager->getBuffer(m_scratch1_ID));
    
    assert(scratch_1 && "failed to create or retrieve scratch buffer for blas");
    VkDeviceAddress scratchAddress = scratch_1->getAddress();

    // Allocate a query pool for storing the needed size for every BLAS compaction.
    VkQueryPool queryPool{VK_NULL_HANDLE};
    if(nbCompactions > 0) { // Is compaction requested?
        assert(nbCompactions == nbBlas);  // Don't allow mix of on/off compaction
        VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qpci.queryCount = nbBlas;
        qpci.queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
        vkCreateQueryPool(*m_device, &qpci, nullptr, &queryPool);
    }

    // Batching creation/compaction of BLAS to allow staying in restricted amount of memory
    std::vector<uint32_t> indices;  // Indices of the BLAS to create
    VkDeviceSize batchSize{0};
    VkDeviceSize batchLimit{512'000'000};  // 256 MB
    for(uint32_t idx = 0; idx < nbBlas; idx++) {
        indices.push_back(idx);
        batchSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        // Over the limit or last BLAS element
        if(batchSize >= batchLimit || idx == nbBlas - 1) {
            VkCommandBuffer cmdBuf = renderDevice->commandPool.beginSingleTimeCommand();
            _cmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
            renderDevice->commandPool.endSingleTimeCommand(cmdBuf);

            if (queryPool) {
                VkCommandBuffer cmdBuf = renderDevice->commandPool.beginSingleTimeCommand();
                _cmdCompactBlas(cmdBuf, indices, buildAs, queryPool);
                renderDevice->commandPool.endSingleTimeCommand(cmdBuf);

                // Destroy the non-compacted version
                _destroyNonCompacted(indices, buildAs);
            }
            // Reset

            batchSize = 0;
            indices.clear();
        }
    }

    // Logging reduction
    if(queryPool) {
        VkDeviceSize compactSize = std::accumulate(buildAs.begin(), buildAs.end(), 0ULL, [](const auto& a, const auto& b) {
            return a + b.sizeInfo.accelerationStructureSize;
        });
    }

    // Keeping all the created acceleration structures
    for(auto& b : buildAs) {
        m_blas.emplace_back(b.as);
    }

    // Clean up
    if (queryPool) {
        printf("  vkDestroyQueryPool\n");
        vkDestroyQueryPool(*m_device, queryPool, nullptr); }
}

AccelStructureBufferVulkan* createAcceleration(
    RenderDeviceVulkan* renderDevice, 
    BufferManagerVulkan* bufferManager,
    VkAccelerationStructureCreateInfoKHR& accelInfo
){
    AccelStructureBufferVulkan* accelWrap;
    uint32_t accelWrapID;

    accelWrapID = bufferManager->createAccelStructureBuffer(accelInfo.size, accelInfo);
    accelWrap = dynamic_cast<AccelStructureBufferVulkan*>(bufferManager->getBuffer(accelWrapID));

    assert(accelWrap && "failed to create or retrieve accaleration buffer");

    return accelWrap;
}

// Creating the bottom level acceleration structure for all indices of `buildAs` vector.
// The array of BuildAccelerationStructure was created in buildBlas and the vector of
// indices limits the number of BLAS to create at once. This limits the amount of
// memory needed when compacting the BLAS.
void RaytracingBuilderKHR::_cmdCreateBlas(
    VkCommandBuffer cmdBuf,
    std::vector<uint32_t> indices,
    std::vector<BuildAccelerationStructure>& buildAs,
    VkDeviceAddress scratchAddress,
    VkQueryPool queryPool
) {
    if(queryPool) {    // For querying the compaction size
        vkResetQueryPool(*m_device, queryPool, 0, static_cast<uint32_t>(indices.size()));
    }
    uint32_t queryCnt{0};

    for(const auto& idx : indices) {
        printf("      For BLAS #%d of %zd\n", idx, indices.size());
        // Actual allocation of buffer and acceleration structure.
        VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        // Will be used to allocate memory.
        createInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;
        buildAs[idx].as = createAcceleration(renderDevice, bufferManager, createInfo);

        // BuildInfo #2 part
        // Setting where the build lands
        buildAs[idx].buildInfo.dstAccelerationStructure  = buildAs[idx].as->getAccelStr();
        // All build use the same scratch buffer
        buildAs[idx].buildInfo.scratchData.deviceAddress = scratchAddress;

        // Building the bottom-level-acceleration-structure
        printf("        vkCmdBuildAccelerationStructuresKHR build BLAS\n");
        vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildAs[idx].buildInfo,
                                            &buildAs[idx].rangeInfo);

        // Since the scratch buffer is reused across builds, we
        // need a barrier to ensure one build is finished before
        // starting the next one.
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        printf("        vkCmdPipelineBarrier\n");
        vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                0, 1, &barrier, 0, nullptr, 0, nullptr);

        if(queryPool) {
            // Add a query to find the 'real' amount of memory needed, use for compaction
            printf("      vkCmdWriteAccelerationStructuresPropertiesKHR\n");
            vkCmdWriteAccelerationStructuresPropertiesKHR(
                cmdBuf, 1,
                &buildAs[idx].buildInfo.dstAccelerationStructure,
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                queryPool, queryCnt++
            );
        }
    }
}

//--------------------------------------------------------------------------------------------------
// Create and replace a new acceleration structure and buffer based on the size retrieved by the
// Query.
void RaytracingBuilderKHR::_cmdCompactBlas(
    VkCommandBuffer cmdBuf,
    std::vector<uint32_t> indices,
    std::vector<BuildAccelerationStructure>& buildAs,
    VkQueryPool queryPool
) {
    printf("  cmdCompactBlas\n");
    uint32_t queryCtn{0};

    // Get the compacted size result back
    std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
    vkGetQueryPoolResults(
        *m_device, queryPool, 0, (uint32_t)compactSizes.size(), compactSizes.size() * sizeof(VkDeviceSize),
        compactSizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT
    );

    for(auto idx : indices) {
        buildAs[idx].cleanupAS = buildAs[idx].as->getAccelStr(); // previous AS to destroy
        buildAs[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCtn++];  // new reduced size

        // Creating a compact version of the AS
        VkAccelerationStructureCreateInfoKHR asCreateInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        asCreateInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;
        asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildAs[idx].as = createAcceleration(renderDevice, bufferManager, asCreateInfo);

        // Copy the original BLAS to a compact version
        VkCopyAccelerationStructureInfoKHR copyInfo{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
        copyInfo.src  = buildAs[idx].buildInfo.dstAccelerationStructure;
        copyInfo.dst  = buildAs[idx].as->getAccelStr();
        copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
        printf("  vkCmdCopyAccelerationStructureKHR for BLAS commodification\n");
        vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
    }
}

//--------------------------------------------------------------------------------------------------
// Destroy all the non-compacted acceleration structures
//
void RaytracingBuilderKHR::_destroyNonCompacted(std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs)
{
    printf("  RaytracingBuilderKHR::destroyNonCompacted\n");
    for(auto& i : indices) {
        vkDestroyAccelerationStructureKHR(*m_device, buildAs[i].cleanupAS, nullptr);
    }
}

//--------------------------------------------------------------------------------------------------
// Low level of Tlas creation 
void RaytracingBuilderKHR::_cmdCreateTlas(
    VkCommandBuffer cmdBuf,
    uint32_t countInstance,
    VkDeviceAddress instBufferAddr,
    VkBuildAccelerationStructureFlagsKHR flags,
    bool update,
    bool motion
) {
    // Wraps a device pointer to the above uploaded instances.
    VkAccelerationStructureGeometryInstancesDataKHR instancesVk{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    instancesVk.data.deviceAddress = instBufferAddr;

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    VkAccelerationStructureGeometryKHR topASGeometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    topASGeometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topASGeometry.geometry.instances = instancesVk;

    // Find sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    buildInfo.flags         = flags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &topASGeometry;
    buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(
        *m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &countInstance, &sizeInfo
    );

    // Create TLAS
    if(m_tlas == nullptr || m_tlas->getAccelStr() == VK_NULL_HANDLE) {
        VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        createInfo.size = sizeInfo.accelerationStructureSize; 
        m_tlas = createAcceleration(renderDevice, bufferManager, createInfo);
    }
    
    // Allocate the scratch memory
    // printf("Create scratch buffer for tlas of max size %d\n", sizeInfo.buildScratchSize);
    if (m_scratch2_ID == 0) {
         m_scratch2_ID = bufferManager->createBufferObject(sizeInfo.buildScratchSize);
    }

    auto scratch_2 = dynamic_cast<BufferVulkan*>(bufferManager->getBuffer(m_scratch2_ID));
    assert(scratch_2 && "failed to create or retrieve scratch buffer for tlas");

    // Update build information
    buildInfo.srcAccelerationStructure  = update ? m_tlas->getAccelStr() : VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure  = m_tlas->getAccelStr();
    buildInfo.scratchData.deviceAddress = scratch_2->getAddress();

    // Build Offsets info: n instances
    VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{countInstance, 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

    // Build the TLAS
    // printf("      vkCmdBuildAccelerationStructuresKHR to build the TLAS\n");
    vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);
}

//--------------------------------------------------------------------------------------------------
// Refit BLAS number blasIdx from updated buffer contents.
void RaytracingBuilderKHR::_updateBlas(uint32_t blasIdx, BlasInput& blas, VkBuildAccelerationStructureFlagsKHR flags)
{
    assert (false && "Not used; Not currently maintained;  Probably leaks a VkDeviceMemory");
    printf("  updateBlas\n");
    assert(size_t(blasIdx) < m_blas.size());

    // Preparing all build information, acceleration is filled later
    VkAccelerationStructureBuildGeometryInfoKHR buildInfos{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    buildInfos.flags = flags;
    buildInfos.geometryCount = (uint32_t)blas.asGeometry.size();
    buildInfos.pGeometries = blas.asGeometry.data();
    buildInfos.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;      // UPDATE
    buildInfos.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfos.srcAccelerationStructure = m_blas[blasIdx]->getAccelStr();   // UPDATE
    buildInfos.dstAccelerationStructure = m_blas[blasIdx]->getAccelStr();

    // Find size to build on the device
    std::vector<uint32_t> maxPrimCount(blas.asBuildOffsetInfo.size());
    for(auto tt = 0; tt < blas.asBuildOffsetInfo.size(); tt++) {
        maxPrimCount[tt] = blas.asBuildOffsetInfo[tt].primitiveCount;  // Number of primitives/triangles
    }
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(
        *m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfos,
        maxPrimCount.data(), &sizeInfo
    );

    // Allocate the scratch buffer and setting the scratch info
    // printf("Create scratch buffer for tlas of max size %d\n", sizeInfo.buildScratchSize);
    uint32_t scratch = bufferManager->createBufferObject(sizeInfo.buildScratchSize);
    auto scratchBuffer = dynamic_cast<BufferVulkan*>(bufferManager->getBuffer(m_scratch2_ID));
    
    assert(scratch && "failed to create or retrieve scratch buffer for updating blas");
    VkDeviceAddress scratchAddress = scratchBuffer->getAddress();

    VkBufferDeviceAddressInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    bufferInfo.buffer = static_cast<VkBuffer>(*scratchBuffer);
    buildInfos.scratchData.deviceAddress = vkGetBufferDeviceAddress(*m_device, &bufferInfo);

    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> pBuildOffset(blas.asBuildOffsetInfo.size());
    for(size_t i = 0; i < blas.asBuildOffsetInfo.size(); i++) {
        pBuildOffset[i] = &blas.asBuildOffsetInfo[i];
    }

    // Update the instance buffer on the device side and build the TLAS
    // Update the acceleration structure. Note the VK_TRUE parameter to trigger the update,
    // and the existing BLAS being passed and updated in place
    VkCommandBuffer cmdBuf = renderDevice->commandPool.beginSingleTimeCommand();
    vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfos, pBuildOffset.data());
    renderDevice->commandPool.endSingleTimeCommand(cmdBuf);
}
    
void RaytracingBuilderKHR::buildTlas(
    const std::vector<VkAccelerationStructureInstanceKHR>& instances,
    VkBuildAccelerationStructureFlagsKHR flags,
    bool update, bool motion
) {
    // *NOTE: Cannot call buildTlas twice except to update
    // this will cause driver halt and crash the gpu
    assert((!update || m_tlas != nullptr) && "Cannot update before creating TLAS");
    uint32_t countInstance = static_cast<uint32_t>(instances.size());

    // Command buffer to create the TLAS
    VkCommandBuffer    cmdBuf = renderDevice->commandPool.beginSingleTimeCommand();

    // Create a buffer holding the actual instance data (matrices++) for use by the AS builder
    // Create a buffer (staged) for the instances
    uint32_t instancesBufferID = bufferManager->createInstanceBuffer(instances.data(), countInstance);
    BufferVulkan* instancesBuffer = bufferManager->getBuffer(instancesBufferID);
    
    
    VkBufferDeviceAddressInfo bufferInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, 
        nullptr,
        static_cast<VkBuffer>(*instancesBuffer)
    };
    
    VkDeviceAddress instBufferAddr = vkGetBufferDeviceAddress(*m_device, &bufferInfo);
    
    // Make sure the copy of the instance buffer are copied before triggering the acceleration structure build
    // vkCmdPipelineBarrier ensuring instance buffer is completely filled in
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    vkCmdPipelineBarrier(
        cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0, 1, &barrier, 0, nullptr, 0, nullptr
    );

    // Creating the TLAS
    _cmdCreateTlas(cmdBuf, countInstance, instBufferAddr, flags, update, motion);

    // Finalizing and destroying temporary data
    renderDevice->commandPool.endSingleTimeCommand(cmdBuf);
    
    bufferManager->destroy(instancesBufferID);
 }
