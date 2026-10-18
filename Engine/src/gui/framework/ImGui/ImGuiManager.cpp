#include "ImGuiManager.h"
#include "gui/GuiManager.h"
#include "core/scene/SceneManager.h"
#include "window/appwindow.h"
#include "gui/framework/ImGui/theme/ImGuiThemes.h"
#include "graphics/utils/Utils.h"
#include "core/components/MComponent.h"
#include "core/features/ServiceLocator.h"
#include "graphics/renderers/RenderDevice.h"
#include "core/features/Camera.h"
#include "core/features/EngineStates.h"
#include "volk.h"

ImGuiManager::ImGuiManager() : GuiManager("ImGuiManager")
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
}

ImGuiManager::ImGuiManager(bool darkTheme) : GuiManager()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	this->darkTheme = darkTheme;
}

bool ImGuiManager::init(WindowConfig config)
{
	Service::init(config);

	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::GetStyle().ScaleAllSizes(1.5); // Scale up all sizes
	ImGui::GetIO().FontGlobalScale = 1.5; // Scale up font size

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;		// Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;		// Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;			// Enable docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;			// Enable Multi-Viewport / Platform Windows
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;

	// io.IniFilename = "Assets/imgui.ini";

	float fontSize = 16.0f;

	static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	icons_config.GlyphMinAdvanceX = fontSize;
	icons_config.GlyphOffset.y = -0.5f;
	icons_config.GlyphOffset.x = -1.75f;

	// Setup ImGui GLFW and OpenGL bindings
	void* appWindow = AppWindow::getWindowHandle();
	if (m_config.windowPlatform == WindowPlatform::GLFW) {
		GLFWwindow* window = static_cast<GLFWwindow*>(appWindow);

		if (m_config.renderPlatform == RenderPlatform::OPENGL) {
			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 330");
		}
		else if (m_config.renderPlatform == RenderPlatform::VULKAN) {
			_initVulkan();
		}
		else {
			throw std::runtime_error("ImGuiManager: Unsupported render platform for ImGui initialization with GLFW");
			return false;
		}
	}
	else if (m_config.windowPlatform == WindowPlatform::SDL) {
		if (m_config.renderPlatform == RenderPlatform::VULKAN) {
			_initVulkan();
		}
		else {
			throw std::runtime_error("ImGuiManager: Unsupported render platform for ImGui initialization with SDL");
			return false;
		}
	}
	else {
		throw std::runtime_error("ImGuiManager: Unsupported window platform for ImGui initialization");
		return false;
	}

	io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Regular.ttf", fontSize);
	io.Fonts->AddFontFromFileTTF("assets/fonts/fa/" FONT_ICON_FILE_NAME_FAS, fontSize*0.75, &icons_config, icons_ranges);
	
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMenuButtonPosition = ImGuiDir_None;
	style.FramePadding = ImVec2(10, 4);
	style.ItemSpacing = ImVec2(8, 4);
	style.ItemInnerSpacing = ImVec2(1, 6);
	style.FrameBorderSize = 0.0f;
	style.WindowBorderSize = 0.0f;
	style.TabRounding = 8;
	style.GrabMinSize = 15;

	darkTheme ? useDarkTheme() : useLightTheme(); // preset style from others

	return true;
}

void ImGuiManager::onUpdate(const FrameTime& frameTime)
{
	for (const auto& widget : widgets) {
		widget->update();
	}
}

ImGuiManager::~ImGuiManager() = default;

void ImGuiManager::start(void* handle)
{
	if (width == -1 && height == -1) {
		throw std::logic_error("Object not fully initialized. Did you call init() function before start?");
	}
	
	// Start the ImGui frame
	switch (m_config.renderPlatform) {
		case RenderPlatform::OPENGL: ImGui_ImplOpenGL3_NewFrame(); break;
		case RenderPlatform::VULKAN: ImGui_ImplVulkan_NewFrame(); break;
	}

	if (m_config.windowPlatform == WindowPlatform::GLFW) {
		ImGui_ImplGlfw_NewFrame();
	} else if (m_config.windowPlatform == WindowPlatform::SDL) {
		ImGui_ImplSDL3_NewFrame();
	} else {
		throw std::runtime_error("unsupported window platform");
	}
	
	ImGui::NewFrame();

	// Create a dock space
	//ImGui::DockSpaceOverViewport(0); // old way to setup docking space
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags host_window_flags = 
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDecoration | 
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); // transparent background

	ImGui::Begin("MainDockSpace", nullptr, host_window_flags);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);

	// Create dockspace node that lets ImGui windows dock
	ImGui::DockSpace(ImGui::GetID("MainDockSpaceID"), ImVec2(0, 0), dockspace_flags);
	ImGui::End();

}

