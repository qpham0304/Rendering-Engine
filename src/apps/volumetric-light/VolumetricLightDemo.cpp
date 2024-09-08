#include "VolumetricLightDemo.h"
#include "../../graphics/renderer/ShadowMapRenderer.h"

int VolumetricLightDemo::show_demo() {
    float width = AppWindow::width;
    float height = AppWindow::height;

    Camera camera(width, height, glm::vec3(-6.5f, 3.5f, 8.5f), glm::vec3(0.5, -0.2, -1.0f));
    SkyboxRenderer skybox;

    bool guiOn = true;
    ImGuiController guiController;
    if (guiOn)
        guiController.init(AppWindow::window, width, height);

    Shader postProcessShader("Shaders/postProcess/renderQuad.vert", "Shaders/postProcess/renderQuad.frag");
    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
    Shader shadowMapShader("Shaders/shadowMap.vert", "Shaders/shadowMap.frag");

    Model reimu("Models/reimu/reimu.obj");
    Model sponza("Models/sponza/sponza.obj");

    FrameBuffer applicationFBO(width, height);
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;

    ShadowMapRenderer shadowMapRenderer(2048, 2048);

    glm::vec4 lightColor(1.0, 1.0, 1.0, 1.0);
    glm::vec3 lightPosition(5.0, 5.0, 5.0);
    Light light(lightPosition, lightColor);
    glm::mat4 model(1.0f);
    std::vector<glm::mat4> modelMatrices;
    std::vector<Model> models;

    int numSteps = 15;
    float G = 0.0f;
    float intensity = 1.0f;
    float scatterScale = 1.0f;

    GLuint byteSize = 32;
    GLuint ubo;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, byteSize, nullptr, GL_STATIC_DRAW);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::vec3), glm::value_ptr(lightColor), GL_STATIC_DRAW);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float), &intensity, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

    postProcessShader.Activate();
    postProcessShader.setInt("scene", 0);

    float frameCounter = 0.0f;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;

    model = glm::scale(model, glm::vec3(0.10));
    modelMatrices.push_back(model);
    models.push_back(reimu);

    model = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));
    modelMatrices.push_back(model);
    models.push_back(sponza);


    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LEQUAL);
    while (!glfwWindowShouldClose(AppWindow::window)) {
        camera.onUpdate();
        camera.setCameraSpeed(5);

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;

        frameCounter++;
        if (deltaTime >= 1 / 2) {
            std::string FPS = std::to_string((1.0 / deltaTime) * frameCounter);
            std::string ms = std::to_string((deltaTime / frameCounter) * 1000);
            std::string updatedTitle = "Deferred Shading Demo - " + FPS + "FPS / " + ms + "ms";
            glfwSetWindowTitle(AppWindow::window, updatedTitle.c_str());
            lastFrame = currentFrame;
            frameCounter = 0;
        }

        glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
        glm::mat4 lightView = glm::lookAt(light.position, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 lightMVP = lightProjection * lightView;
        light.mvp = lightMVP;

        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec3), &light.color[0]);
        glBufferSubData(GL_UNIFORM_BUFFER, 16, sizeof(float), &intensity);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        //shadowMapRenderer.renderShadow(light, models, modelMatrices);

        applicationFBO.Bind();
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
        lightShader.Activate();
        model = glm::mat4(1.0);
        lightShader.setMat4("mvp", camera.getMVP());
        model = glm::translate(model, light.position);
        model = glm::scale(model, glm::vec3(0.125f));
        lightShader.setMat4("matrix", model);
        lightShader.setVec3("lightColor", glm::vec3(light.color));
        Utils::OpenGL::Draw::drawSphere(cubeVAO, cubeVBO);
        
        Shader pbrShader("src/apps/volumetric-light/shaders/pbr.vert", "src/apps/volumetric-light/shaders/pbr.frag");
        //pbrShader.reloadShader();
        pbrShader.Activate();
        model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(0.10));
        glm::mat3 normalMatrix = model;
        pbrShader.setMat4("matrix", model);
        pbrShader.setMat4("mvp", camera.getMVP());
        pbrShader.setBool("hasAnimation", false);
        pbrShader.setMat3("normalMatrix", glm::transpose(glm::inverse(normalMatrix)));
        pbrShader.setVec3("lightPos", light.position);
        pbrShader.setVec3("camPos", camera.getPosition());
        pbrShader.setInt("SCREEN_WIDTH", width);
        pbrShader.setInt("SCREEN_HEIGHT", height);
        pbrShader.setInt("NUM_STEPS_INT", numSteps);
        pbrShader.setFloat("G", G);
        pbrShader.setFloat("intensity", intensity);
        pbrShader.setFloat("scatterScale", scatterScale);
        pbrShader.setVec3("lightColor", glm::vec3(light.color));
        pbrShader.setFloat("time", static_cast<float>(glfwGetTime()));
        pbrShader.setVec3("windDirection", glm::vec3(1.0f));
        pbrShader.setMat4("lightMVP", light.mvp);
        pbrShader.setInt("depthMap", 10);

        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, shadowMapRenderer.depthTexture());
        reimu.Draw(pbrShader);

        model = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));
        pbrShader.setMat4("matrix", model);
        //Utils::OpenGL::Draw::drawQuad();
        sponza.Draw(pbrShader);

        skybox.render(camera);
        applicationFBO.Unbind();

        if (guiOn) {
            guiController.start();
            if (ImGui::Begin("control")) {
                ImGui::SliderFloat3("Direcion Light position", &light.position[0], -100.0f, 100.0f);
                ImGui::SliderInt("Steps", &numSteps, 0, 100);
                ImGui::SliderFloat("G", &G, -1.0f, 1.0f);
                ImGui::SliderFloat("intensity", &intensity, 0.0f, 100.0f);
                ImGui::SliderFloat("scatter scale", &scatterScale, 0.0f, 100.0f);
                ImGui::ColorEdit4("Light Color", &light.color[0]);
                ImGui::BeginChild("gBuffers textures");
                ImVec2 wsize = ImGui::GetWindowSize();
                ImGui::Image((ImTextureID)shadowMapRenderer.depthTexture(), wsize, ImVec2(0, 1), ImVec2(1, 0));

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
                if (ImGui::IsItemHovered())
                    camera.processInput(AppWindow::window);
                ImGui::EndChild();
                ImGui::End();
            }
            guiController.end();
        }
        else {
            postProcessShader.Activate();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, applicationFBO.texture);
            Utils::OpenGL::Draw::drawQuad();
        }

        glfwPollEvents();
        glfwSwapBuffers(AppWindow::window);
    }
    return 0;
}

int VolumetricLightDemo::run() {
    try {
        show_demo();
        return 0;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
}