#include "ParticleDemo.h"
#include <core/features/Timer.h>
#include <core/scene/SceneManager.h>
#include <gui/framework/ImGui/ImGuiManager.h>
#include <src/window/AppWindow.h>
#include <src/core/events/EventManager.h>
#include <src/core/layers/LayerManager.h>
#include <src/core/features/Camera.h>

ParticleDemo::ParticleDemo(const std::string& name) : AppLayer(name)
{
    particleRenderer.init(particleControl);
}

void ParticleDemo::onAttach(LayerManager* manager)
{
    AppLayer::onAttach(manager);

    if (!SceneManager::cameraController) {
        camera = std::make_unique<Camera>();
        camera->init(AppWindow::getWidth(), AppWindow::getHeight(), glm::vec3(1.0, 0.0, 0.0), glm::vec3(1.0));
        SceneManager::cameraController = camera.get();
    }

    LayerManager::addFrameBuffer("ParticleDemo", applicationFBO);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	EventManager& eventManager = EventManager::getInstance();
}

void ParticleDemo::onDetach()
{
    AppLayer::onDetach();
}

void ParticleDemo::onUpdate()
{
    glEnable(GL_DEPTH_TEST);
    AppLayer::onUpdate();
    ShaderOpenGL lightShader("Shaders/light.vert", "Shaders/light.frag");
    ShaderOpenGL particleShader("Shaders/particle.vert", "Shaders/particle.frag");
    ShaderOpenGL renderScene("Shaders/postProcess/renderQuad.vert", "Shaders/postProcess/renderQuad.frag");

    applicationFBO.Bind();
    glViewport(0.0, 0.0, AppWindow::getWidth(), AppWindow::getHeight());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // RGBA
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    particleRenderer.render(particleShader, SceneManager::cameraController, numRender, speed, pause);
    //skybox->render(camera);
    applicationFBO.Unbind();
}

void ParticleDemo::onGuiUpdate()
{
    AppLayer::onGuiUpdate();
    ImGui::Begin("control");
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

void ParticleDemo::onEvent(Event& event)
{
    AppLayer::onEvent(event);
}

