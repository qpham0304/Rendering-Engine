#pragma once

#include "Manager.h"
#include <assimp/scene.h>
#include "core/features/Model.h"
#include "core/features/MathIncludes.h"
#include "core/features/Mesh.h"
#include <unordered_set>

class Logger;
class TextureManager;
class MeshManager;
class SkeletonManager;
class MaterialManager;
class AnimationManager;

class MYGRAPHICSENGINE_EXPORT ModelManager : public Manager
{
public:
	ModelManager();
	virtual ~ModelManager();

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
	uint32_t loadModel(std::string_view path);
	uint32_t addModel(Model& model);
	Model* getModel(uint32_t id) const;
	void clearFailedLoads();

	static std::vector<std::string> listPrimitives();	// generated meshes loadModel accepts by name, i.e $prim$Cube
	std::vector<std::string> listModelPaths();			// every loaded model and primitive, sorted, no duplicates

private:
	bool _loadModel(std::string_view path);
    void _processNode(aiNode* node,
		const aiScene* scene,
		std::vector<uint32_t>& meshes,
		std::unordered_map<uint32_t, uint32_t>& meshCache,
		std::string_view directory,
		uint32_t skeletonID,
		uint32_t skeletonNodeIndex
	);
    uint32_t _processMesh(aiMesh* mesh, const aiScene* scene, std::string_view directory, uint32_t skeletonID);
    std::vector<uint32_t> _loadMaterial(aiMaterial* mat, const aiScene* scene, aiTextureType type, std::string typeName, std::string_view directory);
	void _processSkeletonNodeHierarchy(uint32_t skeletonID, aiNode* aiNode, int parentIndex);
	void _extractBoneWeightForVertices(uint32_t skeletonID, std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);
	void _setDefaultBone(Vertex& vertex);

private:
	std::unordered_map<uint32_t, std::shared_ptr<Model>> m_models{};
	std::unordered_map<std::string, uint32_t> m_modelData{};
	std::unordered_set<std::string> m_failedPaths{};
	TextureManager* textureManager{ nullptr };
	MeshManager* meshManager{ nullptr };
	SkeletonManager* skeletonManager{ nullptr };
	MaterialManager* materialManager{ nullptr };
	AnimationManager* animationManager{ nullptr };
};

