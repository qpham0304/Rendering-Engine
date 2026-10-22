#include "RibbonRendererVulkan.h"

#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/BufferVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <core/scene/SceneManager.h>
#include <core/features/Camera.h>
#include <core/components/MComponent.h>
#include <core/features/Timer.h>
#include <glm/gtc/constants.hpp>
#include <algorithm>

RibbonRendererVulkan::RibbonRendererVulkan(std::string serviceName)
	:	RendererVulkan(serviceName)
{

}

RibbonRendererVulkan::~RibbonRendererVulkan()
{

}

bool RibbonRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

    geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));
    depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

    points.reserve(MAX_POINTS);
    for(uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
        pointBufferIDs.push_back(bufferManagerVulkan->createBufferDeviceAddress(MAX_POINTS * sizeof(RibbonPointGPU)));
    }

    _createDescriptor();
    _createPipelines();

    return true;
}

bool RibbonRendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();

    return true;
}

void RibbonRendererVulkan::onUpdate()
{
    Scene* scene = SceneManager::getInstance().getActiveScene();
    if(!scene) {
        return;
    }

    // asset loads stay out of command recording, same as the particle textures
    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        RibbonComponent& ribbon = entity.getComponent<RibbonComponent>();
        if(ribbon.texturePath != ribbon.loadedTexturePath) {
            ribbon.loadedTexturePath = ribbon.texturePath;
            ribbon.textureID = 0;
            if(ribbon.texturePath != "None" && !ribbon.texturePath.empty()) {
                ribbon.textureID = textureManagerVulkan->loadTexture(ribbon.texturePath, 1, false);
            }
        }

        // a loaded texture is not in the bindless array until it is registered
        if(ribbon.textureID != 0 && !bindlessTextures.contains(ribbon.textureID)) {
            textureManagerVulkan->registerBindlessTexture(ribbon.textureID);
            bindlessTextures.insert(ribbon.textureID);
        }
    });

    scene->forEnitiesWith<RibbonComponent>(func);
}

