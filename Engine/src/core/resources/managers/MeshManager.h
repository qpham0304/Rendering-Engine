#pragma once

#include "Manager.h"

class Mesh;
class BufferManager;

class MeshManager : public Manager
{
public:
    struct MeshData {                   // handle to GPU buffers
        uint32_t vertexBufferID = 0;        // TODO: graphics pipeline is still using attribute
        uint32_t indexBufferID = 0;         // move to device buffer address completely
        uint32_t matIndicesBDA_ID = 0; // these are device buffer address
        uint32_t vertexBDA_ID = 0;
        uint32_t indexBDA_ID = 0;
        uint32_t blasBufferID = 0;
    };

    MeshManager();
	virtual ~MeshManager();

	virtual bool init(WindowConfig config) override;
    virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
    virtual uint32_t loadMesh(Mesh& mesh);
    Mesh* getMesh(uint32_t id) const;
    MeshData getMeshData(uint32_t id) const;
    void bindMesh(uint32_t id);

private:
    std::unordered_map<uint32_t, std::shared_ptr<Mesh>> m_meshes;     // CPU-side mesh data
    std::unordered_map<uint32_t, MeshData> m_meshesData;              //mesh's handle to to GPU buffer data;
    
    // not supported now but plan to bundle everything together for draw indirect
    // std::vector<Vertex> vertices;
	// std::vector<uint32_t> indices;
    // std::vector<uint32_t> materialIndices;


    BufferManager* m_bufferManager{ nullptr };
};

