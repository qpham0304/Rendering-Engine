#include "Layer.h"
#include "../../src/core/layers/LayerManager.h"
#include "../../src/core/features/ServiceLocator.h"
#include "../../src/logging/Logger.h"

Layer::Layer(const std::string& name)
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
