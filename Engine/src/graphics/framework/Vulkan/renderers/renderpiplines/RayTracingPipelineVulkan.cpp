#include "RaytracingPipelineVulkan.h"
#include "core/features/ServiceLocator.h"
#include "core/events/EventManager.h"
#include "graphics/renderers/RenderDevice.h"
#include "window/AppWindow.h"

#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <gui/GuiManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <core/scene/SceneManager.h>
#include <imgui.h>
#include "graphics/framework/vulkan/renderers/renderpasses/AlchemyAORendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/SSRGIPassVulkan.h"
#include "graphics/framework/Vulkan/resources/buffers/DeviceAddressBufferVulkan.h"

RaytracingPipelineVulkan::RaytracingPipelineVulkan() 
	: RendererVulkan("RaytracingPipelineVulkan")
{

}

RaytracingPipelineVulkan::~RaytracingPipelineVulkan()
{

}

bool RaytracingPipelineVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);
	
	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));
	
	size_t bufferSize = MAX_INSTANCES * sizeof(StorageBufferObject);
	instanceData.resize(MAX_INSTANCES);
	instanceDataPrev.resize(MAX_INSTANCES);
	bufferManagerVulkan->createStorageBuffers(storagebuffersList, bufferSize);
	bufferManagerVulkan->createStorageBuffers(prevStoragebufferList, bufferSize);

	objects.resize(MAX_INSTANCES);
	size_t objectsBufferSize = MAX_INSTANCES * sizeof(ObjectDesc);
	objDeviceAddressBufferID = bufferManagerVulkan->createBufferDeviceAddress(objectsBufferSize);
	auto deviceAddress = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(objDeviceAddressBufferID));
	objDeviceAddress = deviceAddress->getReference();

	lights.reserve(numLights);
	size_t lightBufferSize = numLights * sizeof(LightSSBO);
	bufferManagerVulkan->createStorageBuffers(lightStoragebuffers, lightBufferSize);

	_createResources();

	m_rtBuilder.setup(renderDeviceVulkan, bufferManagerVulkan);
	m_rtBuilder.create();
    m_rtBuilder.destroy();
	_createAccelStructure();
	_createDescriptor();
	_createPipeline();
	_createShaderBindingTable();

	auto materialManagerVulkan = static_cast<MaterialManagerVulkan*>(materialManager);
	materialsAddress = materialManagerVulkan->getMaterialAddress();

	return true;
}

bool RaytracingPipelineVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();
	
	return true;
}

void RaytracingPipelineVulkan::onUpdate()
{

}

void RaytracingPipelineVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);
	
	if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

	instanceDataPrev = std::move(instanceData);
	instanceData.clear(); 
	// objects.clear();
    // lights.clear();

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}

	if (firstFrame) {
        lastViewProj = camera.getProjectionMatrix() * camera.getViewMatrix();
        lastViewProj[1][1] *= -1.0; 
        firstFrame = false;
    }
	ubo.view = camera.getViewMatrix();
	ubo.prevViewProj = lastViewProj;
	ubo.proj = camera.getProjectionMatrix();
	ubo.cameraPos = glm::vec4(camera.getPosition(), 1.0);
	ubo.proj[1][1] *= -1.0;
	ubo.invView = camera.getInViewMatrix();
	ubo.invProj = camera.getInProjectionMatrix();
	ubo.invProj[1][1] *= -1.0;
	ubo.width  = AppWindow::getWidth();
    ubo.height = AppWindow::getHeight();

	
	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	StorageBufferVulkan* ssbo = storagebuffersList[currentFrame];
	ssbo->update(instanceData.data(), instanceData.size() * sizeof(StorageBufferObject));

	StorageBufferVulkan* ssboPrev = prevStoragebufferList[currentFrame];
	ssboPrev->update(instanceDataPrev.data(), instanceDataPrev.size() * sizeof(StorageBufferObject));
	
	size_t buffersize = MAX_INSTANCES * sizeof(ObjectDesc);
	bufferManagerVulkan->updateBufferDeviceAddress(objDeviceAddressBufferID, objects.data(), buffersize);

	StorageBufferVulkan* lightSSBO = lightStoragebuffers[currentFrame];
	lightSSBO->update(lights.data(), lights.size() * sizeof(LightSSBO));
	

	lastViewProj = ubo.proj * ubo.view;

	pushConstant.objectsRef = objDeviceAddress; 
	pushConstant.objectIdx  = 0;//objectsIndex;

	writeRayTracing(cmd, currentFrame);
	// writePostProcess(cmd, currentFrame);

	// rendererManagerVulkan->setDisplayImage(postProcessImage);
	rendererManagerVulkan->setDisplayImage(rayTraceImage);
}

