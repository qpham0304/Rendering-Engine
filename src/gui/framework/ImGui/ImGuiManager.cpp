#include "ImGuiManager.h"
#include "../../src/gui/GuiManager.h"
#include "../../src/core/scene/SceneManager.h"
#include "../../src/window/appwindow.h"
#include "../../src/gui/framework/ImGui/theme/ImGuiThemes.h"
#include "../../src/graphics/utils/Utils.h"
#include "../../src/core/components/MComponent.h"
#include "../../src/apps/Vulkan-demo/FollowDemo.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_vulkan.h>
#include "camera.h"

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

int ImGuiManager::init(WindowConfig config)
{
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::GetStyle().ScaleAllSizes(1.5); // Scale up all sizes
	ImGui::GetIO().FontGlobalScale = 1.5; // Scale up font size

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;		// Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;		// Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;			// Enable docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		// Enable Multi-Viewport / Platform Windows
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;

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
	if (config.windowPlatform == WindowPlatform::GLFW) {
		if (config.renderPlatform == RenderPlatform::OPENGL) {
			ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(appWindow), true);
			ImGui_ImplOpenGL3_Init("#version 330");
		}
		else if (config.renderPlatform == RenderPlatform::VULKAN) {
			ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(appWindow), true);
			//ImGuiContext context = Demo::GetContext();

			//VkDescriptorPool imguiPool = createImguiDescriptorPool(device);

			// Vulkan init info
			//ImGui_ImplVulkan_InitInfo init_info = {};
			//init_info.Instance = context.instance;
			//init_info.PhysicalDevice = context.physicalDevice;
			//init_info.Device = context.device;
			//init_info.QueueFamily = context.graphicsQueueFamily;
			//init_info.Queue = context.graphicsQueue;
			//init_info.PipelineCache = VK_NULL_HANDLE;
			//init_info.DescriptorPool = imguiPool;
			//init_info.Allocator = nullptr;
			//init_info.MinImageCount = context.minImageCount;
			//init_info.ImageCount = context.swapchainImageCount;
			//init_info.CheckVkResultFn = [](VkResult err) { if (err != VK_SUCCESS) abort(); };

			//ImGui_ImplVulkan_Init(&init_info, renderPass);

			//VkCommandBuffer cmdBuf = beginSingleUseCommandBuffer(device, commandPool);
			//ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);
			//endSingleUseCommandBuffer(device, commandPool, graphicsQueue, cmdBuf);
			//ImGui_ImplVulkan_DestroyFontUploadObjects();
			throw std::runtime_error("ImGuiManager: Vulkan ImGui initialization not implemented yet");

		}
		else {
			throw std::runtime_error("ImGuiManager: Unsupported render platform for ImGui initialization with GLFW");
		}
	}
	else {
		throw std::runtime_error("ImGuiManager: Unsupported window platform for ImGui initialization");
	}

	io.Fonts->AddFontFromFileTTF("src/gui/fonts/Roboto/Roboto-Regular.ttf", fontSize);
	io.Fonts->AddFontFromFileTTF("src/gui/fonts/fa/" FONT_ICON_FILE_NAME_FAS, fontSize*0.75, &icons_config, icons_ranges);

	darkTheme ? useDarkTheme() : useLightTheme(); // preset style from others
	
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMenuButtonPosition = ImGuiDir_None;
	style.FramePadding = ImVec2(10, 4);
	style.ItemSpacing = ImVec2(8, 4);
	style.ItemInnerSpacing = ImVec2(1, 6);
	style.FrameBorderSize = 0.0f;
	style.WindowBorderSize = 0.0f;
	style.TabRounding = 8;
	style.GrabMinSize = 15;

	std::unique_ptr<ImGuiMenuWidget> menu = std::make_unique<ImGuiMenuWidget>();
	widgets.push_back(std::move(menu));
}

ImGuiManager::~ImGuiManager() = default;

void ImGuiManager::start()
{
	if (width == -1 && height == -1) {
		throw std::logic_error("Object not fully initialized. Did you call init() function before start?");
	}
	
	// Start the ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Create a dock space
#define DOCK_SPACE_TRANSPARENT
#ifdef DOCK_SPACE_TRANSPARENT
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags host_window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground;

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
#else
	ImGui::DockSpaceOverViewport(0);

#endif
}

void ImGuiManager::debugWindow(ImTextureID texture)
{
	glm::vec3 camPos = SceneManager::cameraController->getPosition();
	std::string x = "x: " + std::to_string(camPos.x).substr(0, 4);
	std::string y = "y: " + std::to_string(camPos.y).substr(0, 4);
	std::string z = "z: " + std::to_string(camPos.z).substr(0, 4);

	if (ImGui::Begin("Debug Window"))
	{
		//std::string countVertices = "Vertices: " + std::to_string(SceneManager::getNumVertices() * 3);
		//ImGui::Text(countVertices.c_str());
		//countVertices = "Triangles: " + std::to_string(SceneManager::getNumVertices());
		ImGui::SameLine();
		//ImGui::Text(countVertices.c_str());
		ImGui::Text("Camera positon");
		ImGui::SameLine();
		ImGui::Text(x.c_str());
		ImGui::SameLine();
		ImGui::Text(y.c_str());
		ImGui::SameLine();
		ImGui::Text(z.c_str());
		// Using a Child allow to fill all the space of the window.
		// It also alows customization
		ImGui::BeginChild("Debug shadow window");
		// Get the size of the child (i.e. the whole draw size of the windows).
		ImVec2 wsize = ImGui::GetWindowSize();
		ImGui::Image(texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
		ImGui::EndChild();
	}
	ImGui::End();
}

void ImGuiManager::applicationWindow()
{
	//start group
	ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
	ImGui::BeginGroup();
	ImGui::Button("A");
	ImGui::SameLine();
	ImGui::Button("B");
	ImGui::SameLine();
	ImGui::Button("C");
	ImGui::EndGroup();

	ImGuiIO& io = ImGui::GetIO();

	//center group
	ImVec4 buttonActiveColor = ImVec4{ 0.0f, (float)140 / 255, (float)184 / 255, 0.8 };
	ImVec2 buttonSize = ImVec2(36.0f, 36.0f);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonActiveColor);
	ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x / 2, 10.0f));
	ImGui::BeginGroup();
	ImGui::Button(ICON_FA_PAUSE, buttonSize);
	ImGui::SameLine();
	ImGui::Button(ICON_FA_PLAY, buttonSize);
	ImGui::SameLine();
	ImGui::Button(ICON_FA_STOP, buttonSize);
	ImGui::EndGroup();
	ImGui::PopStyleColor();
}

void ImGuiManager::render()
{
	for (const auto& widget : widgets) {
		widget->render();
	}
	menu.render();
	leftSidebar.render();
	rightSidebar.render();
	console.render();
}

//ImGui
void ImGuiManager::end()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow* backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}
}

int ImGuiManager::onClose()
{
	if (AppWindow::platform == RenderPlatform::OPENGL) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
	return 0;
}

void ImGuiManager::setTheme(bool darkTheme)
{
	this->darkTheme = darkTheme;
}

void ImGuiManager::useLightTheme()
{
	ImGuiThemes::purpleTheme();
}

void ImGuiManager::useDarkTheme()
{
	ImGuiThemes::darkTheme();

	auto& colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Tab] = ImVec4(1.000f, 0.684f, 0.000f, 0.208f);
	colors[ImGuiCol_TabHovered] = ImVec4(1.000f, 0.682f, 0.000f, 0.549f);
	colors[ImGuiCol_TabActive] = ImVec4(1.000f, 0.682f, 0.000f, 0.549f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.853f, 0.567f, 0.000f, 0.668f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.506f, 0.337f, 0.000f, 0.522f);
	colors[ImGuiCol_CheckMark] = ImVec4(1.000f, 0.782f, 0.000f, 1.000f);
	colors[ImGuiCol_Header] = ImVec4(0.267f, 0.267f, 0.267f, 0.681f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.316f, 0.316f, 0.316f, 0.360f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.184f, 0.184f, 0.184f, 1.000f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.163f, 0.163f, 0.163f, 0.000f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.338f, 0.338f, 0.338f, 0.540f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.281f, 0.281f, 0.281f, 0.540f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.078f, 0.078f, 0.078f, 0.540f);
	colors[ImGuiCol_Border] = ImVec4(0.212f, 0.212f, 0.212f, 1.000f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.137f, 0.137f, 0.137f, 1.000f);
	//colors[ImGuiCol_TitleBgActive] = ImVec4(1.000f, 0.682f, 0.000f, 0.100f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.137f, 0.137f, 0.137f, 1.000f);
	colors[ImGuiCol_Button] = ImVec4(0.303f, 0.303f, 0.303f, 0.540f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.379f, 0.379f, 0.379f, 0.540f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.148f, 0.148f, 0.148f, 1.000f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.148f, 0.148f, 0.148f, 1.000f);
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

	bool res = ImGuizmo::Manipulate(
		v,
		p,
		(ImGuizmo::OPERATION)GuizmoType,
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
		GuizmoActive = true;
		glm::vec3 translation, rotation, scale;
		Utils::Math::DecomposeTransform(transform, translation, rotation, scale);	// graphics utils dependency, resolve when have time
		glm::vec3 deltaRotation = rotation - transformComponent.rotateVec;

		transformComponent.translateVec = translation;
		transformComponent.rotateVec += deltaRotation;
		transformComponent.scaleVec = scale;
	}
	else {
		GuizmoActive = false;
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
