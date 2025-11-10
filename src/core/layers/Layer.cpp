#include "Layer.h"
#include "../../src/core/layers/LayerManager.h"
#include "../../src/core/features/ServiceLocator.h"

Layer::Layer(const std::string& name)
	:	m_LayerName(name),
		m_Enabled(true),
		m_Manager(nullptr)
{

}

void Layer::onAttach(LayerManager* manager)
{
	this->m_Manager = manager;
	m_Logger = &manager->serviceLocator.Get<Logger>("Engine_LoggerPSD");
}