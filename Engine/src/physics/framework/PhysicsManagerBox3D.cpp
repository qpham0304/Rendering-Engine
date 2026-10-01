#include "PhysicsManagerBox3D.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/components/MComponent.h"
#include "core/entities/Entity.h"
#include "window/AppWindow.h"
#include "core/features/camera.h"

b3Vec3 toB3Vec3(glm::vec3 v) {
    return b3Vec3({ v.x, v.y, v.z });
}

b3Matrix3 toB3Mat3(glm::mat3 mat) {
    return { toB3Vec3(mat[0]), toB3Vec3(mat[1]), toB3Vec3(mat[2]) };
}

glm::vec3 b3ToGlmVec3(b3Vec3 v) {
    return glm::vec3({ v.x, v.y, v.z });
}

glm::mat3 b3ToGlmMat3(b3Matrix3 mat) {
    return { b3ToGlmVec3(mat.cx), b3ToGlmVec3(mat.cy), b3ToGlmVec3(mat.cz) };
}

PhysicsManagerBox3D::PhysicsManagerBox3D(std::string serviceName)
    : PhysicsManager(serviceName)
{
    
}

PhysicsManagerBox3D::~PhysicsManagerBox3D()
{
    
}

bool PhysicsManagerBox3D::init(WindowConfig config)
{
    Service::init(config);

    m_shapeIDs.clear();

    m_worldDef = b3DefaultWorldDef();
    m_worldDef.gravity = {0.0f, -9.81f, 0.0f};
    m_worldId = b3CreateWorld(&m_worldDef);

    return true;
}

bool PhysicsManagerBox3D::onClose()
{
    b3DestroyWorld(m_worldId);

    return true;
}

void PhysicsManagerBox3D::destroy(uint32_t id)
{
    auto it = m_shapeIDs.find(id);
    if(it == m_shapeIDs.end()) {
        m_logger->error("failed to destroy body id: {}", id);
        return;
    }

    b3ShapeId shapeId = it->second;
    b3BodyId bodyId = b3Shape_GetBody(shapeId);
    b3DestroyBody(bodyId);

    m_shapeIDs.erase(id);
}

std::vector<uint32_t> PhysicsManagerBox3D::listIDs() const
{
    return std::vector<uint32_t>();
}

