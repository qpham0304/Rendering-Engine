#include "RayTraceRendererVulkan.h"
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
#include <graphics/framework/vulkan/renderers/renderpasses/AmbientOcclusionPassVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/DeviceAddressBufferVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/AccelStructureBufferVulkan.h>
#include <core/scene/SceneManager.h>
#include <imgui.h>
#include <glm/gtx/string_cast.hpp>

RayTraceRendererVulkan::RayTraceRendererVulkan() 
	: RendererVulkan("RayTraceRendererVulkan")
{

}

RayTraceRendererVulkan::~RayTraceRendererVulkan()
{

}

bool RayTraceRendererVulkan::init(WindowConfig config)
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
	objDeviceAddress = deviceAddress->getAddress();

	lights.reserve(MAX_INSTANCES);
	size_t lightBufferSize = numLights * sizeof(LightSSBO);
	bufferManagerVulkan->createStorageBuffers(lightStoragebuffers, lightBufferSize);

	_createResources();

	m_rtBuilder.setup(renderDeviceVulkan, bufferManagerVulkan);
	m_rtBuilder.create();
    
	_createAccelStructure();
	_createDescriptor();
	_createPipeline();
	_createShaderBindingTable();

	auto materialManagerVulkan = static_cast<MaterialManagerVulkan*>(materialManager);
	materialsAddress = materialManagerVulkan->getMaterialAddress();

    EventManager::getInstance().subscribe(EventType::ModelLoadEvent, [this](Event& event) {
        ModelLoadEvent& e = static_cast<ModelLoadEvent&>(event);
        renderDeviceVulkan->waitIdle();
        // m_rtBuilder.destroy();
        m_tlasInitialized = false;
        _createAccelStructure();
        _createDescriptor();
        // _createPipeline();
        _createShaderBindingTable();
    });

    EventManager::getInstance().subscribe(EventType::KeyPressed, [this](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_2) {
			clear = !clear;
		}
    });

    EventManager::getInstance().subscribe(EventType::KeyPressed, [this](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_L) {
			explicitPass = !explicitPass;
		}
    });

    ubo.frameCount = 0;

    uint32_t imageID = textureManagerVulkan->loadTexture("assets/textures/obluenoise256.png", 1, true);
    textureManagerVulkan->registerTextureSampler(imageID);
    pushConstant.bluenoiseIdx = imageID;

	return true;
}

bool RayTraceRendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();
	
	return true;
}

void RayTraceRendererVulkan::onUpdate()
{

}

void RayTraceRendererVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);
	
	RendererVulkan::_resize();

	instanceDataPrev = std::move(instanceData);
	// instanceData.clear(); 
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
	ubo.width  = currWidth;
    ubo.height = currHeight;
    
    bool shouldClear = camera.isMoving() || clear;
    ubo.frameSeed = !shouldClear ? rand() % 32768 : ubo.frameSeed;
    ubo.frameCount += 1;
    ubo.clear = shouldClear;
	
	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	StorageBufferVulkan* ssbo = storagebuffersList[currentFrame];
	ssbo->update(instanceData.data(), instanceData.size() * sizeof(StorageBufferObject));

	StorageBufferVulkan* ssboPrev = prevStoragebufferList[currentFrame];
	ssboPrev->update(instanceDataPrev.data(), instanceDataPrev.size() * sizeof(StorageBufferObject));
	
	size_t buffersize = MAX_INSTANCES * sizeof(ObjectDesc);
	bufferManagerVulkan->updateBufferDeviceAddress(objDeviceAddressBufferID, objects.data(), buffersize);


	lastViewProj = ubo.proj * ubo.view;

	pushConstant.objectsRef = objDeviceAddress; 
	pushConstant.objectIdx  = 0;//objectsIndex;
    pushConstant.explicitPass = explicitPass ? 1 : 0;

	updateTlas();
    
	StorageBufferVulkan* lightSSBO = lightStoragebuffers[currentFrame];
	lightSSBO->update(lights.data(), lights.size() * sizeof(LightSSBO));

	writeRayTracing(cmd, currentFrame);
	writePostProcess(cmd, currentFrame);

	// rendererManagerVulkan->setDisplayImage(rayTraceImage);
	rendererManagerVulkan->setDisplayImage(postProcessImage);
}

