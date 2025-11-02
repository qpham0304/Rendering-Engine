#include "pbr_demo.h"

int DemoPBR::show_demo() {
    Camera camera(AppWindow::width, AppWindow::height, glm::vec3(-6.5f, 3.5f, 8.5f), glm::vec3(0.5, -0.2, -1.0f));
    
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    unsigned int sphereVAO = 0;
    unsigned int indexCount;
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;

    glm::vec3 lightPositions[] = {
        glm::vec3(-5.0f,  5.0f, 5.0f),
        glm::vec3(5.0f,  5.0f, 5.0f),
        glm::vec3(-5.0f, -5.0f, 5.0f),
        glm::vec3(5.0f, -5.0f, 5.0f),
    };
    glm::vec4 lightColors[] = {
        glm::vec4(100.0f, 100.0f, 100.0f, 1.0f),
        glm::vec4(100.0f, 200.0f, 100.0f, 1.0f),
        glm::vec4(100.0f, 100.0f, 300.0f, 1.0f),
        glm::vec4(200.0f, 100.0f, 100.0f, 1.0f)
    };

    Texture albedo0("pbr/white-marble/albedo.png", "albedoMap", "Textures");
    Texture normal0("pbr/white-marble/normal.png", "normalMap", "Textures");
    Texture metallic0("pbr/white-marble/metallic.png", "metallicMap", "Textures");
    Texture roughness0("pbr/white-marble/roughness.png", "roughnessMap", "Textures");
    Texture ao0("pbr/white-marble/ao.png", "aoMap", "Textures");

    Texture albedo1("pbr/rusted_iron/albedo.png", "albedoMap", "Textures");
    Texture normal1("pbr/rusted_iron/normal.png", "normalMap", "Textures");
    Texture metallic1("pbr/rusted_iron/metallic.png", "metallicMap", "Textures");
    Texture roughness1("pbr/rusted_iron/roughness.png", "roughnessMap", "Textures");
    Texture ao1("pbr/rusted_iron/ao.png", "aoMap", "Textures");

    Texture albedo2("pbr/titanium/albedo.png", "albedoMap", "Textures");
    Texture normal2("pbr/titanium/normal.png", "normalMap", "Textures");
    Texture metallic2("pbr/titanium/metallic.png", "metallicMap", "Textures");
    Texture roughness2("pbr/titanium/roughness.png", "roughnessMap", "Textures");
    Texture ao2("pbr/titanium/ao.png", "aoMap", "Textures");

    Texture albedo3("pbr/wall/albedo.png", "albedoMap", "Textures");
    Texture normal3("pbr/wall/normal.png", "normalMap", "Textures");
    Texture metallic3("pbr/wall/metallic.png", "metallicMap", "Textures");
    Texture roughness3("pbr/wall/roughness.png", "roughnessMap", "Textures");
    Texture ao3("pbr/wall/ao.png", "aoMap", "Textures");

    Texture albedo4("pbr/tight-weave-carpet-bl/albedo.png", "albedoMap", "Textures");
    Texture normal4("pbr/tight-weave-carpet-bl/normal.png", "normalMap", "Textures");
    Texture metallic4("pbr/tight-weave-carpet-bl/metallic.png", "metallicMap", "Textures");
    Texture roughness4("pbr/tight-weave-carpet-bl/roughness.png", "roughnessMap", "Textures");
    Texture ao4("pbr/tight-weave-carpet-bl/ao.png", "aoMap", "Textures");

    Texture albedo5("pbr/caved-floor/albedo.png", "albedoMap", "Textures");
    Texture normal5("pbr/caved-floor/normal.png", "normalMap", "Textures");
    Texture metallic5("pbr/caved-floor/metallic.png", "metallicMap", "Textures");
    Texture roughness5("pbr/caved-floor/roughness.png", "roughnessMap", "Textures");
    Texture ao5("pbr/caved-floor/ao.png", "aoMap", "Textures");

    Texture albedo6("pbr/concrete/albedo.png", "albedoMap", "Textures");
    Texture normal6("pbr/concrete/normal.png", "normalMap", "Textures");
    Texture metallic6("pbr/concrete/metallic.png", "metallicMap", "Textures");
    Texture roughness6("pbr/concrete/roughness.png", "roughnessMap", "Textures");
    Texture ao6("pbr/concrete/ao.png", "aoMap", "Textures");

    std::vector<Texture> albedoMaps = { albedo0, albedo1, albedo2, albedo3, albedo4, albedo5, albedo6 };
    std::vector<Texture> normalMaps = { normal0, normal1, normal2, normal3, normal4, normal5, normal6 };
    std::vector<Texture> metallicMaps = { metallic0, metallic1, metallic2, metallic3, metallic4, metallic5, metallic6 };
    std::vector<Texture> roughnessMaps = { roughness0, roughness1, roughness2, roughness3, roughness4, roughness5, roughness6 };
    std::vector<Texture> aoMaps = { ao0, albedo1, ao2, ao3, ao4, ao5, ao6 };

    unsigned int hdrTexture;
    std::string texRes = Utils::OpenGL::loadHDRTexture("Textures/hdr/industrial_sunset_02_puresky_1k.hdr", hdrTexture);
    std::cout << texRes << std::endl;

    Shader pbrShader("Shaders/default-2.vert", "Shaders/default-2.frag");
    Shader equirectangularToCubemapShader("Shaders/cubemap-hdr.vert", "Shaders/equireRectToCubemap.frag");
    Shader irradianceShader("Shaders/cubemap-hdr.vert", "Shaders/irradianceConvolution.frag");
    Shader backgroundShader("Shaders/background.vert", "Shaders/background.frag");
    Shader prefilterShader("Shaders/cubemap-hdr.vert", "Shaders/prefilter.frag");
    Shader brdfShader("Shaders/brdf.vert", "Shaders/brdf.frag");

    Model helmetModel("Models/DamagedHelmet/gltf/DamagedHelmet.gltf");
    Model backpackModel("Models/mountain_asset_canadian_rockies_modular/scene.gltf");

    pbrShader.Activate();
    pbrShader.setInt("albedoMap", 0);
    pbrShader.setInt("normalMap", 1);
    pbrShader.setInt("metallicMap", 2);
    pbrShader.setInt("roughnessMap", 3);
    pbrShader.setInt("aoMap", 4);
    pbrShader.setInt("emissiveMap", 5);
    pbrShader.setInt("irradianceMap", 6);
    pbrShader.setInt("prefilterMap", 7);
    pbrShader.setInt("brdfLUT", 8);

    backgroundShader.Activate();
    backgroundShader.setInt("environmentMap", 0);

    unsigned int captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    //---------------envcubemap set up---------------//
    unsigned int envCubemapTexture;
    glGenTextures(1, &envCubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemapTexture);
    for (unsigned int i = 0; i < 6; ++i)
    {
        // note that we store each face with 16 bit floating point values
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] =
    {
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    // convert HDR equirectangular environment map to cubemap equivalent
    equirectangularToCubemapShader.Activate();
    equirectangularToCubemapShader.setInt("equirectangularMap", 0);
    equirectangularToCubemapShader.setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, 512, 512); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    for (unsigned int i = 0; i < 6; ++i)
    {
        equirectangularToCubemapShader.setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemapTexture, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Utils::OpenGL::Draw::drawCube(cubeVAO, cubeVBO); // renders a 1x1 cube
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //------------------------------------------------------------//

    // then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemapTexture);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    //---------------irradiance map set up---------------//
    unsigned int irradianceMap;
    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0,
            GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    irradianceShader.Activate();
    irradianceShader.setInt("environmentMap", 0);
    irradianceShader.setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemapTexture);

    glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        irradianceShader.setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Utils::OpenGL::Draw::drawCube(cubeVAO, cubeVBO);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //------------------------------------------------------------//


    //-----------------prefilter map---------------------//
    unsigned int prefilterMap;
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    prefilterShader.Activate();
    prefilterShader.setInt("environmentMap", 0);
    prefilterShader.setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemapTexture);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        // reisze framebuffer according to mip-level size.
        unsigned int mipWidth = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        unsigned int mipHeight = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        prefilterShader.setFloat("roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i)
        {
            prefilterShader.setMat4("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            Utils::OpenGL::Draw::drawCube(cubeVAO, cubeVBO);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //-----------------------------------------------------------//
    

    //-----------------------TLU map-----------------------------//
    unsigned int brdfLUTTexture;
    glGenTextures(1, &brdfLUTTexture);

    // pre-allocate enough memory for the LUT texture.
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

    glViewport(0, 0, 512, 512);
    brdfShader.Activate();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Utils::OpenGL::Draw::drawQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //-----------------------------------------------------------//

    glm::mat4 projection = glm::perspective(glm::radians(camera.getFOV()), (float)AppWindow::width / AppWindow::height, 0.1f, 100.0f);
    backgroundShader.Activate();
    backgroundShader.setMat4("projection", projection);

    // then before rendering, configure the viewport to the original framebuffer's screen dimensions
    int scrWidth, scrHeight;
    glfwGetFramebufferSize(AppWindow::window, &scrWidth, &scrHeight);
    glViewport(0, 0, scrWidth, scrHeight);

    int nrRows = 7;
    int nrColumns = 7;
    float spacing = 2.5;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    float frameCounter = 0.0f;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;

    Texture emissiveMap("Textures/default/emissive.png", "emissiveMap");
    
    while (!glfwWindowShouldClose(AppWindow::window)) {

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        float currentTime = glfwGetTime();
        frameCounter++;
        if (deltaTime >= 1 / 2) {
            std::string FPS = std::to_string((1.0 / deltaTime) * frameCounter);
            std::string ms = std::to_string((deltaTime / frameCounter) * 1000);
            std::string updatedTitle = "PBR - IBL demo - " + FPS + "FPS / " + ms + "ms";
            glfwSetWindowTitle(AppWindow::window, updatedTitle.c_str());
            lastFrame = currentTime;
            frameCounter = 0;
        }

        // Clear the color buffer
        glViewport(0, 0, AppWindow::width, AppWindow::height);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f); // RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        camera.onUpdate();
        camera.processInput(AppWindow::window);
        glm::mat4 model = glm::mat4(1.0f);

        pbrShader.Activate();
        pbrShader.setMat4("matrix", glm::mat4(1.0f));
        pbrShader.setMat4("mvp", camera.getMVP());
        pbrShader.setVec3("camPos", camera.getPosition());
        pbrShader.setBool("gamma", true);

        // bind pre-computed IBL data
        glActiveTexture(GL_TEXTURE0 + 6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        glActiveTexture(GL_TEXTURE0 + 7);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
        glActiveTexture(GL_TEXTURE0 + 8);
        glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
        // use slot 9 for height map
        // use slot 10 for shadowmap directional, first slot should be directional light
        // use 11 for shadowmap point/spotlight

        // render rows*column number of spheres with material properties defined by textures (they all have the same material properties)
        for (int row = 0; row < nrRows; ++row)
        {
            for (int col = 0; col < nrColumns; ++col)
            {
                glActiveTexture(GL_TEXTURE0 + 0);
                glBindTexture(GL_TEXTURE_2D, albedoMaps[col].ID);
                glActiveTexture(GL_TEXTURE0 + 1);
                glBindTexture(GL_TEXTURE_2D, normalMaps[col].ID);
                glActiveTexture(GL_TEXTURE0 + 2);
                glBindTexture(GL_TEXTURE_2D, metallicMaps[col].ID);
                glActiveTexture(GL_TEXTURE0 + 3);
                glBindTexture(GL_TEXTURE_2D, roughnessMaps[col].ID);
                glActiveTexture(GL_TEXTURE0 + 4);
                glBindTexture(GL_TEXTURE_2D, aoMaps[col].ID);
                glActiveTexture(GL_TEXTURE0 + 5);
                glBindTexture(GL_TEXTURE_2D, emissiveMap.ID);
                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(
                    (float)(col - (nrColumns / 2)) * spacing,
                    (float)(row - (nrRows / 2)) * spacing,
                    0.0f
                ));

                pbrShader.setMat4("matrix", model);
                pbrShader.setBool("hasAnimation", false);
                pbrShader.setBool("hasEmission", false);
                pbrShader.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
                Utils::OpenGL::Draw::drawSphere(sphereVAO, indexCount);
            }
        }
        
        model = glm::rotate(glm::mat4(1.0), 90.0f, glm::vec3(1.0, 0.0, 0.0));
        model = glm::translate(model, glm::vec3(0.0f, 3.0f, 0.0f));
        pbrShader.setMat4("matrix", model);
        pbrShader.setBool("hasAnimation", false);
        pbrShader.setBool("hasEmission", true);
        pbrShader.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
        helmetModel.Draw(pbrShader);

        pbrShader.setBool("hasAnimation", false);
        pbrShader.setBool("hasEmission", false);
        model = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 3.0f, 3.0f));
        model = glm::scale(model, glm::vec3(20.0f));
        model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0, 0.0, 0.0));
        pbrShader.setMat4("matrix", model);
        pbrShader.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
        backpackModel.Draw(pbrShader);

        for (unsigned int i = 0; i < sizeof(lightPositions) / sizeof(lightPositions[0]); ++i)
        {
            glm::vec3 newPos = lightPositions[i] + glm::vec3(sin(glfwGetTime() * 5.0) * 5.0, 0.0, 0.0);
            newPos = lightPositions[i];
            pbrShader.setVec3("lightPositions[" + std::to_string(i) + "]", newPos);
            pbrShader.setVec3("lightColors[" + std::to_string(i) + "]", lightColors[i]);

            model = glm::mat4(1.0f);
            model = glm::translate(model, newPos);
            model = glm::scale(model, glm::vec3(0.5f));
            pbrShader.setMat4("matrix", model);
            pbrShader.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
            Utils::OpenGL::Draw::drawSphere(sphereVAO, indexCount);
        }



        // render skybox (render as last to prevent overdraw)
        backgroundShader.Activate();
        backgroundShader.setMat4("view", camera.getViewMatrix());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemapTexture);
        Utils::OpenGL::Draw::drawCube(cubeVAO, cubeVBO);

        glfwPollEvents();
        glfwSwapBuffers(AppWindow::window);
    }
    return 0;

}


int DemoPBR::run()
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