#pragma once

#include "ModelLoader.h"
#include "core/features/Material.h"
#include <initializer_list>
#include <unordered_map>
#include <vector>
#include <assimp/scene.h>

class ModelLoaderAssimp : public ModelLoader
{
public:
	explicit ModelLoaderAssimp(const Context& context);

	bool load(std::string_view path, Model& model) override;

private:
	void _processNode(aiNode* node,
		const aiScene* scene,
		std::vector<uint32_t>& meshes,
		std::unordered_map<uint32_t, uint32_t>& meshCache,
		std::string_view modelPath,
		uint32_t skeletonID,
		uint32_t skeletonNodeIndex
	);
	uint32_t _processMesh(aiMesh* mesh, const aiScene* scene, std::string_view modelPath, uint32_t skeletonID);
	MaterialDesc _processMaterial(aiMaterial* material, const aiScene* scene, std::string_view modelPath);
	uint32_t _loadTexture(aiMaterial* material, const aiScene* scene, std::initializer_list<aiTextureType> types, bool isDataTexture, std::string_view modelPath);
	void _loadAnimations(std::string_view path, const aiScene* scene);
	void _processSkeletonNodeHierarchy(uint32_t skeletonID, aiNode* aiNode, int parentIndex);
	void _extractBoneWeightForVertices(uint32_t skeletonID, std::vector<Vertex>& vertices, aiMesh* mesh);
};
