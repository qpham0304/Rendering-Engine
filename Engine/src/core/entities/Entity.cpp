#include "Entity.h"
#include "../scene/SceneManager.h"
#include "../components/MComponent.h"
#include "src/graphics/framework/OpenGL/core/TextureOpenGL.h"   //TOO remove this dependency on camera creation

std::string ACTIVE_SCENE = "default";

Entity::Entity(const entt::entity& entity, entt::registry& registry)
    : entity(entity), registry(&registry)
{
}

bool Entity::operator==(const Entity& other) const
{
    return entity == (entt::entity) other.getID() && registry == other.registry;
}

bool Entity::operator!=(const Entity& other) const
{
    return !operator==(other);
}

const uint32_t& Entity::getID() const
{
    return (uint32_t)entity;
}

void Entity::onCameraComponentAdded()
{
    if (!this->hasComponent<ModelComponent>()) {
        Scene& scene = *SceneManager::getInstance().getActiveScene();
        auto& modelComponent = this->addComponent<ModelComponent>();

        std::vector<MeshOpenGL> meshes;
        std::vector<MeshOpenGL::Vertex> vertices;
        std::vector<GLuint> indices;
        std::vector<Texture>textures;

        std::vector<MeshOpenGL::Vertex> quadMeshVertices = {
            // Positions            // Colors             // TexCoords   // Normals
            MeshOpenGL::Vertex{glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},  // Bottom-left
            MeshOpenGL::Vertex{glm::vec3(0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},  // Bottom-right
            MeshOpenGL::Vertex{glm::vec3(0.5f,  0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f)},  // Top-right
            MeshOpenGL::Vertex{glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f)},  // Top-left
        };

        std::vector<unsigned int> quadMeshIndices = {
            0, 1, 2,
            2, 3, 0
        };

        TextureOpenGL albedo("assets/Textures/mobi-padoru.png", "albedo");
        TextureOpenGL normal("assets/Textures/default/32x32/normal.png", "normalMap");
        TextureOpenGL metallic("assets/Textures/default/32x32/metallic.png", "metallicMap");
        TextureOpenGL roughness("assets/Textures/default/32x32/roughness.png", "roughnessMap");
        TextureOpenGL ao("assets/Textures/default/32x32/ao.png", "aoMap");
        TextureOpenGL emissive("assets/Textures/default/32x32/emissive.png", "emissiveMap");

        MeshOpenGL quadMesh(quadMeshVertices, quadMeshIndices, { albedo, normal, metallic, roughness, ao, emissive });
        meshes.push_back(quadMesh);
        std::string uuid = SceneManager::getInstance().addModelFromMeshes(meshes);
        if (!uuid.empty()) {
            modelComponent.model = SceneManager::getInstance().models[uuid];
        }
        TransformComponent& transform = scene.entities[this->getID()].getComponent<TransformComponent>();
        transform.translate(glm::vec3(0.0, 1.0, 0.0));
        transform.scale(glm::vec3(5.0));
    }
}