void PhysicsManagerBox3D::onUpdate()
{
    SceneManager& sceneManager = SceneManager::getInstance();
    float deltaTime = AppWindow::getDeltaTime();
    
    if(deltaTime <= 0) {
        for(uint32_t sceneID : sceneManager.listIDs()) {
            Scene* scene = sceneManager.getScene(sceneID);

            auto func = std::function<void(Entity)>([&](Entity entity) -> void {
                auto& collider = entity.getComponent<ColliderComponent>();
                auto& transform = entity.getComponent<TransformComponent>();

                if(collider.shapeID == 0) {
                    return;
                }

                b3ShapeId shapeId = m_shapeIDs.at(collider.shapeID);
                b3BodyId bodyId = b3Shape_GetBody(shapeId);
        
                b3Body_SetType(bodyId, b3_kinematicBody);

                b3Vec3 newPosition = { transform.translateVec.x, transform.translateVec.y, transform.translateVec.z };
                
                glm::vec3 eulerAngles(transform.rotateVec.x, transform.rotateVec.y, transform.rotateVec.z);
                glm::quat rot(eulerAngles);

                b3Quat newRotation;
                newRotation.v = { rot.x, rot.y, rot.z };
                newRotation.s = rot.w;

                b3Body_SetTransform(bodyId, newPosition, newRotation);
            });

            scene->forEnitiesWith<ColliderComponent>(func);
        }
        return;
    }

	float timeStep = 1.0f / 60.0f;
    int subStepCount = 4;
    b3World_Step(m_worldId, timeStep, subStepCount);

    // for(uint32_t sceneID : sceneManager.listIDs()) {
        // Scene* scene = sceneManager.getScene(sceneID);
        Scene* scene = sceneManager.getActiveScene();

        auto func = std::function<void(Entity)>([&](Entity entity) -> void {
            auto& collider = entity.getComponent<ColliderComponent>();
            auto& transform = entity.getComponent<TransformComponent>();

            if(collider.shapeID == 0) {
                return;
            }

            b3ShapeId shapeId = m_shapeIDs.at(collider.shapeID);
            b3BodyId bodyId = b3Shape_GetBody(shapeId);
            b3BodyType bodyType = b3Body_GetType(bodyId);
            ColliderType colliderType = static_cast<ColliderType>(collider.colliderType);

            if(colliderType == ColliderType::Static && bodyType != b3_staticBody){
                b3Body_SetType(bodyId, b3_staticBody);
            } else if(colliderType == ColliderType::Dynamic && bodyType != b3_dynamicBody) {
                b3Body_SetType(bodyId, b3_dynamicBody);
                b3Body_ApplyMassFromShapes(bodyId);
            } else if (colliderType == ColliderType::Kinematic && bodyType != b3_kinematicBody) {
                b3Body_SetType(bodyId, b3_kinematicBody);
                b3Body_SetLinearVelocity(bodyId, {0.0f, 0.0f, 0.0f}); // Clear momentum
            }

            b3Vec3 physicsPos = b3Body_GetPosition(bodyId);
            transform.translate({physicsPos.x, physicsPos.y, physicsPos.z});

            b3Quat physicsRot = b3Body_GetRotation(bodyId);
            glm::quat glmRotation(physicsRot.s, physicsRot.v.x, physicsRot.v.y, physicsRot.v.z);
            glm::vec3 eulerAngles = glm::eulerAngles(glmRotation);
            transform.rotate({eulerAngles.x, eulerAngles.y, eulerAngles.z});
        });

        scene->forEnitiesWith<ColliderComponent>(func);
    // }

    // uint32_t entityID = rayCastClosest(glm::vec3(1.0));
    // if(entityID != 0) {
    //     Entity entity = scene->getEntity(entityID);
    //     m_logger->warn(entity.getComponent<NameComponent>().name);
    // }
}

uint32_t PhysicsManagerBox3D::createMeshBody(
    uint32_t entityID,
    const Mesh& mesh,
    const glm::vec3& pos,
    const glm::vec3& scale,
    uint32_t colType
) {
    ColliderType colliderType = static_cast<ColliderType>(colType);
    if (colliderType == ColliderType::Dynamic || colliderType == ColliderType::Kinematic) {    //TODO: eventually support dynamic shape collider for player
        m_logger->warn("dynamic mesh collider not supported yet");
        return 0;
    }

    std::vector<b3Vec3> vertices;
    for(const auto& vertex : mesh.vertices) {
        glm::vec3 v = vertex.positions;
        vertices.emplace_back(b3Vec3(v.x, v.y, v.z));
    }

    std::vector<int32_t> indices(mesh.indices.begin(), mesh.indices.end());
    if(mesh.indices.empty()) {
        indices.resize(mesh.vertices.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            indices[i] = static_cast<int32_t>(i);
        }
    }

    b3MeshDef meshDef = { 0 };
    meshDef.vertices      = vertices.data();
    meshDef.vertexCount   = mesh.vertices.size();
    meshDef.indices       = indices.data();
    meshDef.triangleCount = mesh.indices.size() / 3;
    meshDef.weldVertices  = true;
    meshDef.identifyEdges = true;

    b3MeshData* meshData = b3CreateMesh(&meshDef, NULL, 0);
    if (meshData == nullptr) {
        m_logger->critical("failed to create mesh data");
    }
    
    b3Mesh meshShape;
    meshShape.data = meshData;
    meshShape.scale = { scale.x, scale.y, scale.z };

    b3ShapeDef shapeDef = b3DefaultShapeDef();
    shapeDef.userData = (void*)(uintptr_t)entityID;

    bool isStatic = colliderType == ColliderType::Static;
    if(!isStatic) {
        shapeDef.density = 1.0f;
        shapeDef.baseMaterial.friction = 0.3f;
    }
    
    b3BodyDef bodyDef = b3DefaultBodyDef();
    bodyDef.type = isStatic ? b3_staticBody : b3_dynamicBody;
    bodyDef.position = { pos.x, pos.y, pos.z };
    
    b3BodyId bodyId = b3CreateBody(m_worldId, &bodyDef);
    b3ShapeId shapeID = b3CreateMeshShape(bodyId, &shapeDef, meshData, meshShape.scale);

    if (!isStatic) {
        b3Body_ApplyMassFromShapes(bodyId);
    }

    m_shapeIDs[m_ids] = shapeID;

    return _assignID();
}

