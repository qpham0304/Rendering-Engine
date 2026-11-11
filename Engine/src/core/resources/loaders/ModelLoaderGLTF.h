#pragma once

#include "ModelLoader.h"
#include "core/features/Material.h"
#include <cstddef>
#include <optional>
#include <vector>

namespace fastgltf {
	class Asset;
	struct Material;
	struct Primitive;
	struct TextureInfo;
}

class ModelLoaderGLTF : public ModelLoader
{
public:
	explicit ModelLoaderGLTF(const Context& context);

	bool load(std::string_view path, Model& model) override;

private:
	struct LoadState;

	void _buildSkeleton(LoadState& state, size_t nodeIndex, int parentIndex);
	std::vector<uint32_t> _loadMesh(LoadState& state, size_t meshIndex, std::optional<size_t> skinIndex);
	uint32_t _loadPrimitive(LoadState& state, const fastgltf::Primitive& primitive, std::optional<size_t> skinIndex);
	uint32_t _loadMaterial(LoadState& state, const fastgltf::Primitive& primitive);
	MaterialDesc _processMaterial(LoadState& state, const fastgltf::Material& material);
	uint32_t _loadTexture(LoadState& state, const fastgltf::TextureInfo& info, bool isDataTexture);
	void _loadAnimations(LoadState& state);
};
