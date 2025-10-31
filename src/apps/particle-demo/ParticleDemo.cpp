#include "ParticleDemo.h"
#include "../../core/features/Timer.h"
#include "../../core/scene/SceneManager.h"
#include "../../gui/framework/ImGui/ImGuiController.h"
#include "camera.h"

ParticleDemo::ParticleDemo(const std::string& name) : AppLayer(name)
{
    particleRenderer.init(particleControl);
}

void ParticleDemo::OnAttach()
{
    AppLayer::OnAttach();
    //SceneManager::cameraController = &camera;
    //camera = *SceneManager::cameraController;
    //EventManager& eventManager = EventManager::getInstance();

    LayerManager::addFrameBuffer("ParticleDemo", applicationFBO);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	EventManager& eventManager = EventManager::getInstance();
}

void ParticleDemo::OnDetach()
{
    AppLayer::OnDetach();
}

void ParticleDemo::OnUpdate()
{
    glEnable(GL_DEPTH_TEST);
    AppLayer::OnUpdate();
    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
    Shader particleShader("Shaders/particle.vert", "Shaders/particle.frag");
    Shader renderScene("Shaders/postProcess/renderQuad.vert", "Shaders/postProcess/renderQuad.frag");

    applicationFBO.Bind();
    glViewport(0.0, 0.0, AppWindow::width, AppWindow::height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // RGBA
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    particleRenderer.render(particleShader, SceneManager::cameraController, numRender, speed, pause);
    //skybox->render(camera);
    applicationFBO.Unbind();
}

void ParticleDemo::OnGuiUpdate()
{
    AppLayer::OnGuiUpdate();
    if (ImGui::Begin("control")) {
        ImGui::BeginChild("gBuffers textures");
        ImVec2 wsize = ImGui::GetWindowSize();
        ImGui::DragFloat("Falling speed", &speed, 0.01, -10.0, 10.0);
        ImGui::DragInt("Num Instances", &numRender, particleControl.numInstances / 100.0, 0, particleControl.numInstances, 0, true);
        if (ImGui::DragFloat3("Spawn Area", glm::value_ptr(particleControl.spawnArea), 0.1, 0, 1000.0, 0, true)) {
            particleRenderer.clear();
            particleRenderer.init(particleControl);
            //if (isPopulating) {
            //    Console::println("populate");
            //    particleRenderer.matrixModels = matrixModels;
            //}
        }
        ImGui::Checkbox("Pause", &pause);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            particleRenderer.reset();
        }
        ImGui::EndChild();
        ImGui::End();
    }
}

void ParticleDemo::OnEvent(Event& event)
{
    AppLayer::OnEvent(event);
}

int ParticleDemo::show_demo()
{
    ParticleDemo* demo = new ParticleDemo("ParticlDemo");

    int width = AppWindow::width;
    int height = AppWindow::height;
    GLFWwindow* window = AppWindow::window;
    Camera camera(width, height, glm::vec3(-3.5f, 1.5f, 5.5f), glm::vec3(0.5, -0.2, -1.0f));

    float frameCounter = 0.0f;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    int firstFrame = 0;
    int frameIndex = 1;
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO;
    
    ImGuiController guiController;
    bool guiOn = true;
    if (guiOn)
        guiController.init(window, width, height);

    glm::vec3 particleSize(0.1, 0.1, 0.1);

    SkyboxRenderer skybox;
    FrameBuffer applicationFBO(width, height, GL_RGBA16F);


    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);

    float velocity = 0.0;
    while (!glfwWindowShouldClose(window)) {
        camera.onUpdate();
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;

        frameCounter++;
        if (deltaTime >= 1 / 2) {
            std::string FPS = std::to_string((1.0 / deltaTime) * frameCounter);
            std::string ms = std::to_string((deltaTime / frameCounter) * 1000);
            std::string updatedTitle = "Deferred Shading Demo - " + FPS + "FPS / " + ms + "ms";
            glfwSetWindowTitle(window, updatedTitle.c_str());
            lastFrame = currentFrame;
            frameCounter = 0;
        }


        Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
        Shader particleShader("Shaders/particle.vert", "Shaders/particle.frag");
        Shader renderScene("Shaders/postProcess/renderQuad.vert", "Shaders/postProcess/renderQuad.frag");
        
        applicationFBO.Bind();

        glViewport(0.0, 0.0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        demo->particleRenderer.render(particleShader, &camera, demo->numRender, demo->speed, demo->pause);

        skybox.render(&camera);

        applicationFBO.Unbind();

        if (guiOn) {
            guiController.start();

            if (ImGui::Begin("control")) {
                ImGui::BeginChild("gBuffers textures");
                ImVec2 wsize = ImGui::GetWindowSize();
                ImGui::DragFloat("Falling speed", &demo->speed, 0.01, -10.0, 10.0);
                ImGui::DragInt("Num Instances", &demo->numRender, demo->particleControl.numInstances/100.0, 0, demo->particleControl.numInstances, 0, true);
                if (ImGui::DragFloat3("Spawn Area", glm::value_ptr(demo->particleControl.spawnArea), 0.1, 0, 1000.0, 0, true)) {
                    demo->particleRenderer.clear();
                    demo->particleRenderer.init(demo->particleControl);
                    //if (isPopulating) {
                    //    Console::println("populate");
                    //    particleRenderer.matrixModels = matrixModels;
                    //}
                }
                ImGui::Checkbox("Pause", &demo->pause);
                ImGui::SameLine();
                if (ImGui::Button("Reset")) {
                    demo->particleRenderer.reset();
                }
                ImGui::EndChild();
                ImGui::End();
            }

            if (ImGui::Begin("Application window")) {
                ImGui::BeginChild("Child");
                ImVec2 wsize = ImGui::GetWindowSize();
                int wWidth = static_cast<int>(ImGui::GetWindowWidth());
                int wHeight = static_cast<int>(ImGui::GetWindowHeight());
                camera.updateViewResize(wWidth, wHeight);
                ImGui::Image((ImTextureID)applicationFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
                if (firstFrame <= 1) {
                    camera.processInput(window);
                    firstFrame++;
                }
                else if (ImGui::IsItemHovered()) {
                    camera.processInput(window);
                }
                ImGui::EndChild();
                ImGui::End();
            }

            guiController.end();
        }
        else {
            renderScene.Activate();
            renderScene.setInt("scene", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, applicationFBO.texture);
            Utils::OpenGL::Draw::drawQuad();
            camera.processInput(window);
        }
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    delete demo;
    return 0;
}

int ParticleDemo::run()
{
    return show_demo();
}