void RibbonRendererVulkan::render(Camera& camera)
{
    Timer timer(m_name, true);

    Scene* scene = SceneManager::getInstance().getActiveScene();
    if(!scene || !geometryPass || !lightingPass) {
        return;
    }

    float dt = std::min(deltaTime, 0.1f);
    time = std::fmod(time + dt, 3600.0f);

    points.clear();
    draws.clear();

    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        RibbonComponent& ribbon = entity.getComponent<RibbonComponent>();
        const glm::mat4& model = entity.getComponent<TransformComponent>().getModelMatrix();
        glm::vec3 origin = glm::vec3(model[3]);
        glm::vec3 target = ribbon.targetInLocalSpace ? glm::vec3(model * glm::vec4(ribbon.target, 1.0f)) : ribbon.target;
        int segments = std::clamp(ribbon.segments, 1, MAX_SEGMENTS);

        switch(static_cast<RibbonMode>(ribbon.mode)) {
            case RibbonMode::Trail: {
                _updateTrail(ribbon, origin, dt);

                // newest first so the strip starts at the entity, the head itself is the first point
                std::vector<glm::vec3> trail { origin };
                std::vector<float> ages { 0.0f };
                for(auto it = ribbon.trailPoints.rbegin(); it != ribbon.trailPoints.rend(); ++it) {
                    trail.push_back(it->position);
                    ages.push_back(it->age);
                }
                StripStyle style{};
                style.ages = &ages;
                _appendStrip(trail, ribbon, style);
                break;
            }

            case RibbonMode::Beam: {
                if(!ribbon.isPlaying) {
                    break;
                }
                std::vector<glm::vec3> beam;
                for(int i = 0; i <= segments; i++) {
                    beam.push_back(glm::mix(origin, target, static_cast<float>(i) / segments));
                }
                _appendStrip(beam, ribbon, StripStyle{});
                break;
            }

            case RibbonMode::Lightning: {
                if(!ribbon.isPlaying) {
                    ribbon.boltStrips.clear();
                    break;
                }
                _updateLightning(ribbon, origin, target, dt);
                for(size_t i = 0; i < ribbon.boltStrips.size(); i++) {
                    StripStyle style{};
                    style.brightness = ribbon.boltBrightness;
                    if(i > 0) {     // branches are thinner and dimmer than the main bolt
                        style.widthScale = 0.5f;
                        style.alphaScale = 0.7f;
                    }
                    _appendStrip(ribbon.boltStrips[i], ribbon, style);
                }
                break;
            }

            default:
                break;
        }
    });

    scene->forEnitiesWith<RibbonComponent>(func);

    if(draws.empty()) {
        return;
    }

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    bufferManagerVulkan->updateBufferDeviceAddress(pointBufferIDs[currentFrame], points.data(), points.size() * sizeof(RibbonPointGPU));
    pushConstant.pointsRef = bufferManagerVulkan->getBuffer(pointBufferIDs[currentFrame])->getAddress();

    TextureVulkan* sceneImage = lightingPass->renderTarget.colorTexture;
    TextureVulkan* depthImage = geometryPass->renderTarget.depthTexture;
    uint32_t width = geometryPass->renderTarget.width;
    uint32_t height = geometryPass->renderTarget.height;

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = sceneImage->textureImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthImage->textureImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {width, height} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];

    renderDeviceVulkan->beginLabel(cmd, "Ribbon Pass", {0.5, 1.0, 1.0, 1.0});
    vkCmdBeginRendering(cmd, &renderingInfo);
    renderDeviceVulkan->setViewport(width, height);
    renderDeviceVulkan->setScissor(width, height);

    // alpha blended ribbons first, additive is order independent and goes on top
    std::stable_sort(draws.begin(), draws.end(), [](const StripDraw& a, const StripDraw& b) {
        return a.blendMode > b.blendMode;
    });

    VulkanPipeline* boundPipeline = nullptr;
    for(const StripDraw& draw : draws) {
        VulkanPipeline* pipeline = pipelines[draw.blendMode].get();
        if(pipeline != boundPipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 1, 1, &bindlessSet, 0, nullptr);
            boundPipeline = pipeline;
        }

        pushConstant.firstPoint = draw.firstPoint;
        pushConstant.pointCount = draw.pointCount;
        pushConstant.textureID = draw.textureID;
        vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RibbonPushConstant), &pushConstant);

        // six vertices per segment, the vertex shader pulls the two points of its segment
        vkCmdDraw(cmd, (draw.pointCount - 1) * 6, 1, 0, 0);
    }

    vkCmdEndRendering(cmd);
    renderDeviceVulkan->endLabel(cmd);
}

void RibbonRendererVulkan::_updateTrail(RibbonComponent& ribbon, const glm::vec3& head, float dt)
{
    // oldest points are at the front and leave first
    for(auto& point : ribbon.trailPoints) {
        point.age += dt;
    }
    float lifetime = std::max(ribbon.trailLifetime, 0.01f);
    auto expired = std::find_if(ribbon.trailPoints.begin(), ribbon.trailPoints.end(), [&](const RibbonComponent::TrailPoint& p) {
        return p.age < lifetime;
    });
    ribbon.trailPoints.erase(ribbon.trailPoints.begin(), expired);

    // a stopped trail keeps fading out what it already recorded
    if(!ribbon.isPlaying) {
        return;
    }

    bool moved = ribbon.trailPoints.empty() || glm::distance(ribbon.trailPoints.back().position, head) >= ribbon.minSegmentLength;
    if(moved) {
        ribbon.trailPoints.push_back({ head, 0.0f });
    }

    size_t maxPoints = static_cast<size_t>(std::clamp(ribbon.maxPoints, 2, MAX_SEGMENTS));
    if(ribbon.trailPoints.size() > maxPoints) {
        ribbon.trailPoints.erase(ribbon.trailPoints.begin(), ribbon.trailPoints.begin() + (ribbon.trailPoints.size() - maxPoints));
    }
}

