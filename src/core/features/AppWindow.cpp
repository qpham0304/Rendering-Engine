#include "AppWindow.h"
#include "Timer.h"
#include "../../events/Event.h"
#include "../../events/EventManager.h"
#include <camera.h>
#include "../../core/scene/SceneManager.h"

unsigned int AppWindow::width = DEFAULT_WIDTH;
unsigned int AppWindow::height = DEFAULT_HEIGHT;
bool AppWindow::VsyncEnabled = false;

ImGuizmo::OPERATION AppWindow::GuizmoType = ImGuizmo::TRANSLATE;
Platform AppWindow::platform = PLATFORM_UNDEFINED;
const std::set<Platform> AppWindow::supportPlatform = { PLATFORM_OPENGL };	// add more platform when we support more

glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
glm::vec3 lightPos = glm::vec3(0.5f, 4.5f, 5.5f);
glm::vec4 bgColor(38/255.0f, 43/255.0f, 42/255.0f, 1.0f);

GLFWwindow* AppWindow::window = nullptr;
GLFWwindow* AppWindow::sharedWindow = nullptr;
ImGuiController AppWindow::guiController(true);

bool show_debug_window = true;
bool face_culling_enabled = true;
bool draw_frame_buffer = true;
bool debug = false;
bool animate_enable = true;
bool show_navigator = true;
bool show_post_processing = false;
bool drawCube_enabled = false;
bool drawGrid_enabled = false;
bool configToggle = false;
bool renderShadow = true;
bool render_skybox = true;
int cameraSpeed = 2;
float lf = 0.0f;
bool enablefog = false;
float explodeRadius = 0.0f;
bool enableTencil = true;
UniformProperties uniforms = UniformProperties(enablefog, explodeRadius);
std::string id = "";

void getUniformBlockSize() {
	GLint maxUniformBlockSize;
	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
	std::cout << "Max Uniform Block Size: " << maxUniformBlockSize << std::endl;
}

void AppWindow::renderGuizmo(Component& component, const bool drawCube, const bool drawGrid) {
	glm::vec3 translateVector(0.0f, 0.0f, 0.0f);
	glm::vec3 scaleVector(1.0f, 1.0f, 1.0f);

	float viewManipulateRight = ImGui::GetIO().DisplaySize.x;
	float viewManipulateTop = 0;

	auto v = &SceneManager::cameraController->getViewMatrix()[0][0];
	auto p = glm::value_ptr(SceneManager::cameraController->getProjectionMatrix());
	glm::mat4 transform = component.getModelMatrix();
	glm::vec3 originRotation = component.rotationVector;

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	float wd = (float)ImGui::GetWindowWidth();
	float wh = (float)ImGui::GetWindowHeight();
	glm::mat4 modelMat = component.getModelMatrix();
	ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, wd, wh);
	glm::mat4 identity(1.0f);

	if(drawCube)
		ImGuizmo::DrawCubes(v, p, glm::value_ptr(transform), 1);
	if(drawGrid)
		ImGuizmo::DrawGrid(v, p, glm::value_ptr(identity), 100.f);
	
	bool res = ImGuizmo::Manipulate(v, p, (ImGuizmo::OPERATION)GuizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform));
	viewManipulateRight = ImGui::GetWindowPos().x + wd;
	viewManipulateTop = ImGui::GetWindowPos().y;
	ImGuizmo::ViewManipulate(v, 5.0f, ImVec2(viewManipulateRight - 128, viewManipulateTop), ImVec2(128, 128), 0x10101010);
	
	if (ImGuizmo::IsUsing()) {
		glm::vec3 translation, rotation, scale;
		Utils::Math::DecomposeTransform(transform, translation, rotation, scale);
		glm::vec3 deltaRotation = rotation - originRotation;
		translateVector = glm::vec3(transform[3]);
		component.rotationVector += deltaRotation;
		component.scaleVector = scale;
		component.translate(translateVector);
		component.rotate(deltaRotation);
	}
}

