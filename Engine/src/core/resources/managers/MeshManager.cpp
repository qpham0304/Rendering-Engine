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
void MeshManager::init()
{
    m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
    // m_bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManager");
}

void MeshManager::shutdown()
{

}

void MeshManager::destroy(uint32_t id)
{
    m_meshes.clear();
}



uint32_t MeshManager::loadMesh(const Mesh& mesh)
{
    m_meshes[m_ids] = std::make_shared<Mesh>(mesh);
    
    MeshData meshData{};
    // meshData.vertexBufferID = m_bufferManager->createVertexBuffer(mesh.vertices);
    // meshData.indexBufferID = m_bufferManager->createIndexBuffer(mesh.indices);
    m_meshesData[m_ids] = meshData;
    
    // ideally upload mesh data to GPU and the gpu handle

    return _assignID();
}


Mesh* MeshManager::getMesh(uint32_t id)
{
    if (m_meshes.find(id) == m_meshes.end()) {
        throw std::runtime_error("Could not find mesh with the given ID");
    }
    return m_meshes[id].get();
}
