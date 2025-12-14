#include "MeshManager.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Mesh.h"
#include "BufferManager.h"

MeshManager::MeshManager()
{

}

MeshManager::~MeshManager()
{

}
int MeshManager::init(WindowConfig config)
{
    Service::init(config);

    m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
    m_bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManager");
    if (!(m_logger && m_bufferManager)) {
        return - 1;
    }
    
    return 0;
}

int MeshManager::onClose()
{
    WriteLock lock = _lockWrite();
    m_meshes.clear();
    m_meshesData.clear();

    return 0;
}

void MeshManager::destroy(uint32_t id)
{

}



uint32_t MeshManager::loadMesh(const Mesh& mesh)
{
    m_meshes[m_ids] = std::make_shared<Mesh>(mesh);
    
    MeshData meshData{};
    meshData.vertexBufferID = m_bufferManager->createVertexBuffer(m_meshes[m_ids]->vertices.data(), m_meshes[m_ids]->vertices.size());
    meshData.indexBufferID = m_bufferManager->createIndexBuffer(m_meshes[m_ids]->indices.data(), m_meshes[m_ids]->indices.size());
    
    m_meshesData[m_ids] = meshData;

    return _assignID();
}


const Mesh* MeshManager::getMesh(uint32_t id)
{
    if (m_meshes.find(id) == m_meshes.end()) {
        return nullptr;
    }
    return m_meshes[id].get();
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