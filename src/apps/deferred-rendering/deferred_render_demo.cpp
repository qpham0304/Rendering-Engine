
#include "deferred_render_demo.h"
#include "../../graphics/renderer/deferredRenderer.h"
#include "../../graphics/renderer/BloomRenderer.h"
#include "../../core/scene/SceneManager.h"

int DeferredRender::show_demo() {
    float width = AppWindow::width;
    float height = AppWindow::height;
    
    Camera camera(width, height, glm::vec3(-6.5f, 3.5f, 8.5f), glm::vec3(0.5, -0.2, -1.0f));
    ImGuiController guiController;
    bool guiOn = true;
    if(guiOn)
        guiController.init(AppWindow::window, width, height);

    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
    Shader bloomShader("Shaders/bloom/bloom.vert", "Shaders/bloom/bloom.frag");
    Shader postProcessShader("Shaders/postProcess/renderQuad.vert", "Shaders/postProcess/renderQuad.frag");

    SkyboxRenderer skybox("Textures/night-skybox");
    SceneManager::addComponent("Models/backpack/backpack.obj");
    std::vector<Component*> components;
    for (const auto& pair : SceneManager::components) {
        components.push_back(pair.second.get());
    }

    const unsigned int NR_LIGHTS = 32;
    std::vector<Light> lights;
    srand(13);
    for (unsigned int i = 0; i < NR_LIGHTS; i++)
    {
        // calculate slightly random offsets
        float xPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0);
        float yPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 4.0);
        float zPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0);
        // also calculate random color
        float rColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.)
        float gColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.)
        float bColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.)
        lights.push_back(Light(glm::vec3(xPos, yPos, zPos), glm::vec4(rColor, gColor, bColor, 1.0)));
    }

    float frameCounter = 0.0f;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
    FrameBuffer applicationFBO(width, height);

    postProcessShader.Activate();
    postProcessShader.setInt("scene", 0);

    DeferredRenderer deferredRenderer(width, height);
    BloomRenderer bloomRenderer;
    bloomRenderer.Init(width, height);

    bloomShader.Activate();
    bloomShader.setInt("scene", 0);
    bloomShader.setInt("bloomBlur", 1);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    while (!glfwWindowShouldClose(AppWindow::window)) {
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

        glClearColor(0.0, 0.0, 0.0, 1.0); // keep it black so it doesn't leak into g-buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.getFOV()), (float)width / (float)height, 0.1f, 100.0f);
        glm::mat4 view = camera.getViewMatrix();
        //deferredRenderer.renderGeometry(camera, components);
        deferredRenderer.renderGeometry(camera, *components[0]);

        applicationFBO.Bind();
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        camera.onUpdate();
        camera.processInput(AppWindow::window);


        deferredRenderer.renderColor(camera, lights);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, deferredRenderer.getGBuffer());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, applicationFBO.FBO); // write to application framebuffer
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        lightShader.Activate();
        lightShader.setMat4("mvp", projection * view);
        for (unsigned int i = 0; i < lights.size(); i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, lights[i].position);
            model = glm::scale(model, glm::vec3(0.125f));
            lightShader.setMat4("matrix", model);
            lightShader.setVec3("lightColor", lights[i].color);
            Utils::OpenGL::Draw::drawCube(cubeVAO, cubeVBO);
        }
        
        skybox.render(camera);
        applicationFBO.Unbind();


        if (guiOn) {
            guiController.start();

            if (ImGui::Begin("control")) {
                ImGui::BeginChild("gBuffers textures");
                ImVec2 wsize = ImGui::GetWindowSize();
                ImGui::Image((ImTextureID)deferredRenderer.getGPosition(), wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)deferredRenderer.getGNormal(), wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)deferredRenderer.getGAlbedoSpec(), wsize, ImVec2(0, 1), ImVec2(1, 0));
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

        glfwPollEvents();
        glfwSwapBuffers(AppWindow::window);
    }
    return 0;
}

int bloomTest() {
    float width = AppWindow::width;
    float height = AppWindow::height;

    Camera camera(width, height, glm::vec3(-6.5f, 3.5f, 8.5f), glm::vec3(0.5, -0.2, -1.0f));
    
    bool guiOn = false;
    ImGuiController guiController;
    if(guiOn)
        guiController.init(AppWindow::window, width, height);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
    Shader modelShader("Shaders/models.vert", "Shaders/model.frag");
    Shader bloomShader("Shaders/bloom/bloom.vert", "Shaders/bloom/bloom.frag");

    SkyboxRenderer skybox("Textures/night-skybox");
    Model backpack("Models/backpack/backpack.obj");

    const unsigned int NR_LIGHTS = 32;
    std::vector<glm::vec3> lightPositions;
    std::vector<glm::vec3> lightColors;
    srand(13);
    for (unsigned int i = 0; i < NR_LIGHTS; i++)
    {
        // calculate slightly random offsets
        float xPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0);
        float yPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 4.0);
        float zPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0);
        lightPositions.push_back(glm::vec3(xPos, yPos, zPos));
        // also calculate random color
        float rColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.)
        float gColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.)
        float bColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.)
        lightColors.push_back(glm::vec3(rColor, gColor, bColor));
    }

    float frameCounter = 0.0f;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
    FrameBuffer applicationFBO(width, height);

    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    bloomShader.Activate();
    bloomShader.setInt("scene", 0);
    bloomShader.setInt("bloomBlur", 1);

    BloomRenderer bloomRenderer;
    bloomRenderer.Init(width, height);

    glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LEQUAL);
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

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        lightShader.Activate();
        lightShader.setMat4("mvp", camera.getProjectionMatrix() * camera.getViewMatrix());
        for (unsigned int i = 0; i < lightPositions.size(); i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, lightPositions[i]);
            model = glm::scale(model, glm::vec3(0.125f));
            lightShader.setMat4("matrix", model);
            lightShader.setVec3("lightColor", lightColors[i]);
            Utils::OpenGL::Draw::drawCube(cubeVAO, cubeVBO);
        }

        //skybox.render(camera);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if(guiOn)
            applicationFBO.Bind();
        bloomRenderer.RenderBloomTexture(colorBuffers[1], 0.005f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        bloomShader.Activate();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomRenderer.BloomTexture());

        float exposure = 1.0f;
        bloomShader.setInt("bloomOn", true);
        bloomShader.setFloat("exposure", exposure);
        Utils::OpenGL::Draw::drawQuad();
        if(guiOn)
            applicationFBO.Unbind();

        if (guiOn) {
            guiController.start();
            if (ImGui::Begin("control")) {
                ImGui::BeginChild("gBuffers textures");
                ImVec2 wsize = ImGui::GetWindowSize();
                ImGui::Image((ImTextureID)colorBuffers[0], wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)colorBuffers[1], wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)bloomRenderer.BloomTexture(), wsize, ImVec2(0, 1), ImVec2(1, 0));
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

        glfwPollEvents();
        glfwSwapBuffers(AppWindow::window);
    }
    return 0;
}

int DeferredRender::run()
{
    try {
        //show_demo();
        bloomTest();
        return 0;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
}