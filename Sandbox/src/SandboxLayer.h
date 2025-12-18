#pragma once

#include <core/layers/Layer.h>

class SandBoxLayer : public Layer
{
public:
 	SandBoxLayer(const std::string& name);
	~SandBoxLayer() = default;
	
	virtual bool init() override;
	void onAttach(LayerManager* manager) override;
	void onDetach() override;
	void onUpdate() override;
	void onGuiUpdate() override;
	void onEvent(Event& event) override;

private:

};