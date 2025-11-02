#include "area_light_demo.h"
struct VertexAL {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
};

const GLfloat psize = 10.0f;
VertexAL planeVertices[6] = {
    { {-psize, 0.0f, -psize}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },
    { {-psize, 0.0f,  psize}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f} },
    { { psize, 0.0f,  psize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f} },
    { {-psize, 0.0f, -psize}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },
    { { psize, 0.0f,  psize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f} },
    { { psize, 0.0f, -psize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f} }
};
VertexAL areaLightVertices[6] = {
    { {-8.0f, 2.4f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} }, // 0 1 5 4
    { {-8.0f, 2.4f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f} },
    { {-8.0f, 0.4f,  1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} },
    { {-8.0f, 2.4f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
    { {-8.0f, 0.4f,  1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} },
    { {-8.0f, 0.4f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f} }
};

GLuint planeVBO, planeVAO;
GLuint areaLightVBO, areaLightVAO;

void setupBuffers()
{
    // PLANE
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // AREA LIGHT
    glGenVertexArrays(1, &areaLightVAO);
    glBindVertexArray(areaLightVAO);
    glGenBuffers(1, &areaLightVBO);
    glBindBuffer(GL_ARRAY_BUFFER, areaLightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(areaLightVertices), areaLightVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

int AreaLightDemo::show_demo()
{
	Camera camera(AppWindow::width, AppWindow::height, glm::vec3(-6.5f, 3.5f, 8.5f), glm::vec3(0.5, -0.2, -1.0f));
	Camera reflectionCamera(AppWindow::width, AppWindow::height, glm::vec3(-6.5f, 3.5f, 8.5f), glm::vec3(0.5, -0.2, -1.0f));
    
    glm::mat4 reflectionMatrix = glm::mat4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
    
    GLuint mat1 = Utils::OpenGL::loadMTexture(LTC1);
    GLuint mat2 = Utils::OpenGL::loadMTexture(LTC2);

    Shader lightShader("Shaders/areaLight.vert", "Shaders/areaLight.frag");
    Shader planeShader("Shaders/light.vert", "Shaders/light.frag");
    Shader godRaysShader("Shaders/godRays/godRays.vert", "Shaders/godRays/godRays.frag");
    Shader blackScreenShader("Shaders/godRays/blackScreen.vert", "Shaders/godRays/blackScreen.frag");

    Texture albedo("pbr/concrete/albedo.png", "albedoMap", "Textures");
    Texture normal("pbr/concrete/normal.png", "normalMap", "Textures");
    Texture metallic("pbr/concrete/metallic.png", "metallicMap", "Textures");
    Texture roughness("pbr/concrete/roughness.png", "roughnessMap", "Textures");
    Texture ao("pbr/concrete/ao.png", "aoMap", "Textures");

    ImGuiController guiController;
    guiController.init(AppWindow::window, AppWindow::width, AppWindow::height);

    FrameBuffer applicationFBO(AppWindow::width, AppWindow::height);
    FrameBuffer reflectionFBO(AppWindow::width, AppWindow::height);
    FrameBuffer blackSceneFBO(AppWindow::width, AppWindow::height);
    FrameBuffer postProcessFBO(AppWindow::width, AppWindow::height);

    float frameCounter = 0.0f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float roughnessLevel = 0.2f;
    float intensity = 0.5f;
    bool twoSides = true;
    bool godraysEnabled = true;
    glm::vec3 lightColor(1.0, 1.0, 1.0);

    lightShader.Activate();
    lightShader.setVec3("areaLight.points[0]", areaLightVertices[0].position);
    lightShader.setVec3("areaLight.points[1]", areaLightVertices[1].position);
    lightShader.setVec3("areaLight.points[2]", areaLightVertices[4].position);
    lightShader.setVec3("areaLight.points[3]", areaLightVertices[5].position);
    lightShader.setVec3("areaLight.color", lightColor);
    lightShader.setInt("LTC1", 0);
    lightShader.setInt("LTC2", 1);
    lightShader.setInt("material.diffuse", 2);
    lightShader.setInt("reflectedScene", 3);

    planeShader.Activate();
    planeShader.setMat4("matrix", glm::mat4(1.0f));
    planeShader.setMat4("mvp", camera.getMVP());
    planeShader.setVec3("lightColor", lightColor);

    setupBuffers();
    glm::vec3 lightPosition = glm::vec3(0.0f, 0.0f, 0.0f);

    Component reimu("Models/reimu/reimu.obj");
    Model reimuModel("Models/reimu/reimu.obj");
    glm::vec3 scale(0.25);
    reimu.scale(scale);

    float decay = 0.96815;
    float exposure = 0.2;
    float density = 0.926;
    float weight = 0.58767;


    glEnable(GL_DEPTH_TEST);
    while (!glfwWindowShouldClose(AppWindow::window)) {
        glViewport(0, 0, AppWindow::width, AppWindow::height);
        glClearColor(0.16f, 0.18f, 0.17f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        guiController.start();


        ImGui::Begin("control");
        lightShader.Activate();
        ImGui::SliderFloat("Roughness", &roughnessLevel, 0.0f, 1.0f);
        lightShader.setVec4("material.albedoRoughness", glm::vec4(lightColor, roughnessLevel));
        ImGui::SliderFloat("Intensity", &intensity, 0.0f, 10.0f);
        lightShader.setFloat("areaLight.intensity", intensity);
        ImGui::Checkbox("Two sided", &twoSides);
        ImGui::SameLine();
        ImGui::Checkbox("Enable god rays", &godraysEnabled);
        lightShader.setFloat("areaLight.twoSided", twoSides);
        if (ImGui::ColorEdit4("Light color", &lightColor[0])) {
            lightShader.Activate();
            lightShader.setVec3("areaLight.color", lightColor);
            planeShader.Activate();
            planeShader.setVec3("lightColor", lightColor);
        }
        ImGui::SliderFloat3("Light position", &lightPosition[0], -5.0f, 5.0f);
        ImGui::SliderFloat("decay", &decay, -1.0f, 1.0f);
        ImGui::SliderFloat("exposure", &exposure, -1.0f, 1.0f);
        ImGui::SliderFloat("density", &density, -1.0f, 1.0f);
        ImGui::SliderFloat("weight", &weight, -1.0f, 1.0f);
        ImGui::End();

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        float currentTime = glfwGetTime();
        frameCounter++;
        if (deltaTime >= 1 / 2) {
            std::string FPS = std::to_string((1.0 / deltaTime) * frameCounter);
            std::string ms = std::to_string((deltaTime / frameCounter) * 1000);
            std::string updatedTitle = "Area Light Demo - " + FPS + "FPS / " + ms + "ms";
            glfwSetWindowTitle(AppWindow::window, updatedTitle.c_str());
            lastFrame = currentTime;
            frameCounter = 0;
        }

        UniformProperties uniforms(false, false);
        glm::vec3 lightAmbient = glm::vec3(0.5f, 0.5f, 0.5f);
        glm::vec3 lightDiffuse = glm::vec3(0.5f, 0.5f, 0.5f);
        glm::vec3 lightSpecular = glm::vec3(1.0f, 1.0f, 1.0f);
        glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 12.5f);
        float near_plane = 1.00f, far_plane = 25.0f;
        //glm::mat4 lightProjection = glm::perspective(glm::radians(90.0f), float(AppWindow::width) / float(AppWindow::height), near_plane, far_plane * 2);
        glm::mat4  lightView = glm::lookAt(lightPosition, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 lightMVP = lightProjection * lightView;

        Light light = Light(lightPosition, glm::vec4(1.0, 1.0, 1.0, 1.0), lightAmbient, lightDiffuse, lightSpecular, lightMVP, 2);

        camera.onUpdate();

        lightShader.Activate();
        glm::mat4 model(1.0f);
        glm::mat3 normalMatrix = glm::mat3(model);
        lightShader.setMat4("matrix", model);
        lightShader.setMat3("normalMatrix", normalMatrix);
        lightShader.setMat4("mvp", camera.getMVP());
        lightShader.setVec3("viewPosition", camera.getPosition());
        lightShader.setVec3("lightPosition", lightPosition);

        applicationFBO.Bind();
        glViewport(0, 0, AppWindow::width, AppWindow::height);
        glClearColor(0.16f, 0.18f, 0.17f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mat1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mat2);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, albedo.ID);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, reflectionFBO.texture);
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glUseProgram(0);

        planeShader.Activate();
		model = glm::translate(model, lightPosition);
		planeShader.setMat4("matrix", model);
		planeShader.setMat4("mvp", camera.getMVP());
        planeShader.setVec3("lightColor", lightColor);  // reset shader's light color to the correct one
        glBindVertexArray(areaLightVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
		glUseProgram(0);

        reimu.render(camera, light, uniforms);
        applicationFBO.Unbind();

        blackSceneFBO.Bind();
        glViewport(0, 0, AppWindow::width, AppWindow::height);
        glClearColor(0.16f, 0.18f, 0.17f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        blackScreenShader.Activate();
        blackScreenShader.setMat4("matrix", model);
        blackScreenShader.setMat4("mvp", camera.getMVP());

        planeShader.Activate();
        model = glm::translate(model, lightPosition);
        planeShader.setMat4("matrix", model);
        planeShader.setMat4("mvp", camera.getMVP());
        planeShader.setVec3("lightColor", lightColor);
        glBindVertexArray(areaLightVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glUseProgram(0);

        glm::mat4 scaleDown = glm::scale(glm::mat4(1.0f), glm::vec3(0.05));
        blackScreenShader.Activate();
        blackScreenShader.setMat4("matrix", scaleDown);
        blackScreenShader.setMat4("mvp", camera.getMVP());
        reimuModel.Draw(blackScreenShader);
        blackSceneFBO.Unbind();

        reflectionFBO.Bind();
        glViewport(0, 0, AppWindow::width, AppWindow::height);
        glClearColor(0.16f, 0.18f, 0.17f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 reflectedMVP = camera.getMVP() * reflectionMatrix;
        reflectionCamera.mvp = reflectedMVP;
        reimu.render(reflectionCamera, light, uniforms);
        reflectionFBO.Unbind();

        postProcessFBO.Bind();
        glViewport(0, 0, AppWindow::width, AppWindow::height);
        glClearColor(0.16f, 0.18f, 0.17f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        godRaysShader.Activate();
        godRaysShader.setInt("blackSceneTexture", 0);
        godRaysShader.setInt("sceneTexture", 1);
        godRaysShader.setFloat("decay", decay); 
        godRaysShader.setFloat("exposure", exposure); 
        godRaysShader.setFloat("density", density); 
        godRaysShader.setFloat("weight", weight);

        glm::vec4 lightP = lightProjection * lightView * glm::vec4(lightPosition, 1.0);
        glm::vec2 lightPosNDC;
        lightPosNDC.x = lightP.x / lightP.w;
        lightPosNDC.y = lightP.y / lightP.w;
        lightPosNDC.x = lightPosNDC.x * 0.5 + 0.5;
        lightPosNDC.y = lightPosNDC.y * 0.5 + 0.5;

        godRaysShader.setMat4("viewMatrix", lightView);
        godRaysShader.setMat4("projectionMatrix", lightProjection);
        godRaysShader.setVec2("lightPos2D", lightPosNDC);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, blackSceneFBO.texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, applicationFBO.texture);

        Utils::OpenGL::Draw::drawQuad();
        postProcessFBO.Unbind();

        ImGui::Begin("Application window");
        ImGui::BeginChild("Child");
        ImVec2 wsize = ImGui::GetWindowSize();
        int wWidth = static_cast<int>(ImGui::GetWindowWidth());
        int wHeight = static_cast<int>(ImGui::GetWindowHeight());
        camera.updateViewResize(wWidth, wHeight);
        if (godraysEnabled)
            ImGui::Image((ImTextureID)postProcessFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
        else
            ImGui::Image((ImTextureID)applicationFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
        
        if (ImGui::IsItemHovered())
            camera.processInput(AppWindow::window);
        ImGui::EndChild();
        ImGui::End();

        ImGui::Begin("debug window");
        ImGui::Image((ImTextureID)blackSceneFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::End();

        guiController.end();

        glfwPollEvents();
        glfwSwapBuffers(AppWindow::window);
	}

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteVertexArrays(1, &areaLightVAO);
    glDeleteBuffers(1, &areaLightVBO);

    return 0;
}

int AreaLightDemo::run()
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
