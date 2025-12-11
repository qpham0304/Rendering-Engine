#include "VulkanApplication.h"
#include <cstring>
#include <cassert>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "window/platform/GLFW/AppWindowGLFW.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"
#include "core/features/Mesh.h"

//TODO: remove from application, only renderer can see these concrete implementation
#include "graphics/framework/Vulkan/RenderDeviceVulkan.h"	
#include "graphics/framework/Vulkan/Renderers/RendererVulkan.h"
#include "graphics/framework/vulkan/resources/textures/TextureManagerVulkan.h"
#include "imgui.h"

ModelOpenGL* modelPtr = nullptr;

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

std::vector<uint32_t> indices = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};

void VulkanApplication::pushLayer(Layer* layer)
{
	layerManager->addLayer(layer);
}

void VulkanApplication::init(WindowConfig config)
{
	windowConfig = config;

	ServiceLocator::setContext(&serviceLocator);
	engineLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Client");
	appWindow = platformFactory.createWindow(windowConfig.windowPlatform);
	renderDevice = platformFactory.createRenderDevice(windowConfig.renderPlatform);
	renderDeviceVulkan = static_cast<RenderDeviceVulkan*>(renderDevice.get());
	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);

	textureManager = std::make_unique<TextureManagerVulkan>();
	meshManager = std::make_unique<MeshManager>();
	modelManager = std::make_unique<ModelManager>();

	serviceLocator.Register<TextureManager>("TextureManager", *textureManager);
	serviceLocator.Register<MeshManager>("MeshManager", *meshManager);
	serviceLocator.Register<ModelManager>("ModelManager", *modelManager);

	engineLogger->setLevel(LogLevel::Debug);

	ServiceLocator::supportingServices();
}

void VulkanApplication::start()
{
	appWindow->init(windowConfig);

	textureManager->init();
	meshManager->init();
	modelManager->init();
	
	camera.init(AppWindow::getWidth(), AppWindow::getHeight(),
		glm::vec3(2.0f, 2.0f, 2.0f),
		glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(2.0f, 2.0f, 2.0f))
	);


	EventManager::getInstance().subscribe(EventType::WindowResize, [this](Event& event) {
		WindowResizeEvent& windowResizeEvent = static_cast<WindowResizeEvent&>(event);
		camera.updateViewResize(windowResizeEvent.m_width, windowResizeEvent.m_height);
	});

	EventManager& eventManager = EventManager::getInstance();
	eventManager.subscribe(EventType::MouseScrolled, [this](Event& event) {
		MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		camera.scroll_callback(mouseEvent.m_x, mouseEvent.m_y);
	});

	eventManager.subscribe(EventType::MouseMoved, [this](Event& event) {
	MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);
		//camera.processMouse();
		//camera.processKeyboard();
		camera.processInput();
		//if (showGui) {
		//	mouseEvent.Handled = true;	// block mouse event from other layers
		//}
	});


	EventManager::getInstance().subscribe(EventType::WindowClose, [this](Event& event) {
		isRunning = false;
	});

	EventManager::getInstance().subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_2) {
			showGui = !showGui;
		}
	});

	vulkanRenderer.init();

	aruID = modelManager->loadModel("assets/models/aru/aru.gltf");
	//reimuID = modelManager->loadModel("assets/models/reimu/reimu.obj");

	uint32_t id = textureManager->loadTexture("assets/textures/mobi-padoru.png");
	renderDeviceVulkan->texture = dynamic_cast<TextureVulkan*>(textureManager->getTexture(id));

	renderDeviceVulkan->createDescriptorSets();
	renderDeviceVulkan->createTextureViewDescriptorSet();

	vertexBufferID = renderDeviceVulkan->vulkanBufferManager.createVertexBuffer(vertices.data(), vertices.size(), renderDeviceVulkan->commandPool.commandPool);
	indexBufferID = renderDeviceVulkan->vulkanBufferManager.createIndexBuffer(indices.data(), indices.size(), renderDeviceVulkan->commandPool.commandPool);

	Model* aruModel = modelManager->getModel(aruID);
	clientLogger->warn("mesh count: {}", aruModel->meshIDs.size());
	for (uint32_t meshID : aruModel->meshIDs) {
		Mesh* mesh = meshManager->getMesh(meshID);
		uint32_t meshVertexBufferID = renderDeviceVulkan->vulkanBufferManager.createVertexBuffer(mesh->vertices.data(), mesh->vertices.size(), renderDeviceVulkan->commandPool.commandPool);
		uint32_t meshIndexBufferID = renderDeviceVulkan->vulkanBufferManager.createIndexBuffer(mesh->indices.data(), mesh->indices.size(), renderDeviceVulkan->commandPool.commandPool);
		aruMeshIDs.first.push_back(meshVertexBufferID);
		aruMeshIDs.second.push_back(meshIndexBufferID);
		clientLogger->warn("mesh ID: {}", mesh->indices.size());
	}

	guiManager->init(windowConfig);

	layerManager = std::make_unique<LayerManager>(serviceLocator);

	editorLayer = new EditorLayer("EditorLayer", *guiManager);
	//editorLayer->init(guiManager.get());
}


