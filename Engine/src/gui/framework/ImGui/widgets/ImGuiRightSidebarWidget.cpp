#include "ImGuiRightSidebarWidget.h"

#include "core/scene/SceneManager.h"
#include "core/layers/LayerManager.h"
#include "core/components/MComponent.h"
#include "core/components/CubeMapComponent.h"
#include "graphics/utils/Utils.h"
#include "window/AppWindow.h"
#include "logging/Logger.h"
#include "graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureVulkan.h"
#include "vulkan/vulkan.h" //TODO: remove dependency

ImGuiRightSidebarWidget::ImGuiRightSidebarWidget() 
    :   RightSidebarWidget(),
        popupOpen(false),
        selectedTexture(0)
{
	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = &static_cast<DescriptorManagerVulkan&>(descriptorManager);	//TODO: move to use glue file

	if(AppWindow::getWindowConfig().renderPlatform == RenderPlatform::VULKAN) {
		_createViewDescriptorBind();	// texture view layout and pool
	}
}

void ImGuiRightSidebarWidget::TextureModal(const ImTextureID& id) {
    //ImVec2 appSize = ImGui::GetIO().DisplaySize;
    //ImVec2 popupSize = ImVec2(appSize.y * 0.75f, appSize.y * 0.75f);
    //ImGui::SetNextWindowSize(popupSize);

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0, 0.0, 0.0, 0.5);
    if (ImGui::BeginPopupModal("Image View", &popupOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        //ImVec2 availableSize = ImGui::GetContentRegionAvail();
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 availableSize = ImVec2(viewport->Size.y * 0.75, viewport->Size.y * 0.75);
        float aspectRatio = 1.0 / 1.0;

        ImVec2 displaySize;
        if (availableSize.x / availableSize.y > aspectRatio) {
            displaySize.y = availableSize.y;
            displaySize.x = availableSize.y * aspectRatio;
        }
        else {
            displaySize.x = availableSize.x;
            displaySize.y = availableSize.x / aspectRatio;
        }

        VkDescriptorSet VkDescriptorSet = descriptorManagerVulkan->getDescriptorSet(textureIDs[id])[0];
        ImGui::Image(ImTextureID(VkDescriptorSet), displaySize);

        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PopStyleVar();

        if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ImGuiRightSidebarWidget::layersControl()
{
    ImGui::Begin("Layers");

    ImGui::End();
}

void ImGuiRightSidebarWidget::textureView()
{
    ImGui::Begin("Texture View");
    TextureModal((ImTextureID)selectedTexture);

    Scene* scene = SceneManager::getInstance().getActiveScene();
    auto selectedEntities = scene->getSelectedEntities();
    for (auto& entity : selectedEntities) {
        ImVec2 wsize = ImGui::GetWindowSize();
        wsize.x /= 5;
        wsize.y = wsize.x;
        if (entity.hasComponent<ModelComponent>()) {
            uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
            const Model* model = modelManager->getModel(modelID);

            if(scene->getSelectedMeshID() == 0){
                continue;
            }

            // printf("selected mesh ID: %d", scene->getSelectedMeshID());
            const Mesh* mesh = meshManager->getMesh(scene->getSelectedMeshID());
            MaterialDesc meshDesc = materialManager->getMaterial(mesh->materialID);

            std::vector<uint32_t> ids = {
                meshDesc.albedoIDs[0],
                meshDesc.normalIDs[0],
                meshDesc.metallicIDs[0],
                meshDesc.roughnessIDs[0],
                meshDesc.aoIDs[0],
                meshDesc.emissiveIDs[0],
            };

            for (auto& id : ids) {
                textureIDs[id] = _createViewDescriptorSet(id);
                Texture* texture = textureManager->getTexture(id);
                ImGui::PushID(texture->path().c_str());
                ImGui::Separator();
                VkDescriptorSet VkDescriptorSet = descriptorManagerVulkan->getDescriptorSet(textureIDs[id])[0];
                ImGui::Image((ImTextureID)VkDescriptorSet, wsize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::PopID();
                
                if (ImGui::IsItemClicked()) {
                    selectedTexture = id;
                    ImGui::OpenPopup("Image View");
                    popupOpen = true;
                }

                ImGui::SameLine();
                ImGui::TextWrapped(texture->path().c_str());
            }
        }
    }
    ImGui::End();
}

void ImGuiRightSidebarWidget::environmentControl()
{
    Scene& scene = *SceneManager::getInstance().getActiveScene();
    auto list = scene.getEntitiesWith<CubeMapComponent>();
    CubeMapComponent* cubeMap = nullptr;

    if (!list.empty() && list[0].hasComponent<CubeMapComponent>()) {
        cubeMap = &list[0].getComponent<CubeMapComponent>();
    }

    ImGui::Begin("Environment Control");

    ImVec2 wsize = ImGui::GetWindowSize();
    int wWidth = static_cast<int>(ImGui::GetWindowWidth());
    int wHeight = static_cast<int>(ImGui::GetWindowHeight());

    if (cubeMap) {

        if (ImGui::Button("Change Cubemap Texture", ImVec2(-1.0, 0.0))) {
            auto list = scene.getEntitiesWith<CubeMapComponent>();

            std::string path;
            path = Utils::fileDialog();
            if (!path.empty()) {

                //#define USE_THREAD
#ifdef USE_THREAD
                AsyncEvent event;
                auto function = [this, cubeMap, path](AsyncEvent& event) mutable {
                    glfwMakeContextCurrent(AppWindow::sharedWindow);            //EXPERIMENTATION
                    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                        std::cerr << "Failed to initialize GLAD for shared context" << std::endl;
                        return;
                    }
                    if (cubeMap) {
                        cubeMap->reloadTexture(path);
                    }
                    glfwMakeContextCurrent(AppWindow::window);
                    };
                EventManager::getInstance().queue(event, function);
#else
                if (cubeMap) {
                    cubeMap->reloadTexture(path);
                }
#endif

            }

            else {
                cubeMap->reloadTexture();
            }
        }

    }

    else {
        if (ImGui::Button("+ Add Cubemap", ImVec2(-1.0, 0.0))) {
            // uint32_t cubemapID = scene.addEntity("cubemap");
            // Entity cubemapEntity = scene.getEntity(cubemapID);
            // std::string path = Utils::fileDialog();
            // cubemapEntity.addComponent<CubeMapComponent>(path);
            std::runtime_error("add cubemap unimplemented");
        }
    }



    ImGui::End();
}

void ImGuiRightSidebarWidget::render()
{
    Scene* scene = SceneManager::getInstance().getActiveScene();

    ImGui::BeginGroup();
    if (scene) {
        layersControl();
        textureView();
        environmentControl();
    }
    ImGui::EndGroup();

}

void ImGuiRightSidebarWidget::_listTextureManager()
{
	ImGui::Begin("Textures");

	if(AppWindow::getWindowConfig().renderPlatform == RenderPlatform::VULKAN) {
		std::vector<uint32_t> ids = textureManager->listIDs();
		for (auto& id : ids) {
			textureIDs[id] = _createViewDescriptorSet(id);
			ImGui::Text(std::to_string(id).c_str());
			ImGui::Begin("Texture View");
			ImGui::BeginChild("Image View");
			VkDescriptorSet VkDescriptorSet = descriptorManagerVulkan->getDescriptorSet(textureIDs[id])[0];
			ImGui::Image((ImTextureID)VkDescriptorSet, ImVec2(250, 250));
			ImGui::EndChild();
			ImGui::End();
		}
	}
	
	std::vector<uint32_t> ids = materialManager->listIDs();
	for(auto& id : ids){
	//	ImGui::Text(std::to_string(id).c_str());
	//	MaterialDesc material = materialManager->getMaterial(id);
	//	ImGui::Begin("Texture View");
	//	ImGui::BeginChild("Image View");
	//	ImGui::Image((ImTextureID)material.albedoIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.albedoIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.normalIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.normalIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.metallicIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.metallicIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.roughnessIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.roughnessIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.aoIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.aoIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.emissiveIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.emissiveIDs[0]).c_str());
	//	ImGui::EndChild();
	//	ImGui::End();
	}
	ImGui::End();
}

void ImGuiRightSidebarWidget::_createViewDescriptorBind()
{
	const uint32_t MAX_NUM_SETS = 1000;				// add more if requires more
	
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::vector<VkDescriptorSetLayoutBinding> bindings = { samplerLayoutBinding };
	imGuilayoutID = descriptorManagerVulkan->createLayout(bindings);

	std::vector<VkDescriptorPoolSize> poolSizes = { 
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} 
	};
	imGuipoolID = descriptorManagerVulkan->createPool(poolSizes, MAX_NUM_SETS);
}

uint32_t ImGuiRightSidebarWidget::_createViewDescriptorSet(uint32_t id)
{
	if(textureIDs.find(id) != textureIDs.end()) {
		return textureIDs[id];
	}

	Texture* texture = textureManager->getTexture(id);
	TextureVulkan* textureVulkan = dynamic_cast<TextureVulkan*>(texture);
	if(!textureVulkan){
		throw std::runtime_error("ImGui createViewDescriptorSet: failed to retrieve texture");
	}

	uint32_t imguiSetID = descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1);
	VkDescriptorSet imguiTextureDescriptorSet = descriptorManagerVulkan->getDescriptorSet(imguiSetID)[0];

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = textureVulkan->textureImageView;
	imageInfo.sampler = textureVulkan->textureSampler;

	std::vector<VkWriteDescriptorSet> writes{};
	descriptorManagerVulkan->writeImage(&writes, imguiTextureDescriptorSet, writes.size(), imageInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);

	return imguiSetID;
}
