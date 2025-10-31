#include "deferredIBL_demo.h"
#include "ParticleGeometry.h"
#include "../../core/scene/SceneManager.h"
#include "../../core/components/MComponent.h"
#include "../../core/components/CameraComponent.h"
#include "../../core/components/CubeMapComponent.h"
#include "camera.h"

static glm::vec3 lightPositions[] = {
    glm::vec3(20.00f,  20.0f, 0.0),
    //glm::vec3(10.0f,  1.0f, -10.0f),
    //glm::vec3(-10.0f, 5.0f, 10.0f),
    //glm::vec3(-10.0f, 5.0f, -10.0f),
};
static glm::vec4 lightColors[] = {
    glm::vec4(900.0f, 500.0f, 500.0f, 1.0f),
    //glm::vec4(300.0f, 500.0f, 300.0f, 1.0f),
    //glm::vec4(900.0f, 400.0f, 300.0f, 1.0f),
    //glm::vec4(500.0f, 300.0f, 500.0f, 1.0f)
};

static float customLerp(float a, float b, float f)
{
    return a + f * (b - a);
}

void DeferredIBLDemo::setupBuffers()
{
    int width = AppWindow::width;
    int height = AppWindow::height;

    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gAlbedo, 0);

    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    glGenTextures(1, &gMetalRoughness);
    glBindTexture(GL_TEXTURE_2D, gMetalRoughness);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gMetalRoughness, 0);

    glGenTextures(1, &gEmissive);
    glBindTexture(GL_TEXTURE_2D, gEmissive);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gEmissive, 0);

    glGenTextures(1, &gDUV);
    glBindTexture(GL_TEXTURE_2D, gDUV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gDUV, 0);

    glGenTextures(1, &gDepth);
    glBindTexture(GL_TEXTURE_2D, gDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, width, height, 0, 
        GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 0
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, gDepth, 0);

    const unsigned int numAttachments = 5;
    GLenum drawBuffers[numAttachments];
    for (int i = 0; i < numAttachments; i++) {
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(numAttachments, drawBuffers);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::endl;
    }
}

void DeferredIBLDemo::renderDeferredPass()
{
    Scene& scene = *SceneManager::getInstance().getActiveScene();
    
    scene.addShader("colorPassShader", "Shaders/deferredIBL/colorPass.vert", "Shaders/deferredIBL/colorPass.frag");
    scene.getShader("colorPassShader")->Activate();
    scene.getShader("colorPassShader")->setInt("gDepth", 0);
    scene.getShader("colorPassShader")->setInt("gNormal", 1);
    scene.getShader("colorPassShader")->setInt("gAlbedo", 2);
    scene.getShader("colorPassShader")->setInt("gMetalRoughness", 3);
    scene.getShader("colorPassShader")->setInt("gEmissive", 4);
    scene.getShader("colorPassShader")->setInt("gDUV", 5);
    scene.getShader("colorPassShader")->setInt("irradianceMap", 6);
    scene.getShader("colorPassShader")->setInt("prefilterMap", 7);
    scene.getShader("colorPassShader")->setInt("brdfLUT", 8);
    scene.getShader("colorPassShader")->setInt("ssaoTex", 9);
    scene.getShader("colorPassShader")->setInt("sunDepthMap", 10);
    scene.getShader("colorPassShader")->setInt("atmosphereMap", 11);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gDepth);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, gMetalRoughness);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, gEmissive);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, gDUV);
    
    auto list = scene.getEntitiesWith<CubeMapComponent>();
    CubeMapComponent* cubeMap = nullptr;
    if (!list.empty() && list[0].hasComponent<CubeMapComponent>()) {
        cubeMap = &list[0].getComponent<CubeMapComponent>();
        cubeMap->bindIBL(); // slot 6, 7, 8
    }

    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, ssaoBlurTexture);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, depthMap.texture);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, atmosphereScene.texture);

    scene.getShader("colorPassShader")->setMat4("viewMatrix", camera->getViewMatrix());
    scene.getShader("colorPassShader")->setMat4("invProjection", camera->getInProjectionMatrix());
    scene.getShader("colorPassShader")->setMat4("inverseView", camera->getInViewMatrix());
    scene.getShader("colorPassShader")->setBool("gamma", true);
    scene.getShader("colorPassShader")->setBool("sampleRadius", 2);
    scene.getShader("colorPassShader")->setBool("time", glfwGetTime());
    scene.getShader("colorPassShader")->setBool("rippleStrength", 1.0);


    std::vector<Entity> lights = scene.getEntitiesWith<MLightComponent, TransformComponent>();
    int index = 0;
    for (auto& entity : lights) {
        MLightComponent& light = entity.getComponent<MLightComponent>();
        TransformComponent& transform = entity.getComponent<TransformComponent>();
        glm::vec3 lightIntensity = light.color * (transform.scaleVec * transform.scaleVec);

        if (light.type == DIRECTION_LIGHT) {
            scene.getShader("colorPassShader")->setVec3("sunPos", light.position);
        }

        else if (light.type == POINT_LIGHT) {
            scene.getShader("colorPassShader")->setVec3("lights[" + std::to_string(index) + "].position", light.position);
        }

        else if (light.type == SPOT_LIGHT) {

        }

        else if (light.type == AREA_LIGHT) {

        }

        scene.getShader("colorPassShader")->setVec3("lights[" + std::to_string(index) + "].color", lightIntensity);

        glm::mat4& model = transform.getModelMatrix();
        light.position = transform.translateVec;
        index++;
    }



    lightPassFBO.Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    Utils::OpenGL::Draw::drawQuad();

    float timeDiff = currentFrame - lastFrame;

    if (cubeMap) {
        //if (timeDiff >= 0.05f) {    // hack to reload cubemap less frequently
        //    cubeMap->reloadTexture(atmosphereScene.texture);
        //    lastFrame = currentFrame;
        //}
        cubeMap->render(camera);
    }
    lightPassFBO.Unbind();

}