void VulkanApplication::run() {
	//pushLayer(editorLayer);

	while (isRunning) {
		appWindow->onUpdate();
		float dt = appWindow->getTime();
		eventManager.onUpdate();
		sceneManager.onUpdate(dt);
		layerManager->onUpdate();

		camera.onUpdate();
		camera.processInput();

		render();

		if (showGui) {
			//guiManager->start();
			sceneManager.onGuiUpdate(dt);
			layerManager->onGuiUpdate();
			guiManager->end();
		}
	}
}

void VulkanApplication::end()
{
	renderDeviceVulkan->waitIdle();
	guiManager->onClose();
	textureManager->shutdown();
	renderDeviceVulkan->onClose();
	appWindow->onClose();
}

void VulkanApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	vulkanRenderer.beginRecording(commandBuffer);

	// issue draw
	renderDeviceVulkan->vulkanBufferManager.bind(vertexBufferID);
	renderDeviceVulkan->vulkanBufferManager.bind(indexBufferID);
	renderDeviceVulkan->draw(static_cast<uint32_t>(indices.size()), 1);

	//// issue draw model
	//for (size_t i = 0; i < aruMeshIDs.first.size(); i++) {
	//	uint32_t vertexID = aruMeshIDs.first[i];
	//	uint32_t indexID = aruMeshIDs.second[i];

	//	// bind the vertex buffer
	//	renderDeviceVulkan->vulkanBufferManager.bind(vertexID); // binding 0

	//	// bind the index buffer
	//	renderDeviceVulkan->vulkanBufferManager.bind(indexID);
	//	Model* aruModel = modelManager->getModel(aruID);

	//	// get index count for this mesh
	//	Mesh* mesh = meshManager->getMesh(aruModel->meshIDs[i]);
	//	uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());

	//	// draw the mesh
	//	renderDeviceVulkan->draw(indexCount, 1);
	//}

	renderGui(commandBuffer);

	vulkanRenderer.endRecording(commandBuffer);
}

void VulkanApplication::render()
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	VulkanDevice::UniformBufferObject ubo{};
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	//ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.model = glm::mat4(1.0);
	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.proj[1][1] *= -1;
	renderDeviceVulkan->vulkanBufferManager.updateUniformBuffer(renderDeviceVulkan->getCurrentFrame(), ubo);

	vulkanRenderer.beginFrame();

	vulkanRenderer.render();
	recordCommandBuffer(
		renderDeviceVulkan->commandPool.commandBuffers[renderDeviceVulkan->getCurrentFrame()],
		renderDeviceVulkan->getImageIndex()
	);

	vulkanRenderer.endFrame();
}

void VulkanApplication::renderGui(void* commandBuffer)
{
	if (showGui) {
		guiManager->start();
		ImGui::Begin("Texture View");
		ImGui::BeginChild("Image View");
		ImGui::Image((ImTextureID)renderDeviceVulkan->imguiTextureDescriptorSet, ImVec2(500, 500));
		ImGui::EndChild();
		ImGui::End();
		guiManager->render(commandBuffer);
	}
}
