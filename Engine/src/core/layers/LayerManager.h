#pragma once

#include <vector>
#include <stack>
#include <memory>
#include "Layer.h"
#include "../../window/AppWindow.h"
#include "../../graphics/framework/OpenGL/renderers/FrameBuffer.h"

class ServiceLocator;
class LayerManager
{
private:
	static std::unordered_map<std::string,std::shared_ptr<FrameBuffer>> frameBuffers;
	std::vector<Layer*> m_Layers;
	int m_SelectedLayer;

	bool boundCheck(const int& index) const;

	AppWindow& appWindow;
	ServiceLocator& serviceLocator;

public:
	friend class Layer;

	LayerManager(ServiceLocator& serviceLocator);
	~LayerManager();

	Layer& operator[](const int index);
	const Layer& operator[](const int index) const;

	static bool addFrameBuffer(const std::string& name, FrameBuffer& FBO);
	static std::shared_ptr<FrameBuffer> getFrameBuffer(const std::string name);

	bool addLayer(Layer* layer);
	bool removeLayer(const int index);
	void EnableLayer(const int index);
	void DisableLayer(const int index);
	const int& size() const;
	const std::string& CurrentLayer();

	int init();
	void onUpdate();
	void onClose();
	void onGuiUpdate();

	//std::vector<Layer*>::iterator begin();
	//std::vector<Layer*>::iterator end();
	//std::vector<Layer*>::reverse_iterator rbegin();
	//std::vector<Layer*>::reverse_iterator rend();

	const std::vector<Layer*>::const_iterator begin() const;
	const std::vector<Layer*>::const_iterator end() const;
	const std::vector<Layer*>::const_reverse_iterator rbegin() const;
	const std::vector<Layer*>::const_reverse_iterator rend() const;
};