#include "Profiler.h"
#include "../../graphics/utils/Utils.h"
#include "imgui.h"

Profiler& Profiler::_getInstance()
{
	static Profiler instance;
	return instance;
}

void Profiler::addTracker(ProfilerData&& data)
{
	_getInstance()._addTracker(std::move(data));
}

void Profiler::addTracker(const ProfilerData& data)
{
	_getInstance()._addTracker(data);
}

void Profiler::display()
{
	_getInstance()._display();
}

void Profiler::_addTracker(ProfilerData&& data)
{
	profileList[data.name] = data.time;
}

void Profiler::_addTracker(const ProfilerData& data)
{
	profileList[data.name] = data.time;
}

void Profiler::_display()
{
	ImGui::Begin("Profiler");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 
		1000.0f / ImGui::GetIO().Framerate, 
		ImGui::GetIO().Framerate
	);
	for (const auto& [label, time] : profileList) {
		ImGui::Text(label.c_str());
		ImGui::SameLine();
		ImGui::Text(std::to_string(time).c_str());
	}
	ImGui::End();
}