void DeferredIBLDemo::setupSkyView()
{
    int width = AppWindow::width;
    int height = AppWindow::height;
    transmittanceLUT.Init(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
    multipleScatteredLUT.Init(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
    skyViewLUT.Init(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
    atmosphereScene.Init(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);


    ////////////////////////////////////////////////////////////
    GLuint aerialPerspectiveTexture;
    glGenTextures(1, &aerialPerspectiveTexture);
    glBindTexture(GL_TEXTURE_3D, aerialPerspectiveTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, 32, 32, 32, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    transmittanceLUT.Bind();
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Shader transmittanceShader("Shaders/atmospheric.vert", "Shaders/skyAtmosphere/transmittanceLUT.frag");
    transmittanceShader.Activate();
    Utils::OpenGL::Draw::drawQuad();
    transmittanceLUT.Unbind();

    multipleScatteredLUT.Bind();
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Shader multipleScatteredShader("Shaders/atmospheric.vert", "Shaders/skyAtmosphere/multipleScatteredLUT.frag");
    multipleScatteredShader.Activate();
    multipleScatteredShader.setInt("iChannel0", 0);
    multipleScatteredShader.setVec2("iChannelResolution", glm::vec2(width, height));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, transmittanceLUT.texture);
    Utils::OpenGL::Draw::drawQuad();
    multipleScatteredLUT.Unbind();
}

void DeferredIBLDemo::renderSkyView()
{
    int width = AppWindow::width;
    int height = AppWindow::height;

    if (!timePaused) {
        currentFrame = static_cast<float>(glfwGetTime());
    }

    skyViewLUT.Bind();
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Shader skyViewShader("Shaders/atmospheric.vert", "Shaders/skyAtmosphere/skyViewLUT.frag");
    skyViewShader.Activate();
    skyViewShader.setInt("transmittanceLUT", 0);
    skyViewShader.setInt("multipleScatteredLUT", 1);
    skyViewShader.setVec2("iChannelResolution0", glm::vec2(width, height));
    skyViewShader.setVec2("iChannelResolution1", glm::vec2(width, height));
    skyViewShader.setFloat("iTime", currentFrame);
    skyViewShader.setVec2("iMouse", glm::vec2(0.0, 0.0));
    skyViewShader.setVec3("camPos", camera->getPosition());
    skyViewShader.setMat4("viewMatrix", camera->getViewMatrix());
    skyViewShader.setMat4("projectionMatrix", camera->getProjectionMatrix());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, transmittanceLUT.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, multipleScatteredLUT.texture);
    Utils::OpenGL::Draw::drawQuad();
    skyViewLUT.Unbind();

    atmosphereScene.Bind();
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Shader atmosphereShader("Shaders/atmospheric.vert", "Shaders/skyAtmosphere/atmosphere.frag");
    atmosphereShader.Activate();
    atmosphereShader.setInt("skyLUT", 0);
    atmosphereShader.setInt("transmittanceLUT", 1);
    atmosphereShader.setInt("multipleScatteredLUT", 2);
    atmosphereShader.setVec2("LUTResolution", glm::vec2(width, height));
    atmosphereShader.setFloat("iTime", currentFrame);
    atmosphereShader.setVec3("camPos", camera->getPosition());
    atmosphereShader.setMat4("viewMatrix", camera->getViewMatrix());
    atmosphereShader.setMat4("projectionMatrix", camera->getProjectionMatrix());
    atmosphereShader.setMat4("invProjection", camera->getInProjectionMatrix());
    atmosphereShader.setMat4("invView", camera->getInViewMatrix());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyViewLUT.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, transmittanceLUT.texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, multipleScatteredLUT.texture);
    Utils::OpenGL::Draw::drawQuad();
    atmosphereScene.Unbind();
}

void DeferredIBLDemo::setupSSAO()
{
    glGenFramebuffers(1, &ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glGenTextures(1, &ssaoTexture);
    glBindTexture(GL_TEXTURE_2D, ssaoTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, AppWindow::width, AppWindow::height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "SSAO Framebuffer not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffers(1, &ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glGenTextures(1, &ssaoBlurTexture);
    glBindTexture(GL_TEXTURE_2D, ssaoBlurTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, AppWindow::width, AppWindow::height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoBlurTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    // generate sample kernel
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
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void DeferredIBLDemo::renderSSAO()
{
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    Shader ssaoShader("Shaders/ssao/ssao.vert", "Shaders/ssao/ssao_depth.frag");
    ssaoShader.Activate();
    ssaoShader.setInt("gDepth", 0);
    ssaoShader.setInt("gNormal", 1);
    ssaoShader.setInt("texNoise", 2);
    ssaoShader.setVec2(
        "noiseScale", 
        glm::vec2(AppWindow::width / 4.0f, AppWindow::height / 4.0f)
    );     // tile noise texture over screen based

    for (unsigned int i = 0; i < 64; ++i) {
        ssaoShader.setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
    }

    ssaoShader.setMat4("invProjection", camera->getInProjectionMatrix());
    ssaoShader.setMat4("invView", camera->getInViewMatrix());
    ssaoShader.setMat4("projection", camera->getProjectionMatrix());
    ssaoShader.setMat4("view", camera->getViewMatrix());
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gDepth);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glActiveTexture(GL_TEXTURE3);
    Utils::OpenGL::Draw::drawQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    Shader blurShader("Shaders/ssao/ssao.vert", "Shaders/ssao/blur.frag"); 
    blurShader.Activate();
    blurShader.setInt("ssaoInput", 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoTexture);
    Utils::OpenGL::Draw::drawQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DeferredIBLDemo::setupSSR()
{
    ssrSceneFBO.Init(AppWindow::width, AppWindow::height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);

}

void DeferredIBLDemo::renderSSR()
{
    Shader SSRShader("Shaders/SSR/SSR.vert", "Shaders/SSR/SSR_PBR.frag");
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
    glBindTexture(GL_TEXTURE_2D, gMetalRoughness);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, gDepth);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, lightPassFBO.texture);

    SSRShader.setMat4("view", camera->getViewMatrix());
    SSRShader.setMat4("projection", camera->getProjectionMatrix());
    SSRShader.setMat4("invView", glm::inverse(camera->getViewMatrix()));
    SSRShader.setMat4("invProjection", glm::inverse(camera->getProjectionMatrix()));
    SSRShader.setInt("width", AppWindow::width);
    SSRShader.setInt("height", AppWindow::height);


    Utils::OpenGL::Draw::drawQuad();

    ssrSceneFBO.Unbind();
}

void DeferredIBLDemo::setupShadow()
{
    Scene& scene = *SceneManager::getInstance().getActiveScene();
    //scene.addShader("shadowShader", "Shaders/shadowMap.vert", "Shaders/shadowMap.frag");
}

void DeferredIBLDemo::renderShadow()
{
    Scene& scene = *SceneManager::getInstance().getActiveScene();
    auto entities = scene.getEntitiesWith<ModelComponent, TransformComponent>();
    auto lightEntities = scene.getEntitiesWith<MLightComponent>();
    auto& light = lightEntities[0].getComponent<MLightComponent>();

    glm::mat4 lightOrtho = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    glm::mat4 lightPerspective = glm::perspective(glm::radians(90.0f), (float)AppWindow::width / AppWindow::height, 0.1f, 100.0f);
    
    glm::mat4 lightView = glm::lookAt(light.position, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 lightMVP = lightOrtho * lightView;

    Shader shadowShader("Shaders/shadowMap.vert", "Shaders/shadowMap.frag");

    depthMap.Bind();
    glViewport(0, 0, AppWindow::width, AppWindow::height);
    glClear(GL_DEPTH_BUFFER_BIT);
    //glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // RGBA
    //glEnable(GL_DEPTH_TEST);

    shadowShader.Activate();
    //shadowShader.setMat4("mvp", cameraComponents[0].getComponent<CameraComponent>().camera->getMVP());
    shadowShader.setMat4("mvp", lightMVP);
    
    for (auto entity : entities) {
        ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
        TransformComponent& transform = entity.getComponent<TransformComponent>();
        
        const glm::mat4& normalMatrix = transform.getModelMatrix();
        std::shared_ptr<Model> model = modelComponent.model.lock();

        bool hasAnimation = entity.hasComponent<AnimationComponent>();

        if (hasAnimation) {
            AnimationComponent& animationComponent = entity.getComponent<AnimationComponent>();
            auto transformBoneMatricies = animationComponent.animator.GetFinalBoneMatrices();

            for (int i = 0; i < transformBoneMatricies.size(); ++i) {
                shadowShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transformBoneMatricies[i]);
            }
        }

        if (model != nullptr) {
            shadowShader.setBool("hasAnimation", hasAnimation);
            shadowShader.setMat4("matrix", transform.getModelMatrix());
            model->Draw(shadowShader);
        }

        else {
            modelComponent.reset();
        }
    }
    depthMap.Unbind();
}
 


void DeferredIBLDemo::setupForwardPass()
{

}

void DeferredIBLDemo::renderForwardPass()
{
    Scene& scene = *SceneManager::getInstance().getActiveScene();

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

    applicationFBO.Bind();
    glViewport(0, 0, AppWindow::width, AppWindow::height);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    //glEnable(GL_CULL_FACE);
    //glFrontFace(GL_CW);
    //glCullFace(GL_FRONT);

    pbrShader.Activate();
    pbrShader.setMat4("matrix", glm::mat4(1.0f));
    pbrShader.setMat4("mvp", camera->getMVP());
    pbrShader.setVec3("camPos", camera->getPosition());
    pbrShader.setBool("gamma", true);

    auto list = scene.getEntitiesWith<CubeMapComponent>();
    CubeMapComponent* cubeMap = nullptr;
    if (!list.empty() && list[0].hasComponent<CubeMapComponent>()) {
        cubeMap = &list[0].getComponent<CubeMapComponent>();
        cubeMap->bindIBL();
    }

    auto models = scene.getEnttEntities<ModelComponent, TransformComponent>();

    for (auto entity : models) {
        ModelComponent& modelComponent = models.get<ModelComponent>(entity);
        TransformComponent& transform = models.get<TransformComponent>(entity);
        const glm::mat4& normalMatrix = transform.getModelMatrix();
        std::shared_ptr<Model> model = modelComponent.model.lock();

        if (scene.hasComponent<CameraComponent>(entity) || scene.hasComponent<AnimationComponent>(entity)) {
            continue;
        }

        if (model != nullptr) {
            pbrShader.setBool("hasAnimation", false);
            pbrShader.setBool("hasEmission", true);
            pbrShader.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(normalMatrix))));
            pbrShader.setMat4("matrix", normalMatrix);
            model->Draw(pbrShader);
        }

        else {
            modelComponent.reset();
        }
    }

    auto animatedModels = scene.getEntitiesWith<AnimationComponent>();
    for (auto& entity : animatedModels) {
        ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
        TransformComponent& transform = entity.getComponent<TransformComponent>();
        const glm::mat4& normalMatrix = transform.getModelMatrix();
        std::shared_ptr<Model> model = modelComponent.model.lock();

        pbrShader.setBool("hasAnimation", true);
        pbrShader.setBool("hasEmission", false);
        pbrShader.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(normalMatrix))));
        pbrShader.setMat4("matrix", normalMatrix);

        AnimationComponent& animationComponent = entity.getComponent<AnimationComponent>();
        auto transformBoneMatricies = animationComponent.animator.GetFinalBoneMatrices();

        for (int i = 0; i < transformBoneMatricies.size(); ++i) {
            pbrShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transformBoneMatricies[i]);
        }
        model->Draw(pbrShader);

        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - lf;
        lf = currentTime;
        animationComponent.animator.UpdateAnimation(dt);
    }

    std::vector<Entity> lights = scene.getEntitiesWith<MLightComponent, TransformComponent>();
    int index = 0;
    for (auto& entity : lights) {
        //BUG: old light stays after removed until new one is added
        //index increment won't work for light added but not removed
        MLightComponent& light = entity.getComponent<MLightComponent>();
        TransformComponent& transform = entity.getComponent<TransformComponent>();
        pbrShader.Activate();
        // hack to simulate light size, only work with uniform scale
        glm::vec3 lightIntensity = light.color * (transform.scaleVec * transform.scaleVec);
        pbrShader.setVec3("lightColors[" + std::to_string(index) + "]", lightIntensity);
        pbrShader.setVec3("lightPositions[" + std::to_string(index) + "]", light.position);

        glm::mat4& model = transform.getModelMatrix();
        light.position = transform.translateVec;

        if (scene.getShader("lightShader")) {
            scene.getShader("lightShader")->Activate();
            scene.getShader("lightShader")->setMat4("matrix", model);
            scene.getShader("lightShader")->setVec3("lightColor", glm::normalize(light.color));
            scene.getShader("lightShader")->setMat4("mvp", camera->getMVP());
        }
        Utils::OpenGL::Draw::drawSphere(sphereVAO, indexCount);
        index++;
    }


    if (cubeMap) {
        cubeMap->reloadTexture(atmosphereScene.texture);
        cubeMap->render(camera);
    }

    applicationFBO.Unbind();
}