void RayTraceRendererVulkan::writePostProcess(VkCommandBuffer cmd, uint32_t currentFrame)
{
	rayTraceImage->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1);
    postProcessImage->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1);
	
    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
    uint32_t groupX = (currWidth + 15) / 16;
    uint32_t groupY = (currHeight + 15) / 16;

	postProcessPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
	auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(postProcessSetID)[currentFrame];
	auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
	std::vector<VkDescriptorSet> sets = { descriptorSet, bindlessSet }; //TODO: add material set

	vkCmdPushConstants(cmd, postProcessPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstant), &pushConstant);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        postProcessPipeline->pipelineLayout, 0, 
        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr
    );
    vkCmdDispatch(cmd, groupX, groupY, 1);

	rayTraceImage->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
    postProcessImage->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
}

void RayTraceRendererVulkan::writeRayTracing(VkCommandBuffer cmd, uint32_t currentFrame)
{
	rayTraceImage->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1);

	rtPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	VkDescriptorSet rtSet = descriptorManagerVulkan->getDescriptorSet(raytraceSetID)[currentFrame];
	auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
	std::vector<VkDescriptorSet> sets = { rtSet, bindlessSet };

	vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, 
        rtPipeline->pipelineLayout, 0, 
        static_cast<uint32_t>(sets.size()), sets.data(), 
        0, nullptr
    );

	vkCmdPushConstants(
        cmd, rtPipeline->pipelineLayout, 
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 
        0, sizeof(PushConstant), &pushConstant
    );

	vkCmdTraceRaysKHR(cmd, &m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion, ubo.width, ubo.height, 1);

	rayTraceImage->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
}

#pragma region setup
void RayTraceRendererVulkan::_createResources() 
{
    currWidth = renderDeviceVulkan->swapchain.swapChainExtent.width;
    currHeight = renderDeviceVulkan->swapchain.swapChainExtent.height;
    auto createTexture = [this] (TextureVulkan*& texture){
        uint32_t w = currWidth;
        uint32_t h = currHeight;
        TextureSamplerConfig samplerConfig = { VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_MIPMAP_MODE_LINEAR };
        TextureConfig imageConfig { .width = w, .height = h, .format = VK_FORMAT_R32G32B32A32_SFLOAT};

        uint32_t id = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
        texture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
    };

	createTexture(rayTraceImage);
	createTexture(postProcessImage);

    textureManagerVulkan->registerTextureSampler(rayTraceImage->id());
    textureManagerVulkan->registerTextureSampler(postProcessImage->id());
}

void RayTraceRendererVulkan::_createPipeline()
{
	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);

	void* handle = materialManager->getMaterialLayout();
	auto materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);

	auto rayTraceLayout = descriptorManagerVulkan->getDescriptorLayout(raytraceLayoutID);

	rtPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    rtPipeline->createRayTracePipeline(
		"assets/shaders/spv/raytrace.rgen.spv", 
		{ rayTraceLayout, bindlessLayout, materialLayout }, 
		sizeof(PushConstant)
	);

    VkDescriptorSetLayout postProcess = descriptorManagerVulkan->getDescriptorLayout(postProcessLayoutID);
	postProcessPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	postProcessPipeline->createComputePipeline(
		"assets/shaders/spv/postProcess.comp.spv", 
		{ postProcess, bindlessLayout, materialLayout }, 
		sizeof(PushConstant)
	);

}

