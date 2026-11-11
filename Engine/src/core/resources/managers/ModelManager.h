#pragma once

#include "Manager.h"
#include "core/features/Model.h"
#include "core/features/MathIncludes.h"
#include "core/features/Mesh.h"
#include "core/features/Material.h"
#include <memory>
#include <unordered_set>

class Logger;
class ModelLoader;
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

private:
	std::unique_ptr<ModelLoader> m_loaderGLTF;
	std::unique_ptr<ModelLoader> m_loaderFBX;
	std::unique_ptr<ModelLoader> m_loaderAssimp;
	std::unordered_map<uint32_t, std::shared_ptr<Model>> m_models{};
	std::unordered_map<std::string, uint32_t> m_modelData{};
	std::unordered_set<std::string> m_failedPaths{};
	TextureManager* textureManager{ nullptr };
	MeshManager* meshManager{ nullptr };
	SkeletonManager* skeletonManager{ nullptr };
	MaterialManager* materialManager{ nullptr };
	AnimationManager* animationManager{ nullptr };
};

