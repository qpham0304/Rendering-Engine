#pragma once

#include "Manager.h"

class Mesh;
class BufferManager;

class MeshManager : public Manager
{
public:

public:
    MeshManager();
	virtual ~MeshManager();

	virtual int init(WindowConfig config) override;
    virtual int onClose() override;
	virtual void destroy(uint32_t id) override;
    virtual uint32_t loadMesh(const Mesh& mesh);
    const Mesh* getMesh(uint32_t id);
    void bindMesh(uint32_t id);

private:
    struct MeshData {           // handle to GPU buffers
        uint32_t vertexBufferID;
        uint16_t indexBufferID;
    };

    std::unordered_map<uint32_t, std::shared_ptr<Mesh>> m_meshes;     // CPU-side mesh data
    std::unordered_map<uint32_t, MeshData> m_meshesData;              //mesh's handle to to GPU buffer data;

    BufferManager* m_bufferManager{ nullptr };
};

