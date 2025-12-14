#include "Layer.h"
#include "core/layers/LayerManager.h"
#include "core/features/ServiceLocator.h"
#include "logging/Logger.h"

Layer::Layer(const std::string& name = "Undefined")
	:	m_LayerName(name),
		m_Enabled(true),
		m_Manager(nullptr)
{

}


void Layer::onAttach(LayerManager* manager)
{
	this->m_Manager = manager;
	setLogScopeEngine();
}


void Layer::setLogScopeEngine()
{
	m_Logger = &m_Manager->serviceLocator.Get<Logger>("Engine_LoggerPSD");
}


void Layer::setLogScopeClient()
{
	m_Logger = &m_Manager->serviceLocator.Get<Logger>("Client_LoggerPSD");
}

const std::string& Layer::getName() const
{
	return m_LayerName;
}