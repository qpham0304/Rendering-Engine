//#include "graphics/renderers/DeferredRenderer.h"
//#include "core/components/LightComponent.h"
//#include "core/components/MComponent.h"
//#include "core/features/Camera.h"
//
//DeferredRenderer::DeferredRenderer(const int width, const int height)
//{
//    this->width = width;
//    this->height = height;
//	geometryShader.reset(new ShaderOpenGL("assets/Shaders/deferredShading/gBuffer.vert", "assets/Shaders/deferredShading/gBuffer.frag"));
//    colorShader.reset(new ShaderOpenGL("assets/Shaders/deferredShading/deferredShading.vert", "assets/Shaders/deferredShading/deferredShading.frag"));
//
//    glGenFramebuffers(1, &gBuffer);
//	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
//
//    // - position color buffer we need high precision so use 16/32 bit float per component
//    // Use RGBA16F instead of RGB16F since gpu has better byte alignments for 4-component formats
//    glGenTextures(1, &gPosition);
//    glBindTexture(GL_TEXTURE_2D, gPosition);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);
//
//    // - normal color buffer we need high precision so use 16/32 bit float per component
//    glGenTextures(1, &gNormal);
//    glBindTexture(GL_TEXTURE_2D, gNormal);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);
//
//    // - color + specular color buffer combine to avoid create an extra color buffer texture
//    glGenTextures(1, &gAlbedoSpec);
//    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);
//
//
//    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
//    glDrawBuffers(3, attachments);
//
//    glGenRenderbuffers(1, &rboDepth);
//    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
//    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
//    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
//    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
//        std::cout << "Framebuffer not complete!" << std::endl;
//
//
//    glGenTextures(1, &gDepth);
//    glBindTexture(GL_TEXTURE_2D, gDepth);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, width, height, 0,
//        GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 0
//    );
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, gDepth, 0);
//
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
//
//
//
//}
//
//void DeferredRenderer::renderGeometry(Camera* cameraPtr, std::vector<Component*>& components)
//{
//    Camera& camera = *cameraPtr;
//    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//    glm::mat4 projection = glm::perspective(glm::radians(camera.getFOV()), (float)width / (float)height, 0.1f, 100.0f);
//    glm::mat4 view = camera.getViewMatrix();
//    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(0.2));
//    geometryShader->Activate();
//    //geometryShader->setInt("albedoMap", 0);
//    //geometryShader->setInt("metallicMap", 1);
//    geometryShader->setFloat("reflectiveMap", 0.0f);
//    geometryShader->setMat4("projection", projection);
//    geometryShader->setMat4("view", view);
//    for (unsigned int i = 0; i < components.size(); i++) {
//        geometryShader->setMat4("model", components[i]->getModelMatrix());
//        components[i]->model_ptr->Draw(*geometryShader);
//    }
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}
//
//void DeferredRenderer::renderGeometry(Camera* cameraPtr, Component& component)
//{
//    Camera& camera = *cameraPtr;
//    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//    glm::mat4 projection = glm::perspective(glm::radians(camera.getFOV()), (float)width / (float)height, 0.1f, 100.0f);
//    glm::mat4 model = glm::mat4(1.0f);
//    geometryShader->Activate();
//    geometryShader->setMat4("projection", projection);
//    geometryShader->setMat4("view", camera.getViewMatrix());
//
//    std::vector<glm::vec3> objectPositions;
//    objectPositions.push_back(glm::vec3(-3.0, -0.5, -3.0));
//    objectPositions.push_back(glm::vec3(0.0, -0.5, -3.0));
//    objectPositions.push_back(glm::vec3(3.0, -0.5, -3.0));
//    objectPositions.push_back(glm::vec3(-3.0, -0.5, 0.0));
//    objectPositions.push_back(glm::vec3(0.0, -0.5, 0.0));
//    objectPositions.push_back(glm::vec3(3.0, -0.5, 0.0));
//    objectPositions.push_back(glm::vec3(-3.0, -0.5, 3.0));
//    objectPositions.push_back(glm::vec3(0.0, -0.5, 3.0));
//    objectPositions.push_back(glm::vec3(3.0, -0.5, 3.0));
//    for (unsigned int i = 0; i < objectPositions.size(); i++) {
//        component.translate(objectPositions[i]);
//        component.scale(glm::vec3(0.5f));
//        geometryShader->setMat4("model", component.getModelMatrix());
//        component.model_ptr->Draw(*geometryShader);
//    }
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}
//
//void DeferredRenderer::renderColor(Camera* cameraPtr, std::vector<Light>& lights)
//{
//    Camera& camera = *cameraPtr;
//    colorShader->Activate();
//    colorShader->setInt("gPosition", 0);
//    colorShader->setInt("gNormal", 1);
//    colorShader->setInt("gAlbedoSpec", 2);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//    glActiveTexture(GL_TEXTURE0);
//    glBindTexture(GL_TEXTURE_2D, gPosition);
//    glActiveTexture(GL_TEXTURE1);
//    glBindTexture(GL_TEXTURE_2D, gNormal);
//    glActiveTexture(GL_TEXTURE2);
//    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
//
//    for (unsigned int i = 0; i < lights.size(); i++)
//    {
//        colorShader->setVec3("lights[" + std::to_string(i) + "].Position", lights[i].position);
//        colorShader->setVec3("lights[" + std::to_string(i) + "].Color", lights[i].color);
//        // update attenuation parameters and calculate radius
//        const float constant = 1.0f; // note that we don't send this to the shader, we assume it is always 1.0 (in our case)
//        const float linear = 0.7f;
//        const float quadratic = 1.8f;
//        colorShader->setFloat("lights[" + std::to_string(i) + "].Linear", linear);
//        colorShader->setFloat("lights[" + std::to_string(i) + "].Quadratic", quadratic);
//        // then calculate radius of light volume/sphere
//        const float maxBrightness = std::fmaxf(std::fmaxf(lights[i].color.r, lights[i].color.g), lights[i].color.b);
//        float radius = (-linear + std::sqrt(linear * linear - 4 * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
//        colorShader->setFloat("lights[" + std::to_string(i) + "].Radius", radius);
//    }
//    colorShader->setVec3("viewPos", camera.getPosition());
//    Utils::OpenGL::Draw::drawQuad();
//}
//
//unsigned int DeferredRenderer::getGBuffer()
//{
//    return gBuffer;
//}
//
//unsigned int DeferredRenderer::getGPosition()
//{
//    return gPosition;
//}
//
//unsigned int DeferredRenderer::getGNormal()
//{
//    return gNormal;
//}
//
//unsigned int DeferredRenderer::getGColorspec()
//{
//    return gColorSpec;
//}
//
//unsigned int DeferredRenderer::getGAlbedoSpec()
//{
//    return gAlbedoSpec;
//}
//
//unsigned int DeferredRenderer::getGDepth()
//{
//    return gDepth;
//}
