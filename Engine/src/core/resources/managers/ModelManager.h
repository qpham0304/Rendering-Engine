#pragma once

#include "Manager.h"
#include <assimp/scene.h>
#include "core/features/Model.h"

class Logger;
class TextureManager;
class MeshManager;

class ModelManager : public Manager
{
public:
	ModelManager();
	~ModelManager();

	virtual int init() override;
	virtual int onClose() override;
	virtual void destroy(uint32_t id) override;
	uint32_t loadModel(std::string_view path);
	Model* getModel(uint32_t id);

private:
	void _loadModel(std::string_view path);
    void _processNode(aiNode* node, const aiScene* scene, std::vector<uint32_t>& meshes, std::string_view directory);
    uint32_t _processMesh(aiMesh* mesh, const aiScene* scene, std::string_view directory);
    std::vector<uint8_t> _loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, std::string_view directory);
	//void _extractBoneWeightForVertices(std::vector<MeshOpenGL::Vertex>& vertices, aiMesh* mesh, const aiScene* scene);
	//void _setVertexBoneDataToDefault(MeshOpenGL::Vertex& vertex);
	
private:
	std::unordered_map<uint32_t, std::shared_ptr<Model>> m_models;
	std::unordered_map<std::string, uint32_t> m_modelData;
	TextureManager* textureManager;
	MeshManager* meshManager;

};