uint32_t PhysicsManagerBox3D::createBoxBody(
    uint32_t entityID,
    const Mesh& mesh,
    const glm::vec3& pos,
    const glm::vec3& scale,
    uint32_t colType
) {
    ColliderType colliderType = static_cast<ColliderType>(colType);
    const float min = std::numeric_limits<float>::max();
    const float max = -std::numeric_limits<float>::max();
    b3Vec3 minBounds = {  min,  min,  min };
    b3Vec3 maxBounds = { max, max, max };

    for(const auto& vertex : mesh.vertices) {
        minBounds.x = std::min(minBounds.x, vertex.positions.x);
        minBounds.y = std::min(minBounds.y, vertex.positions.y);
        minBounds.z = std::min(minBounds.z, vertex.positions.z);
        
        maxBounds.x = std::max(maxBounds.x, vertex.positions.x);
        maxBounds.y = std::max(maxBounds.y, vertex.positions.y);
        maxBounds.z = std::max(maxBounds.z, vertex.positions.z);
    }

    b3Vec3 center{};
    center.x = (minBounds.x + maxBounds.x) * 0.5;
    center.y = (minBounds.y + maxBounds.y) * 0.5;
    center.z = (minBounds.z + maxBounds.z) * 0.5;

    //Half-extents: distance from center to edges
    float hx = (maxBounds.x - minBounds.x) * 0.5 * scale.x;
    float hy = (maxBounds.y - minBounds.y) * 0.5 * scale.y;
    float hz = (maxBounds.z - minBounds.z) * 0.5 * scale.z;

    b3ShapeDef shapeDef = b3DefaultShapeDef();
    shapeDef.userData = (void*)(uintptr_t)entityID;

    bool isStatic = colliderType == ColliderType::Static;
    if(!isStatic) {
        shapeDef.density = 1.0f;
        shapeDef.baseMaterial.friction = 0.3f;
    }
    
    b3BodyDef bodyDef = b3DefaultBodyDef();
    bodyDef.type = isStatic ? b3_staticBody : b3_dynamicBody;
    bodyDef.position = { pos.x, pos.y, pos.z };
    
    b3BodyId bodyId = b3CreateBody(m_worldId, &bodyDef);
    b3BoxHull boxHull = b3MakeBoxHull(hx, hy, hz);
    b3ShapeId shapeID = b3CreateHullShape(bodyId, &shapeDef, &boxHull.base);

    if (!isStatic) {
        b3Body_ApplyMassFromShapes(bodyId);
    }

    m_shapeIDs[m_ids] = shapeID;


    return _assignID();
}