int AppWindow::init(Platform platform) {
	AppWindow::platform = platform;
	if (supportPlatform.find(platform)  == supportPlatform.end()) {
		throw std::runtime_error("Platform unsupported");
	}

	if (AppWindow::platform == PLATFORM_OPENGL) {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		// Initialize GLFW
		if (!glfwInit())
			return -1;

		// Get primary monitor
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		if (!monitor) {
			std::cerr << "Failed to get primary monitor" << std::endl;
			glfwTerminate();
			return -1;
		}

		// Get monitor video mode
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		if (!mode) {
			std::cerr << "Failed to get video mode" << std::endl;
			glfwTerminate();
			return -1;
		}

		width = mode->width * 2 / 3;
		height = mode->height * 2 / 3;
	}

	return 0;
}

int AppWindow::start(const char* title) {
	if (platform == PLATFORM_UNDEFINED) {
		throw std::runtime_error("Platform not specified have you called init() with supported platform yet?");
	}

	else if (platform == PLATFORM_OPENGL) {
		window = glfwCreateWindow(width, height, title, NULL, NULL);

		if (window == NULL) {
			std::cerr << "Failed to create GLFW window" << std::endl;
			glfwTerminate();
			return -1;
		}
		glfwMakeContextCurrent(window);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			std::cerr << "Failed to initialize GLAD for main context" << std::endl;
			glfwDestroyWindow(window);
			glfwTerminate();
			return -1;
		}

		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);	// Set Invisible for second context
		sharedWindow = glfwCreateWindow(width, height, "", NULL, window);

		if (sharedWindow == NULL) {
			std::cerr << "Failed to create shared GLFW window" << std::endl;
			glfwDestroyWindow(window);
			glfwTerminate();
			return -1;
		}
		glfwMakeContextCurrent(sharedWindow);

		glfwMakeContextCurrent(window);
	}
	setEventCallback();

	glfwSwapInterval(1);
	//glfwSwapInterval(0);
	return 0;
}

int AppWindow::end() {
	if (platform == PLATFORM_OPENGL) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwMakeContextCurrent(nullptr);
		glfwDestroyWindow(window);
		glfwDestroyWindow(sharedWindow);
		glfwTerminate();
	}
	return 0;
}

void AppWindow::setEventCallback()
{
	glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y)
		{
			Timer timer("cursor event", true);
			MouseMoveEvent cursorMoveEvent(window, x, y);
			EventManager::getInstance().Publish(cursorMoveEvent);
		});

	glfwSetScrollCallback(window, [](GLFWwindow* window, double x, double y)
		{
			Timer timer("scroll event", true);
			MouseScrollEvent scrollEvent(window, x, y);
			EventManager::getInstance().Publish(scrollEvent);
			//EventManager::getInstance().Publish("mouseScrollEvent", x, y);
		});

	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			//Application* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
			switch (action) {
				case GLFW_PRESS: {
					if (key == KEY_ESCAPE) {
						WindowCloseEvent windowCloseEvent;
						EventManager::getInstance().Publish(windowCloseEvent);
					}
					else {
						KeyPressedEvent keyPressEvent(key);
						EventManager::getInstance().Publish(keyPressEvent);
					}
				}
				case GLFW_RELEASE: {
					//Console::println("released");
				}
				case GLFW_REPEAT: {
					//Console::println("key repeat");
				}
			}
		});

	glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height)
		{
			WindowResizeEvent resizeEvent(window, width, height);
			EventManager::getInstance().Publish(resizeEvent);
		});

	glfwSetWindowCloseCallback(window, [](GLFWwindow* window)
		{
			WindowCloseEvent closeEvent;
			EventManager::getInstance().Publish(closeEvent);
		});
}

