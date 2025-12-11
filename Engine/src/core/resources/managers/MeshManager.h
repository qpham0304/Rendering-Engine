#pragma once

#include "Manager.h"

class Mesh;
class BufferManager;

class MeshManager : public Manager
{
public:
    struct MeshData {
        uint32_t vertexBufferID;
        uint32_t indexBufferID;
    };

public:
    MeshManager();
	virtual ~MeshManager();

	virtual void init() override;
	virtual void shutdown() override;
	virtual void destroy(uint32_t id) override;
    virtual uint32_t loadMesh(const Mesh& mesh);
    Mesh* getMesh(uint32_t id);

private:
    std::unordered_map<uint32_t, std::shared_ptr<Mesh>> m_meshes;     // CPU-side mesh data
    std::unordered_map<uint32_t, MeshData> m_meshesData;              //mesh's handle to to GPU buffer data;

    BufferManager* m_bufferManager;
};

