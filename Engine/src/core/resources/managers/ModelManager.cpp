#include "ModelManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "SkeletonManager.h"
#include "MaterialManager.h"
#include "logging/Logger.h"
#include "animation/AnimationManager.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Material.h"
#include "core/events/EventManager.h"
#include "core/events/Event.h"
#include "core/features/EngineUtils.h"
#include "core/resources/loaders/ModelLoaderGLTF.h"
#include "core/resources/loaders/ModelLoaderFBX.h"
#include "core/resources/loaders/ModelLoaderAssimp.h"

#include <functional>
#include <algorithm>
#include <set>
#include <filesystem>
#include <cctype>

const std::unordered_map<std::string, std::function<std::pair<std::string, Mesh>()>> meshPrimitives = {
    { "$prim$Sphere", []() { return std::pair<std::string, Mesh>( "$prim$Sphere", EngineUtils::drawSphere(0.5f, 36, 36) ); }},
    { "$prim$Quad",   []() { return std::pair<std::string, Mesh>( "$prim$Quad", EngineUtils::drawQuad() ); }},
    { "$prim$Cube",   []() { return std::pair<std::string, Mesh>( "$prim$Cube", EngineUtils::drawCube(1.0) ); } }
    // { "$prim$Capsule",[]() { return std::pair<std::string, Mesh>( "$prim$Capsule", EngineUtils::drawCapsule(1.0) ); } },
    // { "$prim$Convex", []() { return std::pair<std::string, Mesh>( "$prim$Convex", EngineUtils::drawConvex(1.0) ); } }
};


ModelManager::ModelManager()
    :   Manager("ModelManager")
{
    setFootprint("ModelManager", sizeof(ModelManager), MemorySystem::Resource);

}

ModelManager::~ModelManager()
{

}