void RaytracingPipelineVulkan::writePostProcess(VkCommandBuffer cmd, uint32_t currentFrame)
{
	TextureManagerVulkan::transitionImageLayout(
		cmd, postProcessImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan
	);
	
    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
    uint32_t groupX = (swapchain.swapChainExtent.width + 15) / 16;
    uint32_t groupY = (swapchain.swapChainExtent.height + 15) / 16;

	postProcessPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
	auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(postProcessSetID);
	vkCmdPushConstants(cmd, postProcessPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstant), &pushConstant);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, postProcessPipeline->pipelineLayout, 0, 1, &descriptorSet[currentFrame], 0, nullptr);
    vkCmdDispatch(cmd, groupX, groupY, 1);

	TextureManagerVulkan::transitionImageLayout(
		cmd, postProcessImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan
	);
}

void RaytracingPipelineVulkan::writeRayTracing(VkCommandBuffer cmd, uint32_t currentFrame)
{
	TextureManagerVulkan::transitionImageLayout(
		cmd, rayTraceImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan
	);

	rtPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	VkDescriptorSet rtSet = descriptorManagerVulkan->getDescriptorSet(raytraceSetID)[currentFrame];
	auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
	// auto rayTraceLayout = descriptorManagerVulkan->getDescriptorLayout(raytraceLayoutID);
	std::vector<VkDescriptorSet> sets = { rtSet, bindlessSet };

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, 
							rtPipeline->pipelineLayout, 0, 
							static_cast<uint32_t>(sets.size()), sets.data(), 
							0, nullptr);

	vkCmdPushConstants(cmd, rtPipeline->pipelineLayout, 
					VK_SHADER_STAGE_RAYGEN_BIT_KHR,// | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 
					0, sizeof(PushConstant), &pushConstant);

	// Trace Command using SBT region info
	vkCmdTraceRaysKHR(cmd, &m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion, ubo.width, ubo.height, 1);

	TextureManagerVulkan::transitionImageLayout(
		cmd, rayTraceImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan
	);
}

#pragma region setup
void RaytracingPipelineVulkan::_createResources() 
{
    auto createTexture = [this] (uint32_t& id, TextureVulkan*& texture){
        id = textureManagerVulkan->createTexture();
        texture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
        
        assert(texture && "failed to cast texture into vulkan texture");
        
        VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
        VkDevice device = renderDeviceVulkan->device;
        
        TextureManagerVulkan::createImage(
            swapchain.swapChainExtent.width,
            swapchain.swapChainExtent.height,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture->textureImage,
            texture->textureImageMemory,
            1,
            renderDeviceVulkan->device
        );

        TextureManagerVulkan::createImageView(
            texture->textureImage,
            texture->textureImageView,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1,
            renderDeviceVulkan->device
        );

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;

        TextureManagerVulkan::createTextureSampler(
            texture->textureSampler, 
            renderDeviceVulkan->device,
            samplerInfo
        );

		VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
		TextureManagerVulkan::transitionImageLayout(
			cmd, texture->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan
		);
		renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);
    };
	
	createTexture(rayTraceImageID, rayTraceImage);
	createTexture(postProcessImageID, postProcessImage);
}

