#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "core/features/Model.h"
#include "core/features/Mesh.h"

class Logger;
class TextureManager;
class MeshManager;
class SkeletonManager;
class MaterialManager;
class AnimationManager;

class ModelLoader
{
public:
	struct Context {
		Logger* logger{ nullptr };
		TextureManager* textureManager{ nullptr };
		MeshManager* meshManager{ nullptr };
		SkeletonManager* skeletonManager{ nullptr };
		MaterialManager* materialManager{ nullptr };
		AnimationManager* animationManager{ nullptr };
	};

	explicit ModelLoader(const Context& context);
	virtual ~ModelLoader() = default;

	virtual bool load(std::string_view path, Model& model) = 0;

protected:
	std::string _resolveTexturePath(std::string_view texturePath, std::string_view modelPath) const;
	static void _setDefaultBone(Vertex& vertex);
	static void _generateNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
	static void _generateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
	static bool _looksLikeNormalMap(std::string_view name);

protected:
	Context m_context;
};
