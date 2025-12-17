#pragma once

#include "Manager.h"
#include <assimp/scene.h>
#include "core/features/Model.h"

class Logger;
class TextureManager;
class MeshManager;
class MaterialManager;

class ModelManager : public Manager
{
public:
	ModelManager();
	~ModelManager();

	virtual int init(WindowConfig config) override;
	virtual int onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
	uint32_t loadModel(std::string_view path);
	const Model* getModel(uint32_t id) const;

private:
	void _loadModel(std::string_view path);
    void _processNode(aiNode* node, const aiScene* scene, std::vector<uint32_t>& meshes, std::string_view directory);
    uint32_t _processMesh(aiMesh* mesh, const aiScene* scene, std::string_view directory);
    std::vector<uint32_t> _loadMaterial(aiMaterial* mat, aiTextureType type, std::string typeName, std::string_view directory);
	//void _extractBoneWeightForVertices(std::vector<MeshOpenGL::Vertex>& vertices, aiMesh* mesh, const aiScene* scene);
	//void _setVertexBoneDataToDefault(MeshOpenGL::Vertex& vertex);
	
private:
	std::unordered_map<uint32_t, std::shared_ptr<Model>> m_models;
	std::unordered_map<std::string, uint32_t> m_modelData;
	TextureManager* textureManager;
	MeshManager* meshManager;
	MaterialManager* materialManager;

};