void RaytracingPipelineVulkan::_createPipeline()
{
	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);


	auto rayTraceLayout = descriptorManagerVulkan->getDescriptorLayout(raytraceLayoutID);

	rtPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    rtPipeline->createRayTracePipeline(
		"assets/shaders/spv/raytrace.rgen.spv", 
		{ rayTraceLayout, bindlessLayout }, 
		sizeof(PushConstant)
	);

    VkDescriptorSetLayout postProcess = descriptorManagerVulkan->getDescriptorLayout(postProcessLayoutID);
	postProcessPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	postProcessPipeline->createComputePipeline(
		"assets/shaders/spv/postProcess.comp.spv", 
		{ postProcess }, 
		sizeof(PushConstant)
	);

}

void RaytracingPipelineVulkan::_createDescriptor()
{
	uint32_t frameCount = VulkanUtils::numFrames();

	// ray tracing pipeline
	rtBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
	};
    std::vector<VkDescriptorPoolSize> rtPoolSizes {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
	};
    raytraceLayoutID = descriptorManagerVulkan->createLayout(rtBindings);
    raytracePoolID = descriptorManagerVulkan->createPool(rtPoolSizes, frameCount);
	raytraceSetID = descriptorManagerVulkan->createSets(raytraceLayoutID, raytracePoolID, frameCount);

	// post process pipeline
	std::vector<VkDescriptorSetLayoutBinding> postBindings {
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
    std::vector<VkDescriptorPoolSize> postPoolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
	};
    postProcessLayoutID = descriptorManagerVulkan->createLayout(postBindings);
    postProcessPoolID = descriptorManagerVulkan->createPool(postPoolSizes, frameCount);
	postProcessSetID = descriptorManagerVulkan->createSets(postProcessLayoutID, postProcessPoolID, frameCount);

    for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void RaytracingPipelineVulkan::_updateDescriptor(uint32_t index)
{	
	VkDescriptorImageInfo outputImageInfo{};
	VkDescriptorBufferInfo bufferInfo{};
	VkWriteDescriptorSetAccelerationStructureKHR descASInfo{};

	//ray tracing
	auto rtDescriptorSets = descriptorManagerVulkan->getDescriptorSet(raytraceSetID);
	auto tlas = m_rtBuilder.getAccelerationStructure();

	outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outputImageInfo.imageView = rayTraceImage->textureImageView;
	outputImageInfo.sampler = rayTraceImage->textureSampler;
    
    bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[index]);
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

	
	descASInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descASInfo.accelerationStructureCount = 1;
    descASInfo.pAccelerationStructures    = &tlas;

	std::vector<VkWriteDescriptorSet> writesRayTrace;
	descriptorManagerVulkan->writeAccelStruct(&writesRayTrace, rtDescriptorSets[index], 0, rtBindings, descASInfo);
	descriptorManagerVulkan->writeStorageImage(&writesRayTrace, rtDescriptorSets[index], 1, outputImageInfo);
	descriptorManagerVulkan->writeUniform(&writesRayTrace, rtDescriptorSets[index], 2, bufferInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writesRayTrace);
	
	//post process
	auto postDescriptorSets = descriptorManagerVulkan->getDescriptorSet(postProcessSetID);
	outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outputImageInfo.imageView = postProcessImage->textureImageView;
	outputImageInfo.sampler = postProcessImage->textureSampler;

    bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[index]);
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

	std::vector<VkWriteDescriptorSet> writePostProcess;
	descriptorManagerVulkan->writeStorageImage(&writePostProcess, postDescriptorSets[index], 0, outputImageInfo);
	descriptorManagerVulkan->writeUniform(&writePostProcess, postDescriptorSets[index], 1, bufferInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writePostProcess);
}

void RaytracingPipelineVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	rendererManagerVulkan->setDisplayImage(nullptr);
	_cleanupResources();
	_createResources();
	_createDescriptor();
	uint32_t frameCount = VulkanUtils::numFrames();
	for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void RaytracingPipelineVulkan::_cleanupResources()
{
	rtPipeline->destroy();
	postProcessPipeline->destroy();
}

BlasInput RaytracingPipelineVulkan::_toVkGeometry(uint32_t meshID) {
    const MeshManager::MeshData& meshData = meshManager->getMeshData(meshID);
    Mesh* mesh = meshManager->getMesh(meshID);

	ObjectDesc desc{};
	desc.materialsRef = materialsAddress;
	DeviceAddressBufferVulkan* bdaBuffer;
	bdaBuffer = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.matIndicesBDA_ID));
	desc.materialIndicesRef = bdaBuffer->getReference();
	bdaBuffer = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.vertexBDA_ID));
	desc.vertexAddress = bdaBuffer->getReference();
	bdaBuffer = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.indexBDA_ID));
	desc.indexAddress = bdaBuffer->getReference();
    
    // describe the geometry with BDA pointers
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    
    auto& triangles = geometry.geometry.triangles;
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = desc.vertexAddress;
    triangles.vertexStride = sizeof(Vertex);
    triangles.maxVertex = mesh->vertices.size();
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = desc.indexAddress;  
    
    // query build sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

	VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
    asGeom.geometry.triangles = triangles;

    // The entire array will be used to build the BLAS.
    uint32_t maxPrimitiveCount = mesh->indices.size() / 3;
    VkAccelerationStructureBuildRangeInfoKHR offset;
    offset.firstVertex     = 0;
    offset.primitiveCount  = maxPrimitiveCount;
    offset.primitiveOffset = 0;
    offset.transformOffset = 0;

    BlasInput input;
    input.asGeometry.emplace_back(asGeom);
    input.asBuildOffsetInfo.emplace_back(offset);
    return input;
}

