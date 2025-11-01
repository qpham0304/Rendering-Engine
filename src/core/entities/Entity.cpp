#include "Entity.h"
#include "../scene/SceneManager.h"
#include "../components/MComponent.h"

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

        std::vector<Mesh> meshes;
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        std::vector<Texture>textures;

        std::vector<Vertex> quadMeshVertices = {
            // Positions            // Colors             // TexCoords   // Normals
            Vertex{glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},  // Bottom-left
            Vertex{glm::vec3(0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},  // Bottom-right
            Vertex{glm::vec3(0.5f,  0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f)},  // Top-right
            Vertex{glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f)},  // Top-left
        };

        std::vector<unsigned int> quadMeshIndices = {
            0, 1, 2,
            2, 3, 0
        };

        Texture albedo("Textures/mobi-padoru.png", "albedo");
        Texture normal("Textures/default/32x32/normal.png", "normalMap");
        Texture metallic("Textures/default/32x32/metallic.png", "metallicMap");
        Texture roughness("Textures/default/32x32/roughness.png", "roughnessMap");
        Texture ao("Textures/default/32x32/ao.png", "aoMap");
        Texture emissive("Textures/default/32x32/emissive.png", "emissiveMap");

        Mesh quadMesh(quadMeshVertices, quadMeshIndices, { albedo, normal, metallic, roughness, ao, emissive });
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