// toolbar row drawn and centered above the viewport image
void ImGuiManager::applicationWindow()
{
	const ImVec2 buttonSize = ImVec2(28.0f, 28.0f);
	const ImVec4 playingColor = ImVec4(0.20f, 0.60f, 0.28f, 1.0f);
	const ImVec4 playingHoveredColor = ImVec4(0.25f, 0.70f, 0.33f, 1.0f);
	const ImVec4 idleColor = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
	const ImVec4 idleHoveredColor = ImVec4(0.38f, 0.38f, 0.38f, 1.0f);

	const bool isPlaying = EngineState::isPlaying();
	const float groupWidth = buttonSize.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - groupWidth) * 0.5f);

	ImGui::PushStyleColor(ImGuiCol_Button, isPlaying ? playingColor : idleColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isPlaying ? playingHoveredColor : idleHoveredColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, isPlaying ? idleColor : playingColor);
	if (ImGui::Button(isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY, buttonSize)) {
		isPlaying ? EngineState::pause() : EngineState::play();
	}
	ImGui::PopStyleColor(3);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(isPlaying ? "Pause" : "Play");
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(EngineState::isStopped());
	if (ImGui::Button(ICON_FA_STOP, buttonSize)) {
		EngineState::stop();
	}
	ImGui::EndDisabled();
}

void ImGuiManager::render(void* handle)
{
	for (const auto& widget : widgets) {
		widget->render();
	}

	ImGui::Render();
	switch (m_config.renderPlatform) {
		case RenderPlatform::OPENGL:
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			break;
		case RenderPlatform::VULKAN:
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(handle));
			break;
		default:
			assert(false && "Unknown render platform");
			break;
	}
}

//ImGui
void ImGuiManager::end(void* handle)
{
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		switch (m_config.windowPlatform) {
			case WindowPlatform::GLFW: {
				GLFWwindow* currentContext = static_cast<GLFWwindow*>(AppWindow::getWindowHandle());
				AppWindow::setContextCurrent();
				break;
			}
			case WindowPlatform::SDL:
				break;	// no gl context to restore
			default:
				assert(false && "Unknown render platform");
				break;
		}
	}
}

bool ImGuiManager::onClose()
{
	switch (m_config.renderPlatform) {
		case RenderPlatform::OPENGL:
			_onCloseOpenGL();
			break;
		case RenderPlatform::VULKAN:
			_onCloseVulkan();
			break;
		default:
			assert(false && "Unknown render platform");
			break;
	}

	switch (m_config.windowPlatform) {
		case WindowPlatform::GLFW:
			ImGui_ImplGlfw_Shutdown();
			break;
		case WindowPlatform::SDL:
			AppWindow::setNativeEventCallback(nullptr);
			ImGui_ImplSDL3_Shutdown();
			break;
		default:
			assert(false && "Unknown window platform");
			break;
	}
	ImGui::DestroyContext();


	return true;
}

void ImGuiManager::setTheme(bool darkTheme)
{
	this->darkTheme = darkTheme;
}

void ImGuiManager::useLightTheme()
{
	// ImGuiThemes::purpleTheme();
	ImGuiThemes::lightTheme();
}

void ImGuiManager::useDarkTheme()
{
	ImGuiThemes::darkTheme();
}

void ImGuiManager::renderGuizmo(TransformComponent& transformComponent)
{
	ImGuizmo::BeginFrame();
	glm::vec3 translateVector(0.0f, 0.0f, 0.0f);
	glm::vec3 scaleVector(1.0f, 1.0f, 1.0f);

	float viewManipulateRight = ImGui::GetIO().DisplaySize.x;
	float viewManipulateTop = 0;

	auto v = &SceneManager::cameraController->getViewMatrix()[0][0];
	auto p = glm::value_ptr(SceneManager::cameraController->getProjectionMatrix());

	glm::mat4& transform = transformComponent.getModelMatrix();

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
	// ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
	float wd = (float)ImGui::GetWindowWidth();
	float wh = (float)ImGui::GetWindowHeight();

	ImVec2 windowPos = ImGui::GetWindowPos();
	ImGuizmo::SetRect(windowPos.x, windowPos.y, wd, wh);
	//ImVec2 viewportPos = ImGui::GetMainViewport()->Pos;
	//ImGuizmo::SetRect(
	//	windowPos.x - viewportPos.x,
	//	windowPos.y - viewportPos.y,
	//	ImGui::GetWindowWidth(),
	//	ImGui::GetWindowHeight()
	//);

	glm::mat4 identity(1.0f);

	if (drawGrid) {
		ImGuizmo::DrawGrid(v, p, glm::value_ptr(identity), 100.f);
	}

	ImGuizmo::OPERATION op = (ImGuizmo::OPERATION)GuizmoType;

	if (op == ImGuizmo::ROTATE) {
		op = (ImGuizmo::OPERATION)(ImGuizmo::ROTATE | ImGuizmo::ROTATE_SCREEN);
	}
	// op = ImGuizmo::UNIVERSAL;

	bool res = ImGuizmo::Manipulate(
		v,
		p,
		op,
		ImGuizmo::LOCAL,
		glm::value_ptr(transform)
	);
	
	viewManipulateRight = ImGui::GetWindowPos().x + wd;
	viewManipulateTop = ImGui::GetWindowPos().y;
	ImGuizmo::ViewManipulate(
		v,
		5.0f,
		ImVec2(viewManipulateRight - 128, viewManipulateTop),
		ImVec2(128, 128), 0x10101010
	);

	if (ImGuizmo::IsUsing()) {
		setGuizmoFocus(true);
		glm::vec3 translation, rotation, scale;
		Utils::Math::DecomposeTransform(transform, translation, rotation, scale);	// graphics utils dependency, resolve when have time
		glm::vec3 deltaRotation = rotation - transformComponent.rotateVec;

		transformComponent.translateVec = translation;
		transformComponent.rotateVec += deltaRotation;
		transformComponent.scaleVec = scale;
	}
	else {
		setGuizmoFocus(false);
	}
}