uint32_t PhysicsManagerBox3D::createSphereBody(
    uint32_t entityID,
    const Mesh& mesh,
    const glm::vec3& pos,
    const glm::vec3& scale,
    float radius,
    uint32_t colType
) {
    ColliderType colliderType = static_cast<ColliderType>(colType);
    const float min = std::numeric_limits<float>::max();
    const float max = -std::numeric_limits<float>::max();
    b3Vec3 minBounds = {  min,  min,  min };
    b3Vec3 maxBounds = { max, max, max };

    for(const auto& vertex : mesh.vertices) {
        minBounds.x = std::min(minBounds.x, vertex.positions.x);
        minBounds.y = std::min(minBounds.y, vertex.positions.y);
        minBounds.z = std::min(minBounds.z, vertex.positions.z);
        
        maxBounds.x = std::max(maxBounds.x, vertex.positions.x);
        maxBounds.y = std::max(maxBounds.y, vertex.positions.y);
        maxBounds.z = std::max(maxBounds.z, vertex.positions.z);
    }

    b3Vec3 center{};
    center.x = (minBounds.x + maxBounds.x) * 0.5;
    center.y = (minBounds.y + maxBounds.y) * 0.5;
    center.z = (minBounds.z + maxBounds.z) * 0.5;

    //Half-extents: distance from center to edges
    float hx = (maxBounds.x - minBounds.x) * 0.5 * scale.x;
    float hy = (maxBounds.y - minBounds.y) * 0.5 * scale.y;
    float hz = (maxBounds.z - minBounds.z) * 0.5 * scale.z;

    b3Transform localTransform = {
        center, 
        b3Quat_identity
    };

    b3ShapeDef shapeDef = b3DefaultShapeDef();
    shapeDef.userData = (void*)(uintptr_t)entityID;

    bool isStatic = colliderType == ColliderType::Static;
    if(!isStatic) {
        shapeDef.density = 1.0f;
        shapeDef.baseMaterial.friction = 0.3f;
    }
    
    b3BodyDef bodyDef = b3DefaultBodyDef();
    bodyDef.type = isStatic ? b3_staticBody : b3_dynamicBody;
    bodyDef.position = { pos.x, pos.y, pos.z };
    
    b3BodyId bodyId = b3CreateBody(m_worldId, &bodyDef);
    
    b3Sphere sphere;
    sphere.center = center;
    sphere.radius = radius;
    b3ShapeId shapeID = b3CreateSphereShape(bodyId, &shapeDef, &sphere);

    if (!isStatic) {
        b3Body_ApplyMassFromShapes(bodyId);
    }

    m_shapeIDs[m_ids] = shapeID;


    return _assignID();
}

uint32_t PhysicsManagerBox3D::createCapsuleBody(
    uint32_t entityID,
    const Mesh& mesh,
    const glm::vec3& pos,
    const glm::vec3& scale,
    float radius,
    const glm::vec3& center1,
    const glm::vec3& center2,
    uint32_t colliderType
) {
    m_logger->warn("createCapsuleBody not supported yet");

    return _assignID();
}


uint32_t PhysicsManagerBox3D::createConvexBody()
{
    m_logger->warn("createConvexBody not supported yet");

    return _assignID();
}

void PhysicsManagerBox3D::addForce(uint32_t id, const glm::vec3 &force)
{
    b3BodyId bodyId = _getBody(id);

    b3Pos position = b3Body_GetPosition(bodyId);
    b3Vec3 pushForce = {force.x, force.y, force.z};
    b3Body_ApplyForceToCenter(bodyId, pushForce, true);

    // b3Pos newPos = {position.x + force.x, position.y + force.y, position.z + force.z}
    // b3Body_SetLinearVelocity(bodyId, newPos); // Clear momentum
}

uint32_t PhysicsManagerBox3D::rayCastClosest(const glm::vec3 &location)
{
    SceneManager& sceneManager = SceneManager::getInstance();
    Camera* camera = SceneManager::cameraController;
    if(!camera) {
        return 0;
    }

    double x, y;
    AppWindow::getCursorPos(&x, &y);

    float xNDC = (x / AppWindow::getWidth()) * 2.0f - 1.0f;
    float yNDC = (y / AppWindow::getHeight()) * 2.0f - 1.0f;
    // float yNDC = 1.0f - (static_cast<float>(y) / AppWindow::getHeight()) * 2.0f;

    glm::vec4 rayClipNear(xNDC, yNDC, 0.0f, 1.0f);
    glm::vec4 rayClipFar(xNDC, yNDC, 1.0f, 1.0f);

    glm::mat4 view = camera->getViewMatrix();
	glm::mat4 proj = camera->getProjectionMatrix();
    
    glm::mat4 inViewProj = glm::inverse(proj * view);
    glm::vec4 worldNear = inViewProj * rayClipNear;
    glm::vec4 worldFar = inViewProj * rayClipFar;
    
    glm::vec3 rayOrigin = worldNear / worldNear.w;
    glm::vec3 rayEnd = worldFar / worldFar.w;
    glm::vec3 rayDir = glm::normalize(rayEnd - rayOrigin);
    
    float maxDistance = 100.0f; 
    b3Vec3 translation = { rayDir.x * maxDistance, rayDir.y * maxDistance, rayDir.z * maxDistance };
    b3Pos origin = { rayOrigin.x, rayOrigin.y, rayOrigin.z };
    b3QueryFilter filter = b3DefaultQueryFilter();
    b3RayResult result = b3World_CastRayClosest(m_worldId, origin, translation, filter);

    if(result.hit) {
        void* userData = b3Shape_GetUserData(result.shapeId);
        return (uint32_t)(uintptr_t)userData;   // cast like this to prevent 4 byte uint32_t with 8 bytes pointer
    }

    return 0;
}

