#include "SSR_view_demo.h"
#include "../../graphics/renderer/deferredRenderer.h"
#include "../../core/scene/SceneManager.h"

int SSR_view_demo::show_demo()
{
    int width = AppWindow::width;
    int height = AppWindow::height;
    Camera camera(width, height, glm::vec3(-6.5f, 3.5f, 8.5f), glm::vec3(0.5, -0.2, -1.0f));
    ImGuiController guiController;
    bool guiOn = true;
    if (guiOn)
        guiController.init(AppWindow::window, width, height);

    float frameCounter = 0.0f;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;

    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");

    SkyboxRenderer skybox;
    
    FrameBuffer colorPassFBO(width, height);
    FrameBuffer ssrSceneFBO(width, height);
    FrameBuffer finalSceneFBO(width, height);

    SceneManager::addComponent("Models/backpack/backpack.obj");
    std::vector<Component*> components;
    for (const auto& pair : SceneManager::components) {
        components.push_back(pair.second.get());
    }

    std::vector<Light> lights;
    lights.push_back(Light(glm::vec3(2.0, 0.5, 2.0), glm::vec4(1.0)));
    lights.push_back(Light(glm::vec3(-2.0, 0.5, 2.0), glm::vec4(1.0)));

    unsigned int planeVAO = 0;
    unsigned int planeVBO = 0;
    Texture planeAlbedo("Textures/squish.png", "albedoMap");
    Texture planeMetallic("Textures/default/metallic.png", "metallicMap");
    Texture planeAO("Textures/default/roughness-2.png", "aoMap");

    unsigned int gBuffer;
    unsigned int gDepth, gNormal, gAlbedo, gSpecular;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gNormal, 0);

    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gAlbedo, 0);

    glGenTextures(1, &gSpecular);
    glBindTexture(GL_TEXTURE_2D, gSpecular);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gSpecular, 0);

    glGenTextures(1, &gDepth);
    glBindTexture(GL_TEXTURE_2D, gDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, gDepth, 0);

    GLenum drawBuffers1[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, drawBuffers1);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::endl;
        return -1;
    }

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    while (!glfwWindowShouldClose(AppWindow::window)) {
        camera.onUpdate();
        camera.processInput(AppWindow::window);
        
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


        glClearColor(1.0f, 0.5f, 0.0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        Shader geometryShader("Shaders/deferredShading/gBufferView.vert", "Shaders/deferredShading/gBufferView.frag");
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
        glClearColor(1.0f, 0.5f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        geometryShader.Activate();
        glm::mat4 model = glm::mat4(1.0);
        model = glm::translate(model, glm::vec3(0.0f, -2.0f, 0.0f));
        model = glm::scale(model, glm::vec3(30.0f));
        geometryShader.setInt("albedoMap", 0);
        geometryShader.setInt("metallicMap", 1.0);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, planeAlbedo.ID);
        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, planeMetallic.ID);

        geometryShader.setFloat("reflectiveMap", 1.0);
        geometryShader.setBool("invertedNormals", false);
        geometryShader.setBool("invertedTexCoords", false);
        geometryShader.setMat4("model", model);
        geometryShader.setMat4("modelViewNormal", glm::transpose(glm::inverse(camera.getViewMatrix() * model)));
        geometryShader.setMat4("mvp", camera.getMVP() * model);
        Utils::OpenGL::Draw::drawQuadNormals();


        for (unsigned int i = 0; i < components.size(); i++) {
            geometryShader.setFloat("reflectiveMap", 0.0);
            geometryShader.setBool("invertedNormals", false);
            geometryShader.setBool("invertedTexCoords", true);
            geometryShader.setMat4("model", components[i]->getModelMatrix());
            geometryShader.setMat4("modelViewNormal", glm::transpose(glm::inverse(camera.getViewMatrix() * components[i]->getModelMatrix())));
            geometryShader.setMat4("mvp", camera.getMVP() * components[i]->getModelMatrix());
            components[i]->model_ptr->Draw(geometryShader);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        Shader colorShader("Shaders/deferredShading/deferredShadingView.vert", "Shaders/deferredShading/deferredShadingView.frag");
        colorPassFBO.Bind();
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        
        colorShader.Activate();
        colorShader.setInt("gNormal", 0);
        colorShader.setInt("gAlbedo", 1);
        colorShader.setInt("gSpecular", 2);
        colorShader.setInt("depthMap", 3);
        colorShader.setInt("ssaoTex", 4);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gAlbedo);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gSpecular);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, gDepth);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, planeAO.ID);

        colorShader.setMat4("invProjection", glm::inverse(camera.getProjectionMatrix()));
        colorShader.setMat4("invView", glm::inverse(camera.getViewMatrix()));

        for (unsigned int i = 0; i < lights.size(); i++)
        {
            colorShader.setVec3("lights[" + std::to_string(i) + "].Position", lights[i].position);
            colorShader.setVec3("lights[" + std::to_string(i) + "].Color", lights[i].color);
            // update attenuation parameters and calculate radius
            const float constant = 1.0f; // note that we don't send this to the shader, we assume it is always 1.0 (in our case)
            const float linear = 0.7f;
            const float quadratic = 1.8f;
            colorShader.setFloat("lights[" + std::to_string(i) + "].Linear", linear);
            colorShader.setFloat("lights[" + std::to_string(i) + "].Quadratic", quadratic);
            // then calculate radius of light volume/sphere
            const float maxBrightness = std::fmaxf(std::fmaxf(lights[i].color.r, lights[i].color.g), lights[i].color.b);
            float radius = (-linear + std::sqrt(linear * linear - 4 * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
            colorShader.setFloat("lights[" + std::to_string(i) + "].Radius", radius);
        }
        Utils::OpenGL::Draw::drawQuad();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, colorPassFBO.FBO); // write to application framebuffer
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        lightShader.Activate();
        lightShader.setMat4("mvp", camera.getMVP());
        for (unsigned int i = 0; i < lights.size(); i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, lights[i].position);
            model = glm::scale(model, glm::vec3(0.125f));
            lightShader.setMat4("matrix", model);
            lightShader.setVec3("lightColor", lights[i].color);
            Utils::OpenGL::Draw::drawCube(cubeVAO, cubeVBO);
        }
        //skybox.render(camera);
        colorPassFBO.Unbind();

        Shader SSRShader("Shaders/SSR/SSR.vert", "Shaders/SSR/SSR.frag");
        ssrSceneFBO.Bind();
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        
        SSRShader.Activate();
        SSRShader.setInt("gNormal", 0);
        SSRShader.setInt("gAlbedo", 1);
        SSRShader.setInt("gSpecular", 2);
        SSRShader.setInt("depthMap", 3);
        SSRShader.setInt("colorBuffer", 4);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gAlbedo);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gSpecular);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, gDepth);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, colorPassFBO.texture);

        SSRShader.setMat4("view", camera.getViewMatrix());
        SSRShader.setMat4("projection", camera.getProjectionMatrix());
        SSRShader.setMat4("invView", glm::inverse(camera.getViewMatrix()));
        SSRShader.setMat4("invProjection", glm::inverse(camera.getProjectionMatrix()));
        SSRShader.setInt("width", width);
        SSRShader.setInt("height", height);
        //SSRShader.setVec2("gTexSizeInv", glm::vec2(1.0 / width, 1.0 / height));

        Utils::OpenGL::Draw::drawQuad();

        ssrSceneFBO.Unbind();

        if (guiOn) {
            guiController.start();

            if (ImGui::Begin("control")) {
                ImGui::BeginChild("gBuffers textures");
                ImVec2 wsize = ImGui::GetWindowSize();
                ImGui::Image((ImTextureID)gNormal, wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)gAlbedo , wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)gSpecular, wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)gDepth, wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)colorPassFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::EndChild();
                ImGui::End();
            }

            if (ImGui::Begin("Application window")) {
                ImGui::BeginChild("Child");
                ImVec2 wsize = ImGui::GetWindowSize();
                int wWidth = static_cast<int>(ImGui::GetWindowWidth());
                int wHeight = static_cast<int>(ImGui::GetWindowHeight());
                camera.updateViewResize(wWidth, wHeight);
                ImGui::Image((ImTextureID)ssrSceneFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
                if (ImGui::IsItemHovered())
                    camera.processInput(AppWindow::window);
                ImGui::EndChild();
                ImGui::End();
            }

            guiController.end();
        }

        glfwPollEvents();
        glfwSwapBuffers(AppWindow::window);
    }

    return 0;
}

int SSR_view_demo::run()
{
    return show_demo();
}