void RayTraceRendererVulkan::_createDescriptor()
{
	uint32_t frameCount = VulkanUtils::numFrames();

	// ray tracing pipeline
	rtBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
	};
    std::vector<VkDescriptorPoolSize> rtPoolSizes {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, frameCount * 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 1},
	};
    raytraceLayoutID = descriptorManagerVulkan->createLayout(rtBindings);
    raytracePoolID = descriptorManagerVulkan->createPool(rtPoolSizes, frameCount);
	raytraceSetID = descriptorManagerVulkan->createSets(raytraceLayoutID, raytracePoolID, frameCount);

	// post process pipeline
	postBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
    std::vector<VkDescriptorPoolSize> postPoolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 2},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, frameCount * 1 },
	};
    postProcessLayoutID = descriptorManagerVulkan->createLayout(postBindings);
    postProcessPoolID = descriptorManagerVulkan->createPool(postPoolSizes, frameCount);
	postProcessSetID = descriptorManagerVulkan->createSets(postProcessLayoutID, postProcessPoolID, frameCount);

    for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void RayTraceRendererVulkan::_updateDescriptor(uint32_t index)
{
	AccelStructureBufferVulkan* tlas = m_rtBuilder.getTlas();

	//ray tracing
	auto rtDescriptorSets = descriptorManagerVulkan->getDescriptorSet(raytraceSetID);
	DescriptorWriter writesRayTrace { {}, rtDescriptorSets[index] };
	descriptorManagerVulkan->writeAccelStruct2(writesRayTrace, rtBindings,  tlas->getDescAccelStructInfo());
	descriptorManagerVulkan->writeStorageImage2(writesRayTrace, rayTraceImage->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeUniform2(writesRayTrace, uniformbuffersList[index]->getDescUniformBufferInfo());
	descriptorManagerVulkan->writeStorage2(writesRayTrace, lightStoragebuffers[index]->getDescStorageBufferInfo());
	descriptorManagerVulkan->updateDescriptorSets(&writesRayTrace.writes);
	
	//post process
	auto postDescriptorSets = descriptorManagerVulkan->getDescriptorSet(postProcessSetID);
	DescriptorWriter writePostProcess { {}, postDescriptorSets[index] };
	descriptorManagerVulkan->writeStorageImage2(writePostProcess, rayTraceImage->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeStorageImage2(writePostProcess, postProcessImage->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeUniform2(writePostProcess, uniformbuffersList[index]->getDescUniformBufferInfo());
	descriptorManagerVulkan->writeAccelStruct2(writePostProcess, postBindings,  tlas->getDescAccelStructInfo());
	descriptorManagerVulkan->updateDescriptorSets(&writePostProcess.writes);
}

void RayTraceRendererVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	rendererManagerVulkan->setDisplayImage(nullptr);
	_cleanupResources();
    
	_createResources();
	_createDescriptor();
	for(int i = 0; i < VulkanUtils::numFrames(); i++) {
        _updateDescriptor(i);
    }
    _createPipeline();
}

void RayTraceRendererVulkan::_cleanupResources()
{
    textureManagerVulkan->destroy(rayTraceImage->id());
    textureManagerVulkan->destroy(postProcessImage->id());
	rtPipeline->destroy();
	postProcessPipeline->destroy();
}

void RayTraceRendererVulkan::_populateData(std::vector<VkAccelerationStructureInstanceKHR>& tlas, int& objectsIndex)
{	
    SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}
	lights.clear();
    
    auto entities = scene->getEntitiesWith<TransformComponent, ModelComponent>();
    for (auto& entity : entities) {
        auto& transform = entity.getComponent<TransformComponent>();
        uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
        const Model* model = modelManager->getModel(modelID);
        
        for (uint32_t meshID : model->meshIDs) {
			const MeshManager::MeshData& meshData = meshManager->getMeshData(meshID);
			
			ObjectDesc desc{};
			desc.vertexAddress = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.vertexBDA_ID))->getAddress();
			desc.indexAddress = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.indexBDA_ID))->getAddress();
			desc.materialsRef = materialsAddress;
			desc.materialIndicesRef = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.matIndicesBDA_ID))->getAddress();
			objects[objectsIndex] = desc;	//blas per mesh not per entity

            VkAccelerationStructureInstanceKHR inst{};
            inst.transform = toTransformMatrixKHR(transform.getModelMatrix());
            inst.instanceCustomIndex = objectsIndex;
            inst.accelerationStructureReference = m_rtBuilder.getBlasDeviceAddress(meshID - 1);
            inst.mask = 0xFF;
            inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            
            tlas.push_back(inst);

            Mesh* mesh = meshManager->getMesh(meshID);
            MaterialDesc mat = materialManager->getMaterial(mesh->materialID);
            if (mat.emissive > 0.01f) {
                LightSSBO light{};
                
                // light.color = mat.albedo;
                // light.color.a = mat.emissive; 

                glm::mat4 modelMatrix = transform.getModelMatrix();

                glm::vec3 localV0 = mesh->vertices[mesh->indices[0]].positions;
                glm::vec3 localV1 = mesh->vertices[mesh->indices[1]].positions;
                glm::vec3 localV2 = mesh->vertices[mesh->indices[2]].positions;

                light.v0 = modelMatrix * glm::vec4(localV0, 1.0f);
                light.v1 = modelMatrix * glm::vec4(localV1, 1.0f);
                light.v2 = modelMatrix * glm::vec4(localV2, 1.0f);

                light.instanceIdx = objectsIndex; 
                light.triangleCount = static_cast<uint32_t>(mesh->indices.size() / 3);
                
                lights.push_back(light);
            }
            objectsIndex++;
        }
    }
}