void RibbonRendererVulkan::_updateLightning(RibbonComponent& ribbon, const glm::vec3& start, const glm::vec3& end, float dt)
{
    // a bolt keeps its shape for a moment, regenerating every frame would only read as noise
    ribbon.regenerateTimer -= dt;
    if(ribbon.regenerateTimer > 0.0f && !ribbon.boltStrips.empty()) {
        return;
    }
    ribbon.regenerateTimer = std::max(ribbon.regenerateInterval, 0.0f);

    std::mt19937 rng(ribbon.seed++);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    float length = glm::distance(start, end);
    int segments = std::clamp(ribbon.segments, 1, MAX_SEGMENTS);

    ribbon.boltStrips.clear();
    ribbon.boltStrips.push_back(_generateBolt(start, end, segments, ribbon.jitter, rng));

    // branches split off the middle of the main bolt and lean away from it
    const std::vector<glm::vec3>& main = ribbon.boltStrips[0];
    glm::vec3 axis = length > 1e-5f ? (end - start) / length : glm::vec3(0.0f, 1.0f, 0.0f);
    for(int i = 0; i < std::clamp(ribbon.branches, 0, 8) && main.size() > 4; i++) {
        size_t index = main.size() / 4 + static_cast<size_t>(unit(rng) * (main.size() / 2));
        glm::vec3 branchStart = main[std::min(index, main.size() - 1)];
        glm::vec3 lean = glm::normalize(glm::vec3(unit(rng), unit(rng), unit(rng)) * 2.0f - 1.0f + axis * 0.5f);
        glm::vec3 branchEnd = branchStart + glm::normalize(glm::mix(axis, lean, 0.7f)) * (ribbon.branchLength * length);
        ribbon.boltStrips.push_back(_generateBolt(branchStart, branchEnd, std::max(segments / 4, 2), ribbon.jitter, rng));
    }

    ribbon.boltBrightness = 1.0f - std::clamp(ribbon.flicker, 0.0f, 1.0f) * unit(rng);
}

