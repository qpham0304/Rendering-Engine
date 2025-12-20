#include "BloomPass.h"
#include "core/features/Camera.h"
#include "core/layers/layerManager.h"
#include "core/scene/SceneManager.h"// this should be gone once ECS is done setting up
#include "window/appwindow.h"
#include "../../graphics/utils/Utils.h"

BloomPass::BloomPass(const std::string& name) : Layer(name), VAO(0), VBO(0)
{
    int width = AppWindow::getWidth();
    int height = AppWindow::getHeight();
	bloomRenderer.Init(width, height);

    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
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

    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    bloomShader.reset(new ShaderOpenGL("assets/Shaders/bloom/bloom.vert", "assets/Shaders/bloom/bloom.frag"));
    bloomShader->Activate();
    bloomShader->setInt("scene", 0);
    bloomShader->setInt("bloomBlur", 1);
}

void BloomPass::onAttach(LayerManager* manager)
{

}

void BloomPass::onDetach()
{

}

void BloomPass::onUpdate()
{
    ShaderOpenGL lightShader("assets/Shaders/light.vert", "assets/Shaders/light.frag");
    ShaderOpenGL bloomShader("assets/Shaders/bloom/bloom.vert", "assets/Shaders/bloom/bloom.frag");

    ShaderOpenGL frameShaderProgram("src/apps/frame-buffer/framebuffer.vert", "src/apps/frame-buffer/framebuffer.frag");
    frameShaderProgram.Activate();
    frameShaderProgram.setFloat("screenTexture", 0);

    ShaderOpenGL quadShader("assets/Shaders/postProcess/renderQuad.vert", "assets/Shaders/postProcess/renderQuad.frag");
    quadShader.Activate();
    quadShader.setInt("scene", 0);
    quadShader.setInt("effect", 1);

	//auto fbo = LayerManager::getFrameBuffer("ParticleDemo");
	auto fbo = LayerManager::getFrameBuffer("DeferredIBLDemo");
	if (fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        lightShader.Activate();
        if (SceneManager::cameraController) {
            lightShader.setMat4("mvp", SceneManager::cameraController->getMVP());
        }
        lightShader.setMat4("matrix", glm::mat4(1.0));
        lightShader.setVec3("lightColor", glm::vec3(0.7, 0.8, 0.5));
        Utils::OpenGL::Draw::drawCube(VAO, VBO);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        bloomRenderer.RenderBloomTexture(colorBuffers[1], 0.005f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        bloomShader.Activate();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomRenderer.BloomTexture());

        bloomShader.setInt("bloomOn", true);
        bloomShader.setFloat("exposure", 1.0);
        Utils::OpenGL::Draw::drawQuad();


		fbo->Bind();
        quadShader.Activate();
        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo->texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomRenderer.BloomTexture());
        Utils::OpenGL::Draw::drawQuad();
		fbo->Unbind();
	}
}

void BloomPass::onGuiUpdate()
{
    // ImVec2 wsize = ImGui::GetWindowSize();
    // ImGui::Image((ImTextureID)colorBuffers[0], wsize, ImVec2(0, 1), ImVec2(1, 0));
    // ImGui::Image((ImTextureID)colorBuffers[1], wsize, ImVec2(0, 1), ImVec2(1, 0));
    // ImGui::Image((ImTextureID)bloomRenderer.BloomTexture(), wsize, ImVec2(0, 1), ImVec2(1, 0));
}

void BloomPass::onEvent(Event& event)
{

}
