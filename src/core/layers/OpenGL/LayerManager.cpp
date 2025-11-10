#include "../LayerManager.h"
#include "ImGui.h" // TODO remove when gui is refactored
#include "../../features/ServiceLocator.h"

#define OUT_OF_BOUND_ERROR(index) { \
    std::string msg = "Index out of bound: \"" + std::to_string(index) + "\""; \
    throw std::runtime_error(msg); \
}

std::unordered_map<std::string, std::shared_ptr<FrameBuffer>> LayerManager::frameBuffers = {};

bool LayerManager::boundCheck(const int& index) const {
	return !(index < 0 || index >= m_Layers.size());
}

LayerManager::LayerManager(ServiceLocator& serviceLocator)
	:	appWindow(serviceLocator.Get<AppWindow>("AppWindow")),
		serviceLocator(serviceLocator),
		m_SelectedLayer(-1)
{
}

LayerManager::~LayerManager()
{
	for (Layer* layer : m_Layers) {
		delete layer;
	}
}

Layer& LayerManager::operator[](const int index)
{
	if (!boundCheck(index)) {
		OUT_OF_BOUND_ERROR(index);
	}
	return *m_Layers[index];
}

const Layer& LayerManager::operator[](const int index) const
{
	if (!boundCheck(index)) {
		OUT_OF_BOUND_ERROR(index);
	}
	return *m_Layers[index];
}

bool LayerManager::addFrameBuffer(const std::string& name, FrameBuffer& FBO)
{
	if (frameBuffers.find(name) == frameBuffers.end()) {
		frameBuffers[name] = std::make_shared<FrameBuffer>(FBO);
		return true;
	}
	return false;
}

std::shared_ptr<FrameBuffer> LayerManager::getFrameBuffer(const std::string name)
{
	if (frameBuffers.find(name) != frameBuffers.end()) {
		return frameBuffers[name];
	}
	return nullptr;
}

bool LayerManager::addLayer(Layer* layer)
{
	layer->onAttach(this);
	m_Layers.push_back(layer);
	return true;
}

bool LayerManager::removeLayer(const int index)
{
	if (!boundCheck(index)) {
		return false;
	}
	m_Layers[index]->onDetach();
	delete m_Layers[index];

	m_Layers.erase(m_Layers.begin() + index);
	return true;
}

void LayerManager::EnableLayer(const int index)
{
	if (!boundCheck(index)) {
		OUT_OF_BOUND_ERROR(index);
	}
	m_Layers[index]->m_Enabled = true;
}

void LayerManager::DisableLayer(const int index)
{
	if (!boundCheck(index)) {
		OUT_OF_BOUND_ERROR(index);
	}
	m_Layers[index]->m_Enabled = false;
}

const int& LayerManager::size() const
{
	return m_Layers.size();
}

const std::string& LayerManager::CurrentLayer()
{
	return m_Layers[m_SelectedLayer]->GetName();
}

void LayerManager::onUpdate() 
{
	for (const auto& layer : m_Layers) {
		if (!layer->m_Enabled) {
			continue;
		}
		layer->onUpdate();
	}
}

void LayerManager::onGuiUpdate() 
{
	for (auto& layer : m_Layers) {
		if (!layer->m_Enabled) {
			continue;
		}
		layer->onGuiUpdate();
	}
	for (auto& layer : m_Layers) {
		ImGui::Begin("Layers");
		ImGui::Checkbox(layer->GetName().c_str(), &layer->m_Enabled);
		ImGui::End();
	}
}

//std::vector<Layer*>::iterator LayerManager::begin()
//{
//	return m_Layers.begin();
//}
//
//std::vector<Layer*>::iterator LayerManager::end()
//{
//	return m_Layers.end();
//}

const std::vector<Layer*>::const_iterator LayerManager::begin() const
{
	return m_Layers.begin();
}

const std::vector<Layer*>::const_iterator LayerManager::end() const
{
	return m_Layers.end();
}

//std::vector<Layer*>::reverse_iterator LayerManager::rbegin()
//{
//	return m_Layers.rbegin();
//}
//
//std::vector<Layer*>::reverse_iterator LayerManager::rend()
//{
//	return m_Layers.rend();
//}

const std::vector<Layer*>::const_reverse_iterator LayerManager::rbegin() const
{
	return m_Layers.rbegin();
}

const std::vector<Layer*>::const_reverse_iterator LayerManager::rend() const
{
	return m_Layers.rend();
}
