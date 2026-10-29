#include "MeshManager.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Mesh.h"
#include "BufferManager.h"

MeshManager::MeshManager()
    :   Manager("MeshManager")
{
    setFootprint("MeshManager", sizeof(MeshManager), MemorySystem::Resource);

}

MeshManager::~MeshManager()
{

}
bool MeshManager::init(WindowConfig config)
{
    Service::init(config);

    m_bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    if (!(m_logger && m_bufferManager)) {
        return false;
    }
    
    return true;
}

bool MeshManager::onClose()
{
    WriteLock lock = _lockWrite();
    m_meshes.clear();
    m_meshesData.clear();

    return true;
}

void MeshManager::destroy(uint32_t id)
{
    meshesTobeDestroyed.push_back(id);
}

std::vector<uint32_t> MeshManager::listIDs() const
{
    std::vector<uint32_t> list;
    for(const auto& [id, mesh] : m_meshes) {
        list.emplace_back(id);
    }
    return list;
}

uint32_t MeshManager::loadMesh(Mesh& mesh)
{
    //TODO: implement feature to reuse identical mesh
    m_meshes[m_ids] = std::make_shared<Mesh>(std::move(mesh));
    
    MeshData meshData{};
    meshData.vertexBufferID = m_bufferManager->createVertexBuffer(
        m_meshes[m_ids]->vertices.data(), m_meshes[m_ids]->vertices.size() * sizeof(Vertex)
    );
    
    meshData.indexBufferID = m_bufferManager->createIndexBuffer(
        m_meshes[m_ids]->indices.data(), m_meshes[m_ids]->indices.size()
    );

    size_t triangleCount = m_meshes[m_ids]->indices.size() / 3;
    size_t matCount = m_meshes[m_ids]->materialIndices.size();

    // mesh data never changes after load, so it lives in plain vram instead of the small host visible BAR heap
    if (matCount >= triangleCount && matCount > 0) {
        // model loaded Mesh has correct indices setup
        meshData.matIndicesBDA_ID = m_bufferManager->createStaticBufferDeviceAddress(
            m_meshes[m_ids]->materialIndices.data(),
            matCount * sizeof(uint32_t)
        );
    } else {
        // manually loaded Mesh might need to be processed by filling with the mesh's designated material ID for all triangles
        // ensures every triangle points to mesh.materialID... only took 2 days to fix this #despair
        std::vector<uint32_t> dummyIndices(std::max<size_t>(triangleCount, 1), m_meshes[m_ids]->materialID);
        meshData.matIndicesBDA_ID = m_bufferManager->createStaticBufferDeviceAddress(
            dummyIndices.data(),
            dummyIndices.size() * sizeof(uint32_t)
        );
    }

    meshData.vertexBDA_ID = m_bufferManager->createStaticBufferDeviceAddress(
        m_meshes[m_ids]->vertices.data(),
        m_meshes[m_ids]->vertices.size() * sizeof(Vertex)
    );

    meshData.indexBDA_ID = m_bufferManager->createStaticBufferDeviceAddress(
        m_meshes[m_ids]->indices.data(),
        m_meshes[m_ids]->indices.size() * sizeof(uint32_t)
    );

    m_meshesData[m_ids] = std::move(meshData);

    return _assignID();
}

void MeshManager::onUpdate(const FrameTime& frameTime)
{
    for(auto& id: meshesTobeDestroyed) {
        MeshData meshData = m_meshesData[id];
        m_bufferManager->destroy(meshData.vertexBufferID);
        m_bufferManager->destroy(meshData.indexBufferID);
        m_bufferManager->destroy(meshData.matIndicesBDA_ID);
        m_bufferManager->destroy(meshData.vertexBDA_ID);
        m_bufferManager->destroy(meshData.indexBDA_ID);
        m_meshes.erase(id);
        m_meshesData.erase(id);
    }
    meshesTobeDestroyed.clear();
}

Mesh* MeshManager::getMesh(uint32_t id) const
{
    if (m_meshes.find(id) == m_meshes.end()) {
        return nullptr;
    }
    return m_meshes.at(id).get();
}

MeshManager::MeshData MeshManager::getMeshData(uint32_t id) const
{
    return m_meshesData.at(id);
}

void MeshManager::bindMesh(uint32_t id)
{
    try {
        const MeshData& meshData = m_meshesData[id];
        m_bufferManager->bind(meshData.vertexBufferID);
        m_bufferManager->bind(meshData.indexBufferID);
    }
    catch (const std::exception& e) {
        m_logger->error("Failed to bind mesh ID {}: {}", id, e.what());
    }
}

Mesh MeshManager::combineMeshes(const std::vector<uint32_t>& meshIDs) const
{
    Mesh combined;
    for (uint32_t id : meshIDs) {
        Mesh* mesh = getMesh(id);
        if (!mesh) {
            continue;
        }

        uint32_t baseVertex = static_cast<uint32_t>(combined.vertices.size());
        combined.vertices.insert(combined.vertices.end(), mesh->vertices.begin(), mesh->vertices.end());

        if (mesh->indices.empty()) {
            for (uint32_t i = 0; i < mesh->vertices.size(); i++) {
                combined.indices.push_back(baseVertex + i);
            }
        } else {
            for (uint32_t index : mesh->indices) {
                combined.indices.push_back(baseVertex + index);
            }
        }
    }
    return combined;
}
