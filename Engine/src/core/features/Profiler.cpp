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

//TODO: let imgui query from profiler and display it no coupling
void Profiler::_display()
{
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Engine Profiler", nullptr, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();
        return;
    }

    static float frame_times[90] = { 0 };
    static int values_offset = 0;
    
    float dt = ImGui::GetIO().DeltaTime;
    float current_ms = dt * 1000.0f;
    frame_times[values_offset] = current_ms;
    values_offset = (values_offset + 1) % 90;

    float average = 0.0f;
    for (int n = 0; n < 90; n++) average += frame_times[n];
    average /= 90.0f;

    ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Performance Summary");
    ImGui::Separator();
    
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::SameLine(ImGui::GetWindowWidth() * 0.5f);
    ImGui::Text("Avg: %.2f ms", average);

    ImVec4 plot_color = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
    if (current_ms > 16.66f) plot_color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
    if (current_ms > 33.33f) plot_color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, plot_color);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    
    ImGui::PlotHistogram("##FrameTimes", frame_times, 90, values_offset, 
                         nullptr, 0.0f, 33.3f, ImVec2(ImGui::GetContentRegionAvail().x, 60.0f));
    
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Detailed Breakdowns", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("ProfileTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Task/System");
            ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();

            for (const auto& [label, time] : profileList) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", label.c_str());
                
                ImGui::TableSetColumnIndex(1);
                if (time > 5.0f) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%.3f ms", time);
                else ImGui::Text("%.3f ms", time);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}