// midpoint displacement: split every segment and push its middle sideways, half as far on each pass
std::vector<glm::vec3> RibbonRendererVulkan::_generateBolt(const glm::vec3& start, const glm::vec3& end, int segments, float jitter, std::mt19937& rng) const
{
    std::uniform_real_distribution<float> signedUnit(-1.0f, 1.0f);
    std::uniform_real_distribution<float> angle(0.0f, glm::two_pi<float>());

    float length = glm::distance(start, end);
    glm::vec3 axis = length > 1e-5f ? (end - start) / length : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 helper = std::abs(axis.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 sideA = glm::normalize(glm::cross(axis, helper));
    glm::vec3 sideB = glm::cross(axis, sideA);

    std::vector<glm::vec3> bolt { start, end };
    float offset = jitter * length;
    while(static_cast<int>(bolt.size()) - 1 < segments) {
        std::vector<glm::vec3> split;
        split.reserve(bolt.size() * 2);
        for(size_t i = 0; i + 1 < bolt.size(); i++) {
            float a = angle(rng);
            glm::vec3 sideways = (sideA * std::cos(a) + sideB * std::sin(a)) * (signedUnit(rng) * offset);
            split.push_back(bolt[i]);
            split.push_back((bolt[i] + bolt[i + 1]) * 0.5f + sideways);
        }
        split.push_back(bolt.back());
        bolt = std::move(split);
        offset *= 0.5f;
    }
    return bolt;
}

void RibbonRendererVulkan::_appendStrip(const std::vector<glm::vec3>& positions, const RibbonComponent& ribbon, const StripStyle& style)
{
    if(positions.size() < 2 || points.size() + positions.size() > MAX_POINTS) {
        return;
    }

    // distance along the strip drives the start to end look and the texture coordinate
    std::vector<float> distances(positions.size(), 0.0f);
    for(size_t i = 1; i < positions.size(); i++) {
        distances[i] = distances[i - 1] + glm::distance(positions[i - 1], positions[i]);
    }
    float total = distances.back();
    if(total < 1e-5f) {
        return;
    }

    float lifetime = std::max(ribbon.trailLifetime, 0.01f);
    StripDraw draw{};
    draw.firstPoint = static_cast<uint32_t>(points.size());
    draw.pointCount = static_cast<uint32_t>(positions.size());
    draw.textureID = static_cast<int>(ribbon.textureID);
    draw.blendMode = std::clamp(ribbon.blendMode, 0, static_cast<int>(pipelines.size()) - 1);

    for(size_t i = 0; i < positions.size(); i++) {
        float t = distances[i] / total;
        RibbonPointGPU point{};
        point.position = positions[i];
        point.width = glm::mix(ribbon.widthStart, ribbon.widthEnd, t) * style.widthScale;
        point.color = glm::mix(ribbon.colorStart, ribbon.colorEnd, t);
        point.color = glm::vec4(glm::vec3(point.color) * style.brightness, point.color.a * style.alphaScale);
        if(style.ages) {
            point.color.a *= 1.0f - std::clamp((*style.ages)[i] / lifetime, 0.0f, 1.0f);
        }
        point.u = distances[i] * ribbon.textureTiling - time * ribbon.textureScroll;
        points.push_back(point);
    }
    draws.push_back(draw);
}

void RibbonRendererVulkan::_createPipelines()
{
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	AttachmentsInfo attachmentsInfo{};
    attachmentsInfo.colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
    attachmentsInfo.depthAttachmentFormat = depthFormat;
    attachmentsInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkDescriptorSetLayout descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(textureManagerVulkan->getBindlessTextureLayout());

    for(size_t blendMode = 0; blendMode < pipelines.size(); blendMode++) {
        PipelineConfigInfo config = VulkanPipeline::defaultPipelineConfigInfo(1);
        config.renderPass = VK_NULL_HANDLE;
        bool alpha = blendMode == static_cast<size_t>(ParticleBlendMode::Alpha);
        alpha ? config.setAlphaBlend() : config.setAdditiveBlend();

        // same alpha handling as the particles, the target alpha must not depend on draw order
        for(auto& attachment : config.colorBlendAttachments) {
            attachment.srcAlphaBlendFactor = alpha ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ZERO;
            attachment.dstAlphaBlendFactor = alpha ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
        }

        config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;     // a strip twists, both sides are seen
        config.depthStencilInfo.depthTestEnable = VK_TRUE;
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;

        pipelines[blendMode] = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
        pipelines[blendMode]->createGraphicsPipelineDynamic(
            "assets/shaders/spv/ribbon.vert.spv",
            "assets/shaders/spv/ribbon.frag.spv",
            config,
            attachmentsInfo,
            vertexInputInfo,
            { descriptorSetLayout, bindlessLayout },
            sizeof(RibbonPushConstant)
        );
    }
}

void RibbonRendererVulkan::_createDescriptor()
{
    layoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	});

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
	};
	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);

	descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);
    auto uniformBuffersList = rendererManagerVulkan->getBufferUBO();
	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		DescriptorWriter writer{{}, descriptorSets[i] };
		descriptorManagerVulkan->writeUniform2(writer, uniformBuffersList[i]->getDescUniformBufferInfo());
		descriptorManagerVulkan->updateDescriptorSets(&writer.writes);
	}
}

void RibbonRendererVulkan::_recreateResources()
{
}

void RibbonRendererVulkan::_cleanupResources()
{
    for(auto& pipeline : pipelines) {
        pipeline->destroy();
    }
}

void RibbonRendererVulkan::addToGraph(RenderGraphVulkan& graph)
{
    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    TextureVulkan* sceneImage = lightingPass->renderTarget.colorTexture;
    TextureVulkan* depthImage = geometryPass->renderTarget.depthTexture;

    auto scene = graph.importTexture(sceneImage);
    auto depth = graph.importTexture(depthImage);

    RenderPassNode node{};
    node.name = "Ribbon";
    node.reads = {
        { depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL }
    };
    node.writes = {
        { scene, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
    };
    node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        Camera* camera = SceneManager::cameraController;
        if(camera) {
            render(*camera);
        }
    };

    graph.addPass(node);
}
