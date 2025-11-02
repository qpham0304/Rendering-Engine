#include "SSAO_demo.h"
#include "../../graphics/renderer/deferredRenderer.h"
#include "../../core/scene/SceneManager.h"

float customLerp(float a, float b, float f)
{
    return a + f * (b - a);
}

int SSAO_Demo::show_demo()
{
    float width = AppWindow::width;
    float height = AppWindow::height;
    Camera camera(width, height, glm::vec3(-3.5f, 1.5f, 5.5f), glm::vec3(0.5, -0.2, -1.0f));

    ImGuiController guiController;
    bool guiOn = true;
    if (guiOn)
        guiController.init(AppWindow::window, width, height);

    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");

    //SkyboxRenderer skybox("Textures/night-skybox");
    SceneManager::addComponent("Models/backpack/backpack.obj");
    std::vector<Component*> components;
    for (const auto& pair : SceneManager::components) {
        components.push_back(pair.second.get());
    }

    float frameCounter = 0.0f;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
    unsigned int sphereVAO = 0;
    unsigned int sphereVBO = 0;
    FrameBuffer applicationFBO(width, height);
    DeferredRenderer deferredRenderer(width, height);
    std::vector<Light> lights;
    //lights.push_back(Light(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
    const unsigned int NR_LIGHTS = 16;
    for (unsigned int i = 0; i < NR_LIGHTS; i++)
    {
        // calculate slightly random offsets
        float xPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0) * 2;
        float yPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 4.0) * 2;
        float zPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0) * 2;
        // also calculate random color
        float rColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.)
        float gColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.)
        float bColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.)
        lights.push_back(Light(glm::vec3(xPos, yPos, zPos), glm::vec4(rColor, gColor, bColor, 1.0)));
    }

    unsigned int ssaoFBO;
    glGenFramebuffers(1, &ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

    unsigned int ssaoTexture;
    glGenTextures(1, &ssaoTexture);
    glBindTexture(GL_TEXTURE_2D, ssaoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Framebuffer not complete!" << std::endl;

    unsigned int ssaoBlurFBO;
    glGenFramebuffers(1, &ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);

    unsigned int ssaoBlurTexture;
    glGenTextures(1, &ssaoBlurTexture);
    glBindTexture(GL_TEXTURE_2D, ssaoBlurTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoBlurTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);



    // generate sample kernel
    std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
    std::vector<glm::vec3> ssaoKernel;
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator));    // 0 to 1 only for z since we want half a sphere not an entire one
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / 64.0f;

        // samples are distributed randomly so we want to add larger weight on occlusions close to the fragment
        // scale samples s.t. they're more aligned to center of kernel
        scale = customLerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    // generate noise texture for random rotation which reduce ssaoKernel random sample
    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            0.0f); // rotate around z-axis (in tangent space)
        ssaoNoise.push_back(noise);
    }
    unsigned int noiseTexture; 
    glGenTextures(1, &noiseTexture); 
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    Shader blurShader("Shaders/ssao/ssao.vert", "Shaders/ssao/blur.frag");
    blurShader.Activate();
    blurShader.setInt("ssaoInput", 0);

    Texture cubeTex("Textures/squish.png", "albedoMap");

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    glfwSwapInterval(1);
    while (!glfwWindowShouldClose(AppWindow::window)) {
        camera.onUpdate();
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;

        Shader ssaoShader("Shaders/ssao/ssao.vert", "Shaders/ssao/ssao.frag");
        ssaoShader.Activate();
        ssaoShader.setInt("gPosition", 0);
        ssaoShader.setInt("gNormal", 1);
        ssaoShader.setInt("texNoise", 2);
        ssaoShader.setVec2("noiseScale", glm::vec2(width / 4.0f, height / 4.0f));   // tile noise texture over screen based
        ssaoShader.setInt("gDepth", 3);
        ssaoShader.setMat4("invProjection", camera.getInProjectionMatrix());
        ssaoShader.setMat4("invView", camera.getInViewMatrix());

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
        deferredRenderer.renderGeometry(camera, components);

        glBindFramebuffer(GL_FRAMEBUFFER, deferredRenderer.getGBuffer());
        deferredRenderer.geometryShader->Activate();
        deferredRenderer.geometryShader->setBool("invertedNormals", true);
        deferredRenderer.geometryShader->setMat4("projection", projection);
        deferredRenderer.geometryShader->setMat4("view", view);
        glm::mat4 cubeModel = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
        cubeModel = glm::scale(cubeModel, glm::vec3(6.0f));
        deferredRenderer.geometryShader->setMat4("model", cubeModel);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeTex.ID);
        Utils::OpenGL::Draw::drawCube(cubeVAO, cubeVBO);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        //ssao pass
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ssaoShader.Activate();
        for (unsigned int i = 0; i < 64; ++i) {
            ssaoShader.setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
        }
        ssaoShader.setMat4("projection", projection);
        ssaoShader.setMat4("view", view);
        glClear(GL_COLOR_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, deferredRenderer.getGPosition());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, deferredRenderer.getGNormal());
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, noiseTexture);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, deferredRenderer.getGDepth());
        Utils::OpenGL::Draw::drawQuad();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        blurShader.Activate();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ssaoTexture);
        Utils::OpenGL::Draw::drawQuad();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        applicationFBO.Bind();
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        deferredRenderer.colorShader->Activate();
        deferredRenderer.colorShader->setInt("ssaoTex", 3);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, ssaoBlurTexture);
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
            Utils::OpenGL::Draw::drawSphere(sphereVAO, sphereVBO);
        }

        applicationFBO.Unbind();

        if (guiOn) {
            guiController.start();

            if (ImGui::Begin("control")) {
                ImGui::BeginChild("gBuffers textures");
                ImVec2 wsize = ImGui::GetWindowSize();
                ImGui::Image((ImTextureID)deferredRenderer.getGPosition(), wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)deferredRenderer.getGNormal(), wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)deferredRenderer.getGAlbedoSpec(), wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)noiseTexture, wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)ssaoTexture, wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)ssaoBlurTexture, wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)deferredRenderer.getGDepth(), wsize, ImVec2(0, 1), ImVec2(1, 0));
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

int SSAO_Demo::run()
{
    try {
        show_demo();
        return 0;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
}
