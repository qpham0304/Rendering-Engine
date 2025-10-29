#include "BasicRayTracing.h"

struct MaterialM {
    glm::vec3 albedo;
    float metallic;
    float roughness;
    // glm::vec3 normal;
    // glm::vec3 ao;
    float emissive;
};

struct Sphere {
    glm::vec3 position;
    float radius;
    int materialIndex;

    Sphere(glm::vec3 position, float radius, int index) 
        : position(position), radius(radius), materialIndex(index) {
    }
};

int BasicRayTracing::show_demo()
{
    int width = AppWindow::width;
    int height = AppWindow::height;
    Camera camera(width, height, glm::vec3(3.0f, 3.5f, 8.0f), glm::vec3(0.0, -0.3, -1.0f));
    ImGuiController guiController;
    bool guiOn = true;
    if (guiOn)
        guiController.init(AppWindow::window, width, height);

    float frameCounter = 0.0f;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    int firstFrame = 0;
    int numBounces = 5;
    bool accumulateEnabled = true;
    bool skyboxEnabled = false;
    int frameIndex = 1;

    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
    Shader renderScene("Shaders/postProcess/renderQuad.vert", "Shaders/postProcess/renderQuad.frag");

    SkyboxRenderer skybox;

    FrameBuffer rayTracingFBO(width, height);
    FrameBuffer colorSceneFBO(width, height);
    FrameBuffer finalSceneFBO(width, height);

    std::vector<Light> lights;
    lights.push_back(Light(glm::vec3(2.0, 0.5, -30.0), glm::vec4(1000.0f, 1000.0f, 3000.0f, 1.0f)));
    lights.push_back(Light(glm::vec3(-2.0, 0.5, 2.0), glm::vec4(200.0f, 100.0f, 100.0f, 1.0f)));

    std::vector<MaterialM> materials;
    materials.push_back({ glm::vec3(0.8, 0.5, 1.0), 0.5, 1.0, 0.3 });
    materials.push_back({ glm::vec3(0.0, 1.0, 1.0), 0.11, 0.06, 0.1 });
    materials.push_back({ glm::vec3(1.0, 1.0, 0.0), 0.4, 1.0, 0.3 });
    materials.push_back({ glm::vec3(201/255.0, 175/255.0, 144/255.0), 0.6, 0.013, 0.25 });
    materials.push_back({ glm::vec3(0.3, 0.3, 0.3), 0.3, 0.3, 0.5 });

    std::vector<Sphere> spheres;
    spheres.push_back(Sphere(glm::vec3( 0.0, 1.0, 0.0), 1.0, 0));
    spheres.push_back(Sphere(glm::vec3( 4.0, 1.0, 0.0), 1.0, 1));
    spheres.push_back(Sphere(glm::vec3( 6.0, 2.0, 0.0), 1.0, 2));
    spheres.push_back(Sphere(glm::vec3( 2.439, 1.9, -31.1), 25.0, 3));
    spheres.push_back(Sphere(glm::vec3( 2.5, -200.0, 0.0), 200.0, 4));

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    while (!glfwWindowShouldClose(AppWindow::window)) {

        camera.onUpdate();
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;

        frameCounter++;
        if (deltaTime >= 1 / 2) {
            std::string FPS = std::to_string((1.0 / deltaTime) * frameCounter);
            std::string ms = std::to_string((deltaTime / frameCounter) * 1000);
            std::string updatedTitle = "Path Traced Demo - " + FPS + "FPS / " + ms + "ms";
            glfwSetWindowTitle(AppWindow::window, updatedTitle.c_str());
            lastFrame = currentFrame;
            frameCounter = 0;
        }

        colorSceneFBO.Bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        skybox.render(camera);
        colorSceneFBO.Unbind();

        rayTracingFBO.Bind();
        !accumulateEnabled || camera.isMoving() ? frameIndex = 1 : frameIndex++;
        if(frameIndex == 1) {
            glViewport(0, 0, AppWindow::width, AppWindow::height);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        Shader shader("Shaders/postProcess/renderQuad.vert", "src/apps/ray-tracing/rayTracer.frag");
        shader.Activate();

        shader.setInt("colorScene", 0);
        shader.setInt("envMap", 1);
        shader.setInt("prevScene", 2);
        shader.setVec3("camPos", camera.getPosition());
        shader.setMat4("inViewMat", glm::inverse(camera.getViewMatrix()));
        shader.setMat4("inProjMat", glm::inverse(camera.getProjectionMatrix()));
        shader.setVec2("iResolution", glm::vec2(camera.getViewWidth(), camera.getViewHeight()));
        for (int i = 0; i < spheres.size(); i++) {
            shader.setVec3("spheres[" + std::to_string(i) + "].position", spheres[i].position);
            shader.setVec3("spheres[" + std::to_string(i) + "].material.albedo", materials[i].albedo);
            shader.setFloat("spheres[" + std::to_string(i) + "].material.metallic", materials[i].metallic);
            shader.setFloat("spheres[" + std::to_string(i) + "].material.roughness", materials[i].roughness);
            shader.setFloat("spheres[" + std::to_string(i) + "].material.emissive", materials[i].emissive);
            shader.setFloat("spheres[" + std::to_string(i) + "].radius", spheres[i].radius);
        }
        shader.setFloat("iTime", currentFrame);
        shader.setInt("numBounces", numBounces);
        shader.setInt("frameIndex", frameIndex);
        shader.setInt("skyboxEnabled", skyboxEnabled);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorSceneFBO.texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.getTextureID());
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, finalSceneFBO.texture);
        Utils::OpenGL::Draw::drawQuad();

        rayTracingFBO.Unbind();

        finalSceneFBO.Bind();
        glViewport(0, 0, AppWindow::width, AppWindow::height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Shader accumulateShader("Shaders/postProcess/renderQuad.vert", "src/apps/ray-tracing/accumulateScene.frag");
        accumulateShader.Activate();
        accumulateShader.setInt("prevScene", 0);
        accumulateShader.setInt("rayTracedScene", 1);
        accumulateShader.setBool("accumulateEnabled", accumulateEnabled);
        accumulateShader.setInt("frameIndex", frameIndex);
        accumulateShader.setInt("itime", currentFrame);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, finalSceneFBO.texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rayTracingFBO.texture);
        Utils::OpenGL::Draw::drawQuad();
        finalSceneFBO.Unbind();


        if (guiOn) {
            guiController.start();

            if (ImGui::Begin("control")) {
                ImGui::BeginChild("gBuffers textures");
                ImGui::Checkbox("Accumulate Frame ", &accumulateEnabled);
                ImGui::SameLine();
                ImGui::Checkbox("Enable Skybox", &skyboxEnabled);
                ImVec2 wsize = ImGui::GetWindowSize();
                ImGui::DragInt("Number of bounces", &numBounces, 0.05f, 0.0f, 100.0f, 0, true);
                ImGui::Separator();
                for (int i = 0; i < spheres.size(); i++) {
                    if (ImGui::TreeNodeEx(std::to_string(i).c_str())) {
                        ImGui::SliderFloat3("Position", glm::value_ptr(spheres[i].position), -200.0f, 200.0f, 0);
                        ImGui::SliderFloat("Radius", &spheres[i].radius, 0.0f, 200.0f, 0);
                        ImGui::ColorEdit3("Albedo", glm::value_ptr(materials[i].albedo));
                        ImGui::SliderFloat("Metallic", &materials[i].metallic, 0.0, 1.0);
                        ImGui::SliderFloat("Roughness", &materials[i].roughness, 0.0, 1.0);
                        ImGui::SliderFloat("Emissive", &materials[i].emissive, 0.0, 10.0);
                        ImGui::TreePop();
                    }
                }
                //ImGui::Image((ImTextureID)rayTracingFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::EndChild();
                ImGui::End();
            }

            if (ImGui::Begin("Application window")) {
                ImGui::BeginChild("Child");
                ImVec2 wsize = ImGui::GetWindowSize();
                int wWidth = static_cast<int>(ImGui::GetWindowWidth());
                int wHeight = static_cast<int>(ImGui::GetWindowHeight());
                camera.updateViewResize(wWidth, wHeight);
                ImGui::Image((ImTextureID)finalSceneFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
                if (firstFrame <= 1) {
                    camera.processInput(AppWindow::window);
                    firstFrame++;
                }
                else if (ImGui::IsItemHovered()) {
                    camera.processInput(AppWindow::window);
                }
                ImGui::EndChild();
                ImGui::End();
            }

            guiController.end();
        }
        else {
            glViewport(0, 0, AppWindow::width, AppWindow::height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderScene.Activate();
            renderScene.setInt("scene", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, finalSceneFBO.texture);
            Utils::OpenGL::Draw::drawQuad();
            camera.processInput(AppWindow::window);
        }

        glfwPollEvents();
        glfwSwapBuffers(AppWindow::window);
    }

    return 0;
}

int BasicRayTracing::run()
{
    return show_demo();
}