bool ModelManager::init(WindowConfig config)
{
    Service::init(config);

    textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    skeletonManager = &ServiceLocator::GetService<SkeletonManager>("SkeletonManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
    animationManager = &ServiceLocator::GetService<AnimationManager>("AnimationManager");
    
    if (!(m_logger && textureManager && meshManager && materialManager)) {
        return false;
    }

    const ModelLoader::Context loaderContext{ m_logger.get(), textureManager, meshManager, skeletonManager, materialManager, animationManager };
    m_loaderGLTF = std::make_unique<ModelLoaderGLTF>(loaderContext);
    m_loaderFBX = std::make_unique<ModelLoaderFBX>(loaderContext);
    m_loaderAssimp = std::make_unique<ModelLoaderAssimp>(loaderContext);

    EventManager& eventManager = EventManager::getInstance();
    m_eventSubscriptions.push_back(eventManager.subscribe(EventType::ModelLoadEvent, [&] (Event& event) {
        ModelLoadEvent& e = static_cast<ModelLoadEvent&>(event);

        if (!e.entity.hasComponent<ModelComponent>()) {
			e.entity.addComponent<ModelComponent>();
		}

        ModelComponent& modelComponent = e.entity.getComponent<ModelComponent>();
        auto it = meshPrimitives.find(e.path);
        if(it != meshPrimitives.end()) {
            MaterialDesc materialDesc {};
            
            auto pair = it->second();
            Mesh mesh = pair.second;
            mesh.materialID = materialManager->createMaterial(materialDesc);

            Model model {};
            uint32_t meshID = meshManager->loadMesh(mesh);
            model.meshIDs.push_back(meshID);
            model.path = pair.first;

            // single root node skeleton, each primitive instance owns its own mesh so the name must be unique
            // otherwise addSkeleton returns the existing one by name
            std::string skeletonName = pair.first + "_" + std::to_string(m_ids);
            model.skeletonID = skeletonManager->addSkeleton(skeletonName);
            auto skeleton = skeletonManager->getSkeleton(model.skeletonID);
            if (skeleton) {
                SkeletonManager::SkeletonNode node{};
                node.name = pair.first;
                node.meshIDs.push_back(meshID);
                skeleton->rootIndex = 0;
                skeleton->boneMap[node.name] = 0;
                skeleton->nodes.push_back(node);
            }

            modelComponent.modelID = addModel(model);
        }

        else {
            modelComponent.path = "Loading...";
            uint32_t modelID = loadModel(e.path);

            if (modelID != 0) {
                modelComponent.path = e.path;
                modelComponent.modelID = modelID;
            }
            
            else {
                //TODO: imgui or a widget register an event to popup a warning message
                //ideally editor handle it or supress it for run time version
                std::string message = "Failed to load Model from path: " + e.path;
                m_logger->error(message);

                GuiMessageEvent failEvent(message);
                eventManager.publish(failEvent);

                // e.entity.removeComponent<ModelComponent>();
                modelComponent.path = "None";
            }
        }
    }));

    return true;
}

bool ModelManager::onClose()
{
    WriteLock lock = _lockWrite();
    m_models.clear();
    m_modelData.clear();

    return true;
}

void ModelManager::destroy(uint32_t id)
{
    WriteLock lock = _lockWrite();
    if(m_models.find(id) == m_models.end()) {
        m_logger->warn("Model not found id: {}", id);
        return;
    }
    m_models.erase(id); //TODO: might want queue to remove next frame if something goes wrong
}

std::vector<uint32_t> ModelManager::listIDs() const
{
    std::vector<uint32_t> list;
    for(const auto& [id, model] : m_models) {
        list.emplace_back(id);
    }
    return list;
}

uint32_t ModelManager::loadModel(std::string_view path)
{
    if (m_modelData.find(path.data()) != m_modelData.end()) {
        return m_modelData.at(path.data());;
    }

    // primitives are generated instead of imported, one shared copy per path
    auto primitive = meshPrimitives.find(path.data());
    if(primitive != meshPrimitives.end()) {
        Mesh mesh = primitive->second().second;
        mesh.materialID = materialManager->createMaterial(MaterialDesc{});

        Model model {};
        model.meshIDs.push_back(meshManager->loadMesh(mesh));
        model.path = path.data();
        m_modelData[path.data()] = m_ids;
        return addModel(model);
    }

    std::string key(path);
    if(m_failedPaths.contains(key)) {
        return 0;
    }

    bool loaded = false;
    try {
        loaded = _loadModel(key);
    }
    catch (const std::exception& e) {
        m_logger->error("Model Loading failed: {}: {}", key, e.what());
    }

    if(!loaded) {
        m_failedPaths.insert(key);
        return 0;
    }

    return _assignID();
}

void ModelManager::clearFailedLoads()
{
    m_failedPaths.clear();
}

std::vector<std::string> ModelManager::listPrimitives()
{
    std::vector<std::string> names;
    for(const auto& [name, generator] : meshPrimitives) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> ModelManager::listModelPaths()
{
    ReadLock lock = _lockRead();
    std::set<std::string> paths;
    for(const auto& [id, model] : m_models) {
        if(model && !model->path.empty()) {
            paths.insert(model->path);
        }
    }
    return { paths.begin(), paths.end() };
}

uint32_t ModelManager::addModel(Model& model)
{
    m_models[m_ids] = std::make_shared<Model>(std::move(model));
    return _assignID();
}

Model* ModelManager::getModel(uint32_t id) const
{
    if (m_models.find(id) == m_models.end()) {
        return nullptr;
    }
    return m_models.at(id).get();
}

bool ModelManager::_loadModel(std::string_view path)
{
    std::string extension = std::filesystem::path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    ModelLoader* loader = m_loaderAssimp.get();
    if (extension == ".gltf" || extension == ".glb") {
        loader = m_loaderGLTF.get();
    } else if (extension == ".fbx") {
        loader = m_loaderFBX.get();
    }

    Model model{};
    if (!loader->load(path, model)) {
        return false;
    }

    m_modelData[path.data()] = m_ids;
    m_models[m_ids] = std::make_shared<Model>(std::move(model));
    return true;
}
