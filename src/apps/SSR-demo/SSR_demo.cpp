#include "SSR_demo.h"
#include "../../graphics/renderer/deferredRenderer.h"
#include "../../core/scene/SceneManager.h"

int SSR_demo::show_demo()
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
    unsigned int planeVAO = 0;
    unsigned int planeVBO = 0;

    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
    //Shader planeShader("Shaders/deferredShading/deferredShading.vert", "Shaders/deferredShading/reflectionPlane.frag");

    SkyboxRenderer skybox;
    DeferredRenderer deferredRenderer(width, height);

    Texture cubeTex("Textures/squish.png", "albedoMap");
    Texture defaultAO("Textures/default/roughness-2.png", "aoMap");
    FrameBuffer applicationFBO(width, height);
    FrameBuffer colorSceneFBO(width, height);

    SceneManager::addComponent("Models/backpack/backpack.obj");
    std::vector<Component*> components;
    for (const auto& pair : SceneManager::components) {
        components.push_back(pair.second.get());
    }

    std::vector<Light> lights;
    lights.push_back(Light(glm::vec3(2.0, 0.5, 2.0), glm::vec4(1.0)));
    lights.push_back(Light(glm::vec3(-2.0, 0.5, 2.0), glm::vec4(1.0)));

    unsigned int ssrFBO;
    glGenFramebuffers(1, &ssrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssrFBO);

    unsigned int ssrTexture;
    glGenTextures(1, &ssrTexture);
    glBindTexture(GL_TEXTURE_2D, ssrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssrTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Framebuffer not complete!" << std::endl;

    unsigned int ssroBlurFBO;
    glGenFramebuffers(1, &ssroBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssroBlurFBO);

    unsigned int ssrBlurTexture;
    glGenTextures(1, &ssrBlurTexture);
    glBindTexture(GL_TEXTURE_2D, ssrBlurTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssrBlurTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        deferredRenderer.geometryShader.reset(new Shader("Shaders/deferredShading/gBuffer.vert", "Shaders/deferredShading/gBuffer.frag"));
        deferredRenderer.colorShader.reset(new Shader("Shaders/deferredShading/deferredShading.vert", "Shaders/deferredShading/deferredShading.frag"));


        deferredRenderer.geometryShader->Activate();
        deferredRenderer.geometryShader->setBool("invertedTexCoords", true);
        deferredRenderer.renderGeometry(camera, components);

        deferredRenderer.geometryShader->setBool("invertedTexCoords", false);
        glm::mat4 projection = glm::perspective(glm::radians(camera.getFOV()), (float)width / (float)height, 0.1f, 100.0f);
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 planeModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -2.0f, 0.0f));
        planeModel *= glm::scale(glm::mat4(1.0f), glm::vec3(30.0f));
        //glm::mat4 planeModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -12.0f, 0.0f));
        //planeModel = glm::rotate(planeModel, glm::radians(90.0f), glm::vec3(1.0, 0.0, 0.0));
        glBindFramebuffer(GL_FRAMEBUFFER, deferredRenderer.getGBuffer());
        deferredRenderer.geometryShader->Activate();
        deferredRenderer.geometryShader->setMat4("projection", projection);
        deferredRenderer.geometryShader->setMat4("view", view);
        deferredRenderer.geometryShader->setMat4("model", planeModel); //glm::translate(glm::mat4(1.0), glm::vec3(0.0, -3.0, 0.0))
        deferredRenderer.geometryShader->setInt("albedoMap", 0);
        deferredRenderer.geometryShader->setInt("metallicMap", 1);
        deferredRenderer.geometryShader->setFloat("reflectiveMap", 1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeTex.ID);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, cubeTex.ID);
        //Utils::OpenGL::Draw::drawCube(cubeVAO, cubeVBO);
        Utils::OpenGL::Draw::drawQuadNormals();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        applicationFBO.Bind();
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        deferredRenderer.colorShader->Activate();
        deferredRenderer.colorShader->setFloat("intensity", 5.0f);
        deferredRenderer.colorShader->setInt("ssaoTex", 3);
        deferredRenderer.colorShader->setMat4("view", view);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, defaultAO.ID);
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
        //skybox.render(camera);
        applicationFBO.Unbind();

        //SSR addition as a postprocess effect
        colorSceneFBO.Bind();
        glViewport(0, 0, AppWindow::width, AppWindow::height);
        glClearColor(0.16f, 0.18f, 0.17f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        

        //SSR setup and pass
        Shader SSRShader("Shaders/SSR/SSR.vert", "Shaders/SSR/SSR.frag");
        SSRShader.Activate();
        SSRShader.setInt("gPosition", 0);
        SSRShader.setInt("gNormal", 1);
        SSRShader.setInt("gAlbedoSpec", 2);
        SSRShader.setInt("gExtraComponents", 3);
        SSRShader.setInt("ColorBuffer", 4);
        SSRShader.setMat4("invView", glm::inverse(view));
        SSRShader.setMat4("invProjection", projection);
        SSRShader.setMat4("projection", projection);
        SSRShader.setMat4("view", view);
        SSRShader.setVec3("camPos", camera.getPosition());  // for testing position, remove later
        SSRShader.setInt("width", width);
        SSRShader.setInt("height", height);
        SSRShader.setMat4("mvp", projection * view);
        SSRShader.setMat4("matrix", planeModel);
        SSRShader.setVec2("gTexSizeInv", glm::vec2(1.0/width, 1.0/height));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, deferredRenderer.getGPosition());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, deferredRenderer.getGNormal());
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, deferredRenderer.getGAlbedoSpec());
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, applicationFBO.texture);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, colorSceneFBO.texture);
        Utils::OpenGL::Draw::drawQuad();
        colorSceneFBO.Unbind();

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
                ImGui::Image((ImTextureID)colorSceneFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
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

int SSR_demo::run()
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