BlasInput RayTraceRendererVulkan::_toVkGeometry(uint32_t meshID) {
    const MeshManager::MeshData& meshData = meshManager->getMeshData(meshID);
    Mesh* mesh = meshManager->getMesh(meshID);

	ObjectDesc desc{};
	desc.materialsRef = materialsAddress;
	DeviceAddressBufferVulkan* bdaBuffer;
	bdaBuffer = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.matIndicesBDA_ID));
	desc.materialIndicesRef = bdaBuffer->getAddress();
	bdaBuffer = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.vertexBDA_ID));
	desc.vertexAddress = bdaBuffer->getAddress();
	bdaBuffer = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.indexBDA_ID));
	desc.indexAddress = bdaBuffer->getAddress();
    
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
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
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

void RayTraceRendererVulkan::_createAccelStructure()
{
    Timer timer("acceleration strucure build time", true);
	auto meshIDs = meshManager->listIDs();
    // BLAS - Storing each primitive in a geometry
    std::vector<BlasInput> allBlas;
    allBlas.reserve(meshIDs.size());

    std::vector<VkAccelerationStructureInstanceKHR> tlas;
    // tlas.reserve(instanceData.size());
    tlas.reserve(MAX_INSTANCES);

    lights.clear();

    for (const auto& id : meshIDs)  {
        BlasInput blas = _toVkGeometry(id);
        allBlas.emplace_back(blas);
	}

    m_rtBuilder.buildBlas(allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    int objectsIndex = 0;
	_populateData(tlas, objectsIndex);

    // first frame tlas must be built first before it can be updated or get a screen flash
    m_rtBuilder.buildTlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, m_tlasInitialized, false);
    m_tlasInitialized = true;
}

template <class integral>
constexpr integral align_up(integral x, size_t a) noexcept
{
    return integral((x + (integral(a) - 1)) & ~integral(a - 1));
}

void RayTraceRendererVulkan::_createShaderBindingTable()
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

    // Get the shader group handles.  This is a byte array retrieved from the pipeline.
    uint32_t dataSize = handleCount * handleSize;
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
        offset += m_missRegion.stride; 
    }

    // Hit
    offset = m_rgenRegion.size + m_missRegion.size;
    for(uint32_t c = 0; c < hitCount; c++) {
        memcpy(mappedMemAddress+offset, getHandle(handleIdx++), handleSize);
        offset += m_hitRegion.stride; 
    }

    vkUnmapMemory(renderDeviceVulkan->device, stagingBuffer->getMemory());
    
    bufferManagerVulkan->copyBuffer(static_cast<VkBuffer>(*stagingBuffer), static_cast<VkBuffer>(*shaderBindingTableBuff), sbtSize);

	bufferManagerVulkan->destroy(stagingBufferID);
}

void RayTraceRendererVulkan::updateTlas() {
    std::vector<VkAccelerationStructureInstanceKHR> tlas;

    int objectsIndex = 0;
    _populateData(tlas, objectsIndex);

    m_rtBuilder.buildTlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, m_tlasInitialized, false);
    
    size_t bufferSize = objectsIndex * sizeof(ObjectDesc);
    bufferManagerVulkan->updateBufferDeviceAddress(objDeviceAddressBufferID, objects.data(), bufferSize);

	VkWriteDescriptorSetAccelerationStructureKHR descASInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
	descASInfo.accelerationStructureCount = 1;
	VkAccelerationStructureKHR tlasHandle = m_rtBuilder.getTlas()->getAccelStr();
	descASInfo.pAccelerationStructures = &tlasHandle;

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	auto rayTraceSet = descriptorManagerVulkan->getDescriptorSet(raytraceSetID)[currentFrame];
	std::vector<VkWriteDescriptorSet> writes{};
	descriptorManagerVulkan->writeAccelStruct(&writes, rayTraceSet, 0, rtBindings, descASInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}
#pragma endregion setup