void RaytracingPipelineVulkan::_createAccelStructure()
{
    printf("\nVkApp::createRtAccelerationStructure\n");
    // BLAS - Storing each primitive in a geometry
    std::vector<BlasInput> allBlas;
    allBlas.reserve(objects.size());
    printf("\n  Build vector<BlasInput> for list of objects (of length %ld).\n", objects.size());

	auto meshIDs = meshManager->listIDs();
    for (const auto& id : meshIDs)  {
        printf("    Call VkApp::objectToVkGeometryKHR to return a BlasInput entry.\n");
        BlasInput blas = _toVkGeometry(id);
        allBlas.emplace_back(blas); 
	}

    printf("\n  Call buildBlas to build vector<AccelWrap> m_blas\n");
    printf("                    from vector<BlasInput>\n");

    // TLAS
    printf("\n  Create vector<VkAccelerationStructureInstanceKHR> tlas to hold all BLASes\n");
    std::vector<VkAccelerationStructureInstanceKHR> tlas;
    tlas.reserve(instanceData.size());

	// std::unordered_map<uint32_t, uint32_t> meshToBlasIndex;
	// auto meshIDs = meshManager->listIDs();
	// for (uint32_t i = 0; i < meshIDs.size(); i++) {
	// 	BlasInput blas = _toVkGeometry(meshIDs[i]);
	// 	allBlas.emplace_back(blas);
	// 	meshToBlasIndex[meshIDs[i]] = i; 
	// }
    m_rtBuilder.buildBlas(allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}
    auto entities = scene->getEntitiesWith<TransformComponent, ModelComponent>();
	int index = 0;
    for (auto& instance : instanceData) {
		uint32_t blasIdx = 0;

        // printf("  For each object\n");
        VkAccelerationStructureInstanceKHR _i{};
        _i.transform = toTransformMatrixKHR(instance.model);  // Position of the instance
        _i.instanceCustomIndex = index; 
        _i.accelerationStructureReference = m_rtBuilder.getBlasDeviceAddress(blasIdx);
        _i.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        _i.mask  = 0xFF;       //  Only be hit if rayMask & instance.mask != 0
        _i.instanceShaderBindingTableRecordOffset = 0; // Use the same hit group for all objects
        // printf("    append object's BLAS-address and transformation to the tlas vector\n");
        tlas.emplace_back(_i);
		index++;
    }

	
    int objectsIndex = 0;
    
    // Scene* scene = SceneManager::getInstance().getActiveScene();
    // auto entities = scene->getEntitiesWith<TransformComponent>();

	// materialManager->bindMaterial(cmd, (void*)rtPipeline.get());

    // for (auto& entity : entities) {
    //     auto& transform = entity.getComponent<TransformComponent>();

    //     if (entity.hasComponent<ModelComponent>()) {
    //         uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
    //         const Model* model = modelManager->getModel(modelID);
            
    //         for (uint32_t meshID : model->meshIDs) {
	// 			const MeshManager::MeshData& meshData = meshManager->getMeshData(meshID);
                
    //             ObjectDesc desc{};
    //             desc.vertexAddress = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.vertexBDA_ID))->getReference();
    //             desc.indexAddress = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.indexBDA_ID))->getReference();
    //             desc.materialsRef = materialsAddress;
    //             desc.materialIndicesRef = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.matIndicesBDA_ID))->getReference();

    //             objects[objectsIndex] = desc;

    //             // --- B. Create TLAS Instance ---
    //             VkAccelerationStructureInstanceKHR inst{};
    //             inst.transform = toTransformMatrixKHR(transform.getModelMatrix());
                
    //             // CRITICAL: This links the Ray Hit to objects[objectsIndex]
    //             inst.instanceCustomIndex = objectsIndex; 
                
    //             // You need a way to find which BLAS index this meshID corresponds to
    //             uint32_t blasIdx = meshManager->getBlasIndex(meshID); 
    //             inst.accelerationStructureReference = m_rtBuilder.getBlasDeviceAddress(blasIdx);
                
    //             inst.mask = 0xFF;
    //             inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                
    //             tlas.push_back(inst);
    //             objectsIndex++;
    //         }
    //     }
    // }
	/*
	*/
    
    printf("\n  Call buildTlas with a list of BLAS instances\n");
    m_rtBuilder.buildTlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, false, false);
    printf("\nEnd of VkApp::createRtAccelerationStructure\n\n");

	// bufferManagerVulkan->destroy(m_scratch1_ID);
	// bufferManagerVulkan->destroy(m_scratch2_ID);
}



template <class integral>
constexpr integral align_up(integral x, size_t a) noexcept
{
    return integral((x + (integral(a) - 1)) & ~integral(a - 1));
}