void PhysicsManagerBox3D::setBullet(uint32_t id, bool flag)
{
    b3Body_SetBullet(_getBody(id), flag);
}

bool PhysicsManagerBox3D::isBullet(uint32_t id)
{
    return b3Body_IsBullet(_getBody(id));
}

void PhysicsManagerBox3D::enableSleep(uint32_t id, bool flag)
{
    b3Body_EnableSleep(_getBody(id), flag);
}

bool PhysicsManagerBox3D::isSleepEnabled(uint32_t id)
{
    return b3Body_IsSleepEnabled(_getBody(id));
}

void PhysicsManagerBox3D::setAwake(uint32_t id, bool flag)
{
    b3Body_SetAwake(_getBody(id), flag);
}

bool PhysicsManagerBox3D::isAwake(uint32_t id)
{
    return (b3Body_IsAwake(_getBody(id)));
}

void PhysicsManagerBox3D::setMass(uint32_t id, MassData data)
{
    b3BodyId bodyId = _getBody(id);
    
    b3MassData massData{};
    massData.mass = data.mass;
    massData.center = toB3Vec3(data.center);
    massData.inertia = toB3Mat3(data.inertia);

    b3Body_SetMassData(bodyId, massData);
    // b3Body_ApplyMassFromShapes(bodyId);
}

MassData PhysicsManagerBox3D::getMass(uint32_t id)
{
    b3BodyId bodyId = _getBody(id);

    float mass = b3Body_GetMass(bodyId);
    b3Matrix3 inertia = b3Body_GetLocalRotationalInertia(bodyId);
    b3Vec3 localCenter = b3Body_GetLocalCenter(bodyId);
    b3MassData massData = b3Body_GetMassData(bodyId);

    return { mass, b3ToGlmVec3(localCenter), b3ToGlmMat3(inertia) };
}

void PhysicsManagerBox3D::disable(uint32_t id)
{
    b3Body_Disable(_getBody(id));
}

void PhysicsManagerBox3D::enable(uint32_t id)
{
    b3Body_Enable(_getBody(id));
}

bool PhysicsManagerBox3D::isEnabled(uint32_t id)
{
    return b3Body_IsEnabled(_getBody(id));
}

void PhysicsManagerBox3D::addImpulse(uint32_t id, const glm::vec3 &impulse)
{
    b3BodyId bodyId = _getBody(id);
    b3Vec3 pushImpulse = {impulse.x, impulse.y, impulse.z};
    
    // Use ApplyLinearImpulseToCenter for an instant push
    b3Body_ApplyLinearImpulseToCenter(bodyId, pushImpulse, true);
}

// void PhysicsManagerBox3D::setMotionLocks(uint32_t id, b3MotionLocks locks)
// {
//     b3Body_SetMotionLocks(_getBody(id), locks);
// }

// void PhysicsManagerBox3D::getMotionLocks(uint32_t id)
// {
//     b3Body_GetMotionLocks   
// }

b3BodyId PhysicsManagerBox3D::_getBody(uint32_t id)
{
    auto it = m_shapeIDs.find(id);
    if(it == m_shapeIDs.end()) {
        m_logger->error("failed to setMass on body id: {}", id);
        throw std::runtime_error("could not find body with given id");
    }
    
    b3ShapeId shapeId = it->second;
    b3BodyId bodyId = b3Shape_GetBody(shapeId);

    return bodyId;
}
