#pragma once

#include "Engine/EngineCoreAPI.h"
#include <vector>
#include <memory>
#include "Layer.h"
#include "window/AppWindow.h"
#include "core/resources/managers/Manager.h"
#include "../../graphics/framework/OpenGL/renderers/FrameBuffer.h"

class MYGRAPHICSENGINE_EXPORT LayerManager : public Manager
{
public:
	friend class Layer;

	LayerManager();
	~LayerManager();

	Layer& operator[](const int index);
	const Layer& operator[](const int index) const;

	bool addLayer(Layer* layer);
	bool removeLayer(const int& index);
	void enableLayer(const int& index);
	void disableLayer(const int& index);
	int size() const;
	const std::string& CurrentLayer();

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
	
	virtual void onUpdate();
	void onGuiUpdate();

	const std::vector<Layer*>::const_iterator begin() const;
	const std::vector<Layer*>::const_iterator end() const;
	const std::vector<Layer*>::const_reverse_iterator rbegin() const;
	const std::vector<Layer*>::const_reverse_iterator rend() const;
	
private:
	static std::unordered_map<std::string,std::shared_ptr<FrameBuffer>> frameBuffers;
	std::vector<Layer*> m_Layers;
	int m_SelectedLayer;

	bool boundCheck(const int& index) const;
};