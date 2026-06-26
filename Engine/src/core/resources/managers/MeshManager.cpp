#include "MeshManager.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Mesh.h"
#include "BufferManager.h"

MeshManager::MeshManager()
    :   Manager("MeshManager")
{

}

MeshManager::~MeshManager()
{

}
bool MeshManager::init(WindowConfig config)
{
    Service::init(config);

    m_bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    if (!(m_logger && m_bufferManager)) {
        return - 1;
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
        m_meshes[m_ids]->vertices.data(), m_meshes[m_ids]->vertices.size()
    );
    
    meshData.indexBufferID = m_bufferManager->createIndexBuffer(
        m_meshes[m_ids]->indices.data(), m_meshes[m_ids]->indices.size()
    );

    size_t triangleCount = m_meshes[m_ids]->indices.size() / 3;
    size_t matCount = m_meshes[m_ids]->materialIndices.size();
    size_t bufferSize = std::max(matCount, triangleCount) * sizeof(uint32_t);
    if (bufferSize == 0) {
        bufferSize = sizeof(uint32_t);
    }
    
    meshData.matIndicesBDA_ID = m_bufferManager->createBufferDeviceAddress(bufferSize);
    
    if (matCount >= triangleCount) {
        // model loaded Mesh has correct indices setup
        m_bufferManager->updateBufferDeviceAddress(
            meshData.matIndicesBDA_ID, 
            m_meshes[m_ids]->materialIndices.data(), 
            matCount * sizeof(uint32_t)
        );
    } else {
        // manually loaded Mesh might need to be processed by filling with the mesh's designated material ID for all triangles
        // ensures every triangle points to mesh.materialID... only took 2 days to fix this #despair
        std::vector<uint32_t> dummyIndices(triangleCount, m_meshes[m_ids]->materialID); 
        m_bufferManager->updateBufferDeviceAddress(
            meshData.matIndicesBDA_ID, 
            dummyIndices.data(), 
            dummyIndices.size() * sizeof(uint32_t)
        );
    }

    bufferSize = m_meshes[m_ids]->vertices.size() * sizeof(Vertex);
    meshData.vertexBDA_ID = m_bufferManager->createBufferDeviceAddress(bufferSize);
    m_bufferManager->updateBufferDeviceAddress(
        meshData.vertexBDA_ID, 
        m_meshes[m_ids]->vertices.data(), 
        bufferSize
    );


    bufferSize = m_meshes[m_ids]->indices.size() * sizeof(uint32_t);
    meshData.indexBDA_ID = m_bufferManager->createBufferDeviceAddress(bufferSize);
    m_bufferManager->updateBufferDeviceAddress(
        meshData.indexBDA_ID, 
        m_meshes[m_ids]->indices.data(), 
        bufferSize
    );
    
    m_meshesData[m_ids] = std::move(meshData);

    return _assignID();
}

void MeshManager::onUpdate()
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