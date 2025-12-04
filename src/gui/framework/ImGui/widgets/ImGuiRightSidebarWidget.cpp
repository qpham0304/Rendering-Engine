#include "ImGuiRightSidebarWidget.h"

#include "../../src/core/scene/SceneManager.h"
#include "../../src/core/layers/LayerManager.h"
#include "../../src/core/components/MComponent.h"
#include "../../src/core/components/CubeMapComponent.h"

ImGuiRightSidebarWidget::ImGuiRightSidebarWidget() 
    :   RightSidebarWidget(),
        popupOpen(false),
        selectedTexture(0)
{

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

        ImGui::Image(id, displaySize);

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
            auto& modelComponent = entity.getComponent<ModelComponent>();
            std::shared_ptr modelPtr = modelComponent.model.lock();
            if (modelPtr) {
                for (auto [path, texture] : modelPtr->loaded_textures) {
                    ImGui::PushID(path.c_str());
                    ImGui::Separator();
                    ImGui::Image((ImTextureID)texture.id(), wsize, ImVec2(0, 1), ImVec2(1, 0));
                    ImGui::PopID();

                    if (ImGui::IsItemClicked()) {
                        selectedTexture = texture.id();
                        ImGui::OpenPopup("Image View");
                        popupOpen = true;
                    }

                    ImGui::SameLine();
                    ImGui::TextWrapped(path.c_str());
                }
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
            uint32_t cubemapID = scene.addEntity("cubemap");
            Entity cubemapEntity = scene.getEntity(cubemapID);
            std::string path = Utils::fileDialog();
            cubemapEntity.addComponent<CubeMapComponent>(path);
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