void DeferredIBLDemo::renderPrePass()
{
    Scene& scene = *SceneManager::getInstance().getActiveScene();

    scene.addShader("gPassShader", "Shaders/deferredIBL/gPass.vert", "Shaders/deferredIBL/gPass.frag");
    scene.getShader("gPassShader")->Activate();
    scene.getShader("gPassShader")->setInt("albedoMap", 0);
    scene.getShader("gPassShader")->setInt("normalMap", 1);
    scene.getShader("gPassShader")->setInt("metallicMap", 2);
    scene.getShader("gPassShader")->setInt("roughnessMap", 3);
    scene.getShader("gPassShader")->setInt("aoMap", 4);
    scene.getShader("gPassShader")->setInt("emissiveMap", 5);
    scene.getShader("gPassShader")->setInt("duvMap", 6);

    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glViewport(0, 0, AppWindow::width, AppWindow::height);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);


    scene.getShader("gPassShader")->Activate();
    glm::mat4 model = glm::mat4(1.0);
    model = glm::translate(model, glm::vec3(0.0f, -2.0f, 0.0f));
    model = glm::scale(model, glm::vec3(30.0f));
    scene.getShader("gPassShader")->setInt("albedoMap", 0);
    scene.getShader("gPassShader")->setInt("metallicMap", 1.0);
    
    //glActiveTexture(GL_TEXTURE6);
    //glBindTexture(GL_TEXTURE_2D, duvMap);

    std::vector<Entity> entities = scene.getEntitiesWith<ModelComponent, TransformComponent>();

    for (auto& entity : entities) {
        ModelComponent model = entity.getComponent<ModelComponent>();
        glm::mat4& modelMatrix = entity.getComponent<TransformComponent>().getModelMatrix();
        glm::mat4 modelViewNormal = glm::transpose(glm::inverse(camera->getViewMatrix() * modelMatrix));
        
        scene.getShader("gPassShader")->setFloat("reflectiveMap", 1.0);
        scene.getShader("gPassShader")->setBool("invertedNormals", false);
        scene.getShader("gPassShader")->setBool("invertedTexCoords", false);
        scene.getShader("gPassShader")->setMat4("model", modelMatrix);
        scene.getShader("gPassShader")->setMat4("modelViewNormal", modelViewNormal);
        scene.getShader("gPassShader")->setMat4("mvp", camera->getMVP() * modelMatrix);
        scene.getShader("gPassShader")->setMat4("inverseViewMat", camera->getInViewMatrix());

        bool hasAnimation = entity.hasComponent<AnimationComponent>();
        scene.getShader("gPassShader")->setBool("hasAnimation", hasAnimation);

        if (hasAnimation) {
            AnimationComponent& animationComponent = entity.getComponent<AnimationComponent>();
            auto transformBoneMatricies = animationComponent.animator.GetFinalBoneMatrices();
            for (int i = 0; i < transformBoneMatricies.size(); ++i) {
                scene.getShader("gPassShader")->setMat4(
                    "finalBonesMatrices[" + std::to_string(i) + "]",
                    transformBoneMatricies[i]
                );
            }


            float currentTime = static_cast<float>(glfwGetTime());
            float dt = currentTime - lf;
            lf = currentTime;
            animationComponent.animator.UpdateAnimation(dt);
        }

        model.model.lock()->Draw(*scene.getShader("gPassShader"));
    }
    //particleRenderer.render(particleShader, *SceneManager::cameraController, numRender, speed, pause);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