void RaytracingPipelineVulkan::_createShaderBindingTable()
{
	handleSize = m_rtBuilder.m_rtProperties.shaderGroupHandleSize;
    handleAlignment = m_rtBuilder.m_rtProperties.shaderGroupHandleAlignment;
    baseAlignment   = m_rtBuilder.m_rtProperties.shaderGroupBaseAlignment;
	
    uint32_t missCount{1};
    uint32_t hitCount{1};

    uint32_t handleCount = 1 + missCount + hitCount;

    // The SBT (buffer) needs to have starting group to be aligned
    // and handles in the group to be aligned.
    uint32_t handleSizeAligned = align_up(handleSize, handleAlignment);  	// handleAlignment==32

    m_rgenRegion.stride = align_up(handleSizeAligned, baseAlignment); 		// baseAlignment==64
    m_rgenRegion.size = m_rgenRegion.stride;  // The size member must be equal to its stride member
    
    m_missRegion.stride = handleSizeAligned;
    m_missRegion.size   = align_up(missCount * handleSizeAligned, baseAlignment);
    
    m_hitRegion.stride  = handleSizeAligned;
    m_hitRegion.size    = align_up(hitCount * handleSizeAligned, baseAlignment);

    printf("Shader binding table:\n");
    printf("  alignments:\n");
    printf("    handleAlignment: %d\n", handleAlignment);
    printf("    baseAlignment:   %d\n", baseAlignment);
    printf("  counts:\n");
    printf("    miss:   %d\n", missCount);
    printf("    hit:    %d\n", hitCount);
    printf("    handle: %d = 1+missCount+hitCount\n", handleCount);
    printf("  regions stride:size:\n");
    printf("    rgen %2zd:%2zd\n", m_rgenRegion.stride, m_rgenRegion.size);
    printf("    miss %2zd:%2zd\n", m_missRegion.stride, m_missRegion.size);
    printf("    hit  %2ld:%2ld\n", m_hitRegion.stride,  m_hitRegion.size);
    printf("    call %2ld:%2ld\n", m_callRegion.stride, m_callRegion.size);

    // Get the shader group handles.  This is a byte array retrieved
    // from the pipeline.
    uint32_t             dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles(dataSize);
    printf("\n");
    VkResult result = vkGetRayTracingShaderGroupHandlesKHR(
		renderDeviceVulkan->device, rtPipeline->pipeline, 0, handleCount, dataSize, handles.data()
	);
	
    // @@ Verify success of vkGetRayTracingShaderGroupHandlesKHR.
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RT Shader Group Handle");
    }

    // Allocate a buffer for storing the SBT, and a staging buffer for transferring data to it.
    VkDeviceSize sbtSize = m_rgenRegion.size + m_missRegion.size + m_hitRegion.size + m_callRegion.size;

	uint32_t stagingBufferID = bufferManagerVulkan->createBuffer2(
		sbtSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);
	m_shaderBindingTableBufferID = bufferManagerVulkan->createBuffer2(
		sbtSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	
	auto shaderBindingTableBuff = bufferManagerVulkan->getBuffer(m_shaderBindingTableBufferID);
	auto stagingBuffer = bufferManagerVulkan->getBuffer(stagingBufferID);

    // Find the SBT addresses of each group
    VkBufferDeviceAddressInfo info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    info.buffer                    = static_cast<VkBuffer>(*shaderBindingTableBuff);
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(renderDeviceVulkan->device, &info);
    
    m_rgenRegion.deviceAddress = sbtAddress;
    m_missRegion.deviceAddress = sbtAddress + m_rgenRegion.size;
    m_hitRegion.deviceAddress  = sbtAddress + m_rgenRegion.size + m_missRegion.size;

    // Helper to retrieve the handle data
    auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

    // Map the SBT buffer and write in the handles.
    uint8_t* mappedMemAddress;
    vkMapMemory(renderDeviceVulkan->device, stagingBuffer->getMemory(), 0, sbtSize, 0, (void**)&mappedMemAddress);
    uint8_t offset = 0;

    // Raygen
    uint32_t handleIdx{0};
    memcpy(mappedMemAddress+offset, getHandle(handleIdx++), handleSize);

    // Miss
    offset = m_rgenRegion.size;
    for(uint32_t c = 0; c < missCount; c++) {
        memcpy(mappedMemAddress+offset, getHandle(handleIdx++), handleSize);
        offset += m_missRegion.stride; }

    // Hit
    offset = m_rgenRegion.size + m_missRegion.size;
    for(uint32_t c = 0; c < hitCount; c++) {
        memcpy(mappedMemAddress+offset, getHandle(handleIdx++), handleSize);
        offset += m_hitRegion.stride; }

    vkUnmapMemory(renderDeviceVulkan->device, stagingBuffer->getMemory());
    
    bufferManagerVulkan->copyBuffer(static_cast<VkBuffer>(*stagingBuffer), static_cast<VkBuffer>(*shaderBindingTableBuff), sbtSize);

	bufferManagerVulkan->destroy(stagingBufferID);
}

#pragma endregion setup