void AppWindow::onUpdate()
{
	if (AppWindow::platform == PLATFORM_OPENGL) {
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
}


void AppWindow::renderShadowScene(DepthMap& shadowMap, Shader& shadowMapShader, Light& light) {
	shadowMap.Bind();
	glViewport(0, 0, shadowMap.getWidth(), shadowMap.getHeight());
	glClear(GL_DEPTH_BUFFER_BIT);
	glActiveTexture(GL_TEXTURE0);

	if (face_culling_enabled) {
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
		glCullFace(GL_FRONT);
	}
	else {
		glDisable(GL_CULL_FACE);
	}

	SceneManager::renderShadow(shadowMapShader, light);

	shadowMap.Unbind();
}

void AppWindow::renderObjectsScene(FrameBuffer& framebuffer, DepthMap& depthMap, std::vector<Light> lights, unsigned int depthMapPoint) {
	framebuffer.Bind();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_DEPTH_CLAMP);
	if (enableTencil) {
		glEnable(GL_STENCIL_TEST);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	}

	if (face_culling_enabled) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
	}
	else
		glDisable(GL_CULL_FACE);

	if (configToggle) {
		glEnable(GL_BLEND);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// reset viewport
	glViewport(0, 0, width, height);
	glClearColor(bgColor.x, bgColor.y, bgColor.z, bgColor.w); // RGBA
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, depthMap.texture);
	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_CUBE_MAP, depthMapPoint);

	float currentTime = static_cast<float>(glfwGetTime());
	float dt = currentTime - lf;
	lf = currentTime;
	if (animate_enable && !id.empty()) {
		if (SceneManager::getComponent(id) != nullptr) {
			SceneManager::getComponent(id)->updateAnimation(dt);
		}
	}

	//glStencilFunc(GL_ALWAYS, 1, 0xFF);
	//glStencilMask(0xFF);
	SceneManager::render(lights, uniforms);
	//glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	//glStencilMask(0x00);
	//glDisable(GL_DEPTH_TEST);

	framebuffer.Unbind();
}

int AppWindow::renderScene() 
{
	guiController.init(window, width, height);
	Camera camera(width, height, glm::vec3(-6.5f, 3.5f, 8.5f), glm::vec3(0.5f, -0.2f, -1.0f));
	SceneManager::cameraController = &camera;
	
	SkyboxRenderer skybox("Textures/skybox");
	skybox.setUniform();

	glm::vec3 translate(5.0f, 0.0f, 2.0f);
	glm::vec3 scaleVector(0.5f, 0.5f, 0.5f);
	std::string cubeID = SceneManager::addComponent("Models/planet/planet.obj");
	SceneManager::getComponent(cubeID)->translate(translate);
	std::string reimuID = SceneManager::addComponent("Models/reimu/reimu.obj");
	translate += glm::vec3(0.0f, 3.0f, 0.0f);
	SceneManager::getComponent(reimuID)->translate(translate);
	SceneManager::getComponent(reimuID)->scale(scaleVector);
	id = SceneManager::addComponent("Models/aru/aru.gltf");
	SceneManager::getComponent(id)->loadAnimation("Models/aru/aru.gltf");
	translate += glm::vec3(-9.0f, 1.0f, 2.0f);
	SceneManager::getComponent(id)->translate(translate);

	Shader shadowMapShader("Shaders/shadowMap.vert", "Shaders/shadowMap.frag");
	Shader pointShadowShader("Shaders/shadow/pointShadowsDepth.vert", 
							"Shaders/shadow/pointShadowsDepth.frag", 
							"Shaders/shadow/pointShadowsDepth.geom");
	Shader debugDepthQuad("src/apps/shadow-map/debug.vert", "src/apps/shadow-map/debug.frag");

	DepthMap depthMap;
	FrameBuffer framebuffer(width, height);
	FrameBuffer postRenderFrame(width, height);

	Shader frameShaderProgram("src/apps/frame-buffer/framebuffer.vert", "src/apps/frame-buffer/framebuffer.frag");
	frameShaderProgram.Activate();
	frameShaderProgram.setFloat("screenTexture", 0);

	float near_plane = 1.00f, far_plane = 25.0f;
	glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
	glm::mat4 lightMVP;
	glm::mat4 lightView;
	debugDepthQuad.Activate();
	debugDepthQuad.setInt("depthMap", 0);

	glm::vec3 lightAmbient = glm::vec3(0.5f, 0.5f, 0.5f);
	glm::vec3 lightDiffuse = glm::vec3(0.5f, 0.5f, 0.5f);
	glm::vec3 lightSpecular = glm::vec3(1.0f, 1.0f, 1.0f);
	//Light light = Light(lightPos, lightColor, lightAmbient, lightDiffuse, lightSpecular, lightMVP, 2);

	Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
	Model light1("Models/Planet/planet.obj");
	glm::vec3 lightPos_1 = glm::vec3(0.0f, 0.0f, 0.0f);
	Light l1 = Light(lightPos_1, lightColor);

	std::vector<Light> lights = { Light(lightPos, lightColor, lightAmbient, lightDiffuse, lightSpecular, lightMVP, 2), l1 };
	//lightPos = lights[0].position;
	lightView = glm::lookAt(lights[0].position, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));

	DepthCubeMap depthCubeMap;

	// Main while loop
	glEnable(GL_DEPTH_TEST);
	while (!glfwWindowShouldClose(window))
	{
		guiController.start();
		guiController.render();

		uniforms = UniformProperties(enablefog, explodeRadius);

		lightMVP = lightProjection * lightView;
		lights[0].mvp = lightMVP;



		// Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		if (ImGui::Begin("Global Control")) {
			if(ImGui::SliderInt("Camera speed", &cameraSpeed, 1, 10))
				SceneManager::cameraController->setCameraSpeed(cameraSpeed);
			//ImGui::SliderFloat3("Light position", &light.position[0], -20.0f, 20.0f);
			ImGui::SliderFloat3("Direcion Light position", &lights[0].position[0], -20.0f, 20.0f);
			ImGui::SliderFloat3("Point Light position", &lights[1].position[0], -20.0f, 20.0f);
			ImGui::SliderFloat3("ambient", &lights[0].ambient[0], 0.0f, 1.0f);
			ImGui::SliderFloat3("diffuse", &lights[0].diffuse[0], 0.0f, 1.0f);
			ImGui::SliderFloat3("specular", &lights[0].specular[0], 0.0f, 1.0f);
			ImGui::SliderFloat("explode radius", &explodeRadius, 0, 10.f);

			if (ImGui::Button("+"))
				lights[0].sampleRadius++;
			ImGui::SameLine();
			if (ImGui::Button("-") && lights[0].sampleRadius >= 0)
				lights[0].sampleRadius--;
			ImGui::SameLine();
			ImGui::Text("counter = %d", lights[0].sampleRadius);
			ImGui::Checkbox("Debug Mode", &debug);
			ImGui::SameLine();
			ImGui::Checkbox("Show debug window", &show_debug_window);
			ImGui::ColorEdit4("Light Color", &lights[0].color[0]);
			ImGui::ColorEdit4("Background Color", &bgColor[0]);
			ImGui::Checkbox("Enable face culling", &face_culling_enabled);
			ImGui::SameLine();
			ImGui::Checkbox("Enable animation", &animate_enable);
			ImGui::Checkbox("Show navigator", &show_navigator);
			ImGui::SameLine();
			ImGui::Checkbox("Draw Debug Grid", &drawGrid_enabled);
			ImGui::Checkbox("post frame process", &show_post_processing);
			ImGui::SameLine();
			ImGui::Checkbox("toggle config", &configToggle);
			ImGui::Checkbox("render skybox", &render_skybox);
			ImGui::SameLine();
			ImGui::Checkbox("enable fog", &enablefog);
			ImGui::SameLine();
			ImGui::Checkbox("enable tencil", &enableTencil);
			ImGui::Checkbox("Gamma Correction", &SceneManager::gammaCorrection);
			ImGui::End();
		}


		SceneManager::cameraController->onUpdate();
		glm::mat4 pointLightProjection = glm::perspective(glm::radians(90.0f), float(depthCubeMap.SHADOW_WIDTH) / float(depthCubeMap.SHADOW_HEIGHT), near_plane, far_plane * 2);
		std::vector<glm::mat4> pointShadowMVP;
		pointShadowMVP.push_back(pointLightProjection * glm::lookAt(lights[1].position, lights[1].position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		pointShadowMVP.push_back(pointLightProjection * glm::lookAt(lights[1].position, lights[1].position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		pointShadowMVP.push_back(pointLightProjection * glm::lookAt(lights[1].position, lights[1].position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
		pointShadowMVP.push_back(pointLightProjection * glm::lookAt(lights[1].position, lights[1].position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
		pointShadowMVP.push_back(pointLightProjection * glm::lookAt(lights[1].position, lights[1].position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		pointShadowMVP.push_back(pointLightProjection * glm::lookAt(lights[1].position, lights[1].position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

		depthCubeMap.Bind();
		glViewport(0, 0, depthCubeMap.SHADOW_WIDTH, depthCubeMap.SHADOW_HEIGHT);
		glClear(GL_DEPTH_BUFFER_BIT);
		pointShadowShader.Activate();
		for (unsigned int i = 0; i < 6; ++i)
			pointShadowShader.setMat4("shadowMatrices[" + std::to_string(i) + "]", pointShadowMVP[i]);
		pointShadowShader.setFloat("far_plane", far_plane * 2);
		pointShadowShader.setVec3("lightPos", lights[1].position);
		SceneManager::renderShadow(pointShadowShader, lights[1]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubeMap.texture);
		depthCubeMap.Unbind();
		renderShadowScene(depthMap, shadowMapShader, lights[0]);
		
		//glActiveTexture(GL_TEXTURE5);
		//glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemapTexture);
		//renderObjectsScene(framebuffer, depthMap, light);

		renderObjectsScene(framebuffer, depthMap, lights, depthCubeMap.texture);
		
		// extra custom draw calls
		framebuffer.Bind();
		glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), lights[0].position) * glm::scale(glm::mat4(1.0f), glm::vec3(0.25f));


		if (render_skybox)
			skybox.render(*SceneManager::cameraController);
		framebuffer.Unbind();

		if(ImGui::Begin("Application Window"))
		{
			ImGui::BeginChild("View window");
			ImVec2 wsize = ImGui::GetWindowSize();
			int wWidth = static_cast<int>(ImGui::GetWindowWidth());
			int wHeight = static_cast<int>(ImGui::GetWindowHeight());
			SceneManager::cameraController->updateViewResize(wWidth, wHeight);

			if (show_post_processing) {
				postRenderFrame.Bind();
				frameShaderProgram.Activate();
				glDisable(GL_DEPTH_TEST); // prevents framebuffer rectangle from being discarded
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, framebuffer.texture);
				Utils::OpenGL::Draw::drawQuad();
				postRenderFrame.Unbind();
				ImGui::Image((ImTextureID)postRenderFrame.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
			}
			else
				ImGui::Image((ImTextureID)framebuffer.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
			if (ImGui::IsItemHovered())
				SceneManager::cameraController->processInput(window);
			if (show_navigator) {
				Component* component = SceneManager::getSelectedComponent();
				if (component != nullptr && component->isSelected() && !debug)
					renderGuizmo(*component, drawCube_enabled, drawGrid_enabled);
			}
			ImGui::SetNextItemAllowOverlap();
			guiController.applicationWindow();
			ImGui::EndChild();
		}
		ImGui::End();

		if (show_debug_window)
			guiController.debugWindow((ImTextureID) depthMap.texture);

		guiController.end();
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
		processProgramInput(window);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	return 0;

}

int AppWindow::renderScene(std::function<int()> runFunction)
{
	VsyncEnabled ? glfwSwapInterval(1) : glfwSwapInterval(0);
	runFunction();
	return 0;
}

void AppWindow::processProgramInput(GLFWwindow* window)
{
	//glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS ? debug = true : debug = false;
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
	if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
		GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
		GuizmoType = ImGuizmo::OPERATION::ROTATE;
	if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
		GuizmoType = ImGuizmo::OPERATION::SCALE;
	if (glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS)
		SceneManager::removeComponent(SceneManager::getSelectedID());
}

void AppWindow::framebuffer_size_callback(GLFWwindow* window, int w, int h)
{
	width = w;
	height = h;
	//SceneManager::cameraController->updateViewResize(width, height);
	glViewport(0, 0, width, height);
}