void ImGuiManager::guizmoTranslate() 
{
	GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
};
void ImGuiManager::guizmoRotate() 
{
	GuizmoType = ImGuizmo::OPERATION::ROTATE;
};
void ImGuiManager::guizmoScale() 
{
	GuizmoType = ImGuizmo::OPERATION::SCALE;
};

// OpenGL specific setup
void ImGuiManager::_onCloseOpenGL()
{
	ImGui_ImplOpenGL3_Shutdown();
}


// vulkan specific setup
int ImGuiManager::_initVulkan()
{
	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");

	RenderDevice::DeviceInfo deviceInfo = renderDevice.getDeviceInfo();
	RenderDevice::PipelineInfo pipelineInfo = renderDevice.getPipelineInfo();
	VkInstance instance = static_cast<VkInstance>(renderDevice.getNativeInstance());
	VkDevice device = static_cast<VkDevice>(renderDevice.getNativeDevice());
	VkPhysicalDevice physicalDevice = static_cast<VkPhysicalDevice>(renderDevice.getPhysicalDevice());

	
	ImGui_ImplVulkan_LoadFunctions(deviceInfo.apiVersion.value(), [](const char* function_name, void* user_data) {
		return vkGetInstanceProcAddr(static_cast<VkInstance>(user_data), function_name);
	}, instance);
	
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
	pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
	pool_info.pPoolSizes = pool_sizes;

	VkResult result = vkCreateDescriptorPool(device, &pool_info, nullptr, &guiDescriptorPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool for ImGui");
	}

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.ApiVersion = deviceInfo.apiVersion.value();
	init_info.Instance = instance;
	init_info.PhysicalDevice = physicalDevice;
	init_info.Device = device;
	init_info.QueueFamily = deviceInfo.queueFamilyIndex.value();
	init_info.Queue = static_cast<VkQueue>(deviceInfo.queueHandle.value());
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = guiDescriptorPool;
	init_info.MinImageCount = deviceInfo.minImageCount.value();
	init_info.ImageCount = deviceInfo.imageCount.value();
	init_info.Allocator = VK_NULL_HANDLE;
	init_info.PipelineInfoMain.RenderPass = static_cast<VkRenderPass>(pipelineInfo.renderPass);
	init_info.PipelineInfoMain.Subpass = pipelineInfo.subpass;
	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.CheckVkResultFn = [](VkResult err) { if (err != VK_SUCCESS) abort(); };


	switch (m_config.windowPlatform) {
	case WindowPlatform::GLFW: {
			GLFWwindow* windowHandle = static_cast<GLFWwindow*>(AppWindow::getWindowHandle());
			ImGui_ImplGlfw_InitForVulkan(windowHandle, true);
			break;
		}
		case WindowPlatform::SDL: {
			SDL_Window* windowHandle = static_cast<SDL_Window*>(AppWindow::getWindowHandle());
			ImGui_ImplSDL3_InitForVulkan(windowHandle);
			// GLFW's backend installs its own callbacks, SDL's has to be handed every event
			AppWindow::setNativeEventCallback([](void* nativeEvent) {
				ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(nativeEvent));
			});
			break;
		}
		default: {
			assert(false && "Unknown window platform");
			break;
		}
	}
	
	ImGui_ImplVulkan_Init(&init_info);

	return 0;
}

void ImGuiManager::_onCloseVulkan()
{
	ImGui_ImplVulkan_Shutdown();
	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	VkDevice device = (VkDevice)renderDevice.getNativeDevice();
	vkDestroyDescriptorPool(device, guiDescriptorPool, nullptr);
}

void ImGuiManager::_onUpdateVulkan()
{

}