#pragma once

#include "ModelLoader.h"
#include "core/features/Material.h"
#include <vector>

struct ufbx_mesh;
struct ufbx_material;
struct ufbx_texture;

class ModelLoaderFBX : public ModelLoader
{
public:
	explicit ModelLoaderFBX(const Context& context);

	bool load(std::string_view path, Model& model) override;

private:
	struct LoadState;

	void _buildSkeleton(LoadState& state);
	std::vector<uint32_t> _loadMesh(LoadState& state, const ufbx_mesh* mesh);
	uint32_t _loadMaterial(LoadState& state, const ufbx_material* material);
	MaterialDesc _processMaterial(LoadState& state, const ufbx_material* material);
	uint32_t _loadTexture(LoadState& state, const ufbx_texture* texture, bool isDataTexture);
	void _loadAnimations(LoadState& state);
};