DeferredIBLDemo::DeferredIBLDemo(const std::string& name) : AppLayer(name)
{
    //particleRenderer.init(particleControl);
    pbrShader.Init("Shaders/default-2.vert", "Shaders/default-2.frag");
    particleShader.Init("Shaders/particle.vert", "Shaders/particle.frag");
    lightPassFBO.Init(AppWindow::width, AppWindow::height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
    depthMap.Init(AppWindow::width, AppWindow::height);
}

void DeferredIBLDemo::OnAttach()
{
    AppLayer::OnAttach();

    setupBuffers();
    setupSSAO();
    setupSkyView();
    setupSSR();
    setupShadow();

    SceneManager::cameraController = camera;
    LayerManager::addFrameBuffer("DeferredIBLDemo", applicationFBO);
    Scene& scene = *SceneManager::getInstance().getActiveScene();

    TransformComponent* transform;
    ModelLoadEvent event;
    AnimationLoadEvent animationLoadEvent;

    gDUV = Utils::OpenGL::loadTexture("Textures/wdudv.jpg");

    try {
        uint32_t helmetID = scene.addEntity("helmet");
        event = ModelLoadEvent("Models/DamagedHelmet/gltf/DamagedHelmet.gltf", scene.entities[helmetID]);
        EventManager::getInstance().Publish(event);
        transform = &scene.entities[helmetID].getComponent<TransformComponent>();
        transform->translate(glm::vec3(2.0, 3.0, -3.0));

        uint32_t aruID = scene.addEntity("aru");
        event = ModelLoadEvent("Models/aru/aru.gltf", scene.entities[aruID]);
        EventManager::getInstance().Publish(event);
        animationLoadEvent = AnimationLoadEvent("Models/aru/aru.gltf", scene.entities[aruID]);
        EventManager::getInstance().Publish(animationLoadEvent);
        transform = &scene.entities[aruID].getComponent<TransformComponent>();
        transform->translate(glm::vec3(-5.8, 3.0, 5.8));

        uint32_t terrainID = scene.addEntity("terrain");
        event = ModelLoadEvent("Models/death-valley-terrain/scene.gltf", scene.entities[terrainID]);
        EventManager::getInstance().Publish(event);
        transform = &scene.entities[terrainID].getComponent<TransformComponent>();
        transform->translate(glm::vec3(0.0, -5.0, 0.0));
        transform->rotate(glm::radians(glm::vec3(180.0, 0.0, 0.0)));
        transform->scale(glm::vec3(50.0));

        uint32_t cameraEntityID = scene.addEntity("camera");
        Entity& cameraEntity = scene.entities[cameraEntityID];
        TransformComponent& cameraTransform = cameraEntity.getComponent<TransformComponent>();
        cameraTransform.translate(glm::vec3(-6.5f, 3.5f, 8.5f));
        auto& cameraComponent = cameraEntity.addComponent<CameraComponent>(
            AppWindow::width,
            AppWindow::height,
            cameraTransform.translateVec,
            glm::vec3(0.5, -0.2, -1.0f)
        );
        //auto pos = cameraEntity.getComponent<CameraComponent>().camera->getPosition();
        //Console::println(pos.x, " ", pos.y, " ", pos.z);
        cameraEntity.onCameraComponentAdded();
        cameraTransform.translate(glm::vec3(-6.5f, 3.5f, 8.5f));

        // direction light
        uint32_t sunLightID = scene.addEntity("light " + std::to_string(0));
        Entity sunLightEntity = scene.getEntity(sunLightID);
        auto& lightComponent = sunLightEntity.addComponent<MLightComponent>();
        lightComponent.color = lightColors[0];
        lightComponent.position = sunLightEntity.getComponent<TransformComponent>().translateVec;
        lightComponent.type = DIRECTION_LIGHT;
        sunLightEntity.getComponent<NameComponent>().name = "Sun light";
        sunLightEntity.getComponent<TransformComponent>().translate(lightPositions[0]);
        
        // point lights
        for (int i = 1; i < 4; i++) {
            uint32_t lightID = scene.addEntity("light " + std::to_string(i));
            Entity lightEntity = scene.getEntity(lightID);
            auto& lightComponent = lightEntity.addComponent<MLightComponent>();
            lightComponent.color = lightColors[i];
            lightComponent.position = lightEntity.getComponent<TransformComponent>().translateVec;
            lightEntity.getComponent<TransformComponent>().translate(lightPositions[i]);
        }

        uint32_t cubemapID = scene.addEntity("cubemap");
        Entity cubemapEntity = scene.getEntity(cubemapID);
        cubemapEntity.addComponent<CubeMapComponent>("Textures/hdr/industrial_sunset_02_puresky_1k.hdr");
    }
    catch (std::runtime_error e) {
        Console::error(e.what());
    }

    scene.addShader("lightShader", "Shaders/light.vert", "Shaders/light.frag");
    scene.getShader("lightShader")->Activate();
    scene.getShader("lightShader")->setInt("irradianceMap", 6);

}

void DeferredIBLDemo::OnDetach()
{
    AppLayer::OnDetach();
}

void DeferredIBLDemo::OnUpdate()
{
    AppLayer::OnUpdate();

    renderShadow();
    //renderForwardPass();
    renderPrePass();
    renderSSAO();
    renderSSR();
    renderDeferredPass();
    renderSkyView();
}

void DeferredIBLDemo::OnGuiUpdate()
{
    Scene& scene = *SceneManager::getInstance().getActiveScene();

    if (ImGui::Begin("control")) {
        ImVec2 wsize = ImGui::GetWindowSize();

        ImGui::Checkbox("Pause Time", &timePaused);
        ImGui::DragFloat("Falling speed", &speed, 0.01, -10.0, 10.0);
        ImGui::DragInt("Num Instances", &numRender, particleControl.numInstances / 100.0, 0, particleControl.numInstances, 0, true);
        if (ImGui::DragFloat3("Spawn Area", glm::value_ptr(particleControl.spawnArea), 0.1, 0, 1000.0, 0, true)) {
            particleRenderer.clear();
            particleRenderer.init(particleControl);
        }
        ImGui::Checkbox("Pause", &pause);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            particleRenderer.reset();
        }


        //ImGui::Text("forward lighting");
        //ImGui::Image((ImTextureID)applicationFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("shadow texture");
        ImGui::Image((ImTextureID)depthMap.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("sky view LUT");
        ImGui::Image((ImTextureID)skyViewLUT.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("atmostphere sky");
        ImGui::Image((ImTextureID)atmosphereScene.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("SSAO");
        ImGui::Image((ImTextureID)ssaoTexture, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("Blured SSAO");
        ImGui::Image((ImTextureID)ssaoBlurTexture, wsize, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::Text("gAlbedo");
        ImGui::Image((ImTextureID)gAlbedo, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("gNormal");
        ImGui::Image((ImTextureID)gNormal, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("gMetalRoughness");
        ImGui::Image((ImTextureID)gMetalRoughness, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("gEmissive");
        ImGui::Image((ImTextureID)gEmissive, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("gDUV");
        ImGui::Image((ImTextureID)gDUV, wsize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Text("gDepth");
        ImGui::Image((ImTextureID)gDepth, wsize, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::End();

    }
    
    AppLayer::renderApplication(ssrSceneFBO.texture);
}

void DeferredIBLDemo::OnEvent(Event& event)
{
    AppLayer::OnEvent(event);
}

int DeferredIBLDemo::show_demo()
{
	return 0;
}

int DeferredIBLDemo::run()
{
	return show_demo();
}
