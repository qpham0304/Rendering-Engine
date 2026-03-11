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

	static float frame_times[90] = { 0 }; 
	static int values_offset = 0;

	float current_frame_time = 1000.0f / ImGui::GetIO().Framerate;
	frame_times[values_offset] = current_frame_time;
	values_offset = (values_offset + 1) % 90; // Rotate the index

	// 3. Calculate the average for the overlay text
	float average = 0.0f;
	for (int n = 0; n < 90; n++) {
		average += frame_times[n];
	}
	average /= (float)90;

	char overlay[32];
	sprintf(overlay, "avg %.2f ms", average);

	float min_val = frame_times[0];
	float max_val = frame_times[0];
	for (int i = 1; i < 90; i++) {
		if (frame_times[i] < min_val) min_val = frame_times[i];
		if (frame_times[i] > max_val) max_val = frame_times[i];
	}

	float display_min = min_val * 0.9f; 
	float display_max = max_val * 1.1f;

	ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Pure Green
	ImGui::PlotLines("##FrameTimes", 
		frame_times, 
		90, 
		values_offset, 
		overlay, 
		0.0f,          // Min scale (keep at 0 to see relative height)
		FLT_MAX,       // AUTO-SCALE: Max will be the highest value in the buffer
		ImVec2(0, 80.0f)
	);
	ImGui::PopStyleColor();

	ImGui::SameLine();
	ImGui::Checkbox("Detail", &showDetail);
	if (showDetail) {
		for (const auto& [label, time] : profileList) {
			ImGui::Text("%s", label.c_str());
			ImGui::SameLine();
			ImGui::Text("%s", std::to_string(time).c_str());
		}
	}
	ImGui::End();
}
