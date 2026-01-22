#include "ImGuiMathWidget.h"

void ImGuiMathWidget::render()
{
    ImGui::Begin("Math Widget");
    ImGui::Text("This is a math widget placeholder.");
	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    ImVec2 canvas_sz = viewportPanelSize; 
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // Top-left corner
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
     
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(30, 30, 30, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));


    float mid_y = canvas_p0.y + (canvas_sz.x / 2.0f);
    draw_list->AddLine(
        ImVec2(canvas_p0.x, mid_y), 
        ImVec2(canvas_p1.x, mid_y), 
        IM_COL32(100, 100, 100, 255), 1.0f
    );

    // Vertical Start Line (t = 0 in your math)
    draw_list->AddLine(
        ImVec2(canvas_p0.x, canvas_p0.y), 
        ImVec2(canvas_p0.x, canvas_p1.y), 
        IM_COL32(100, 100, 100, 255), 1.0f
    );

    auto ToScreen = [&](float t, float y) {
        // t: 0.0 to 1.0 maps to canvas width
        float px = canvas_p0.x + (t * canvas_sz.x);
        
        // y: -3.0 to 3.0 maps to canvas height
        // We invert y because in screen space, 0 is at the top.
        // (y + 3) / 6 converts -3...3 range to 0...1 range.
        float normalized_y = (y + 3.0f) / 6.0f; 
        float py = canvas_p1.y - (normalized_y * canvas_sz.y);
        
        return ImVec2(px, py);
    };

    // Example: Draw a line from (t=0, y=-3) to (t=1, y=3)
    draw_list->AddLine(ToScreen(0.0f, -3.0f), ToScreen(1.0f, 3.0f), IM_COL32(255, 255, 0, 255), 2.0f);
   
    for (int i = 0; i <= degree; i++) {
        // 1. Calculate the static t position for this dot
        float t_val = (float)i / (float)degree;
        
        // 2. Map current math (t, y) to screen pixels
        ImVec2 pos = ToScreen(t_val, coeffs[i]);
        
        // 3. Create an interaction area (Invisible Button)
        ImGui::PushID(i); // Unique ID for each button
        ImGui::SetCursorScreenPos(ImVec2(pos.x - 10, pos.y - 10)); // Center the 20x20 hitbox
        ImGui::InvisibleButton("dot_hitbox", ImVec2(20, 20));

        // 4. Handle Dragging
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float mouseY = ImGui::GetIO().MousePos.y;
            
            // Convert screen pixel Y back to math Y (-3 to 3)
            // Reverse of the ToScreen logic:
            float normalizedY = (canvas_p1.y - mouseY) / canvas_sz.y;
            float mathY = (normalizedY * 6.0f) - 3.0f;
            
            // Update the coefficient (clamped to your range)
            coeffs[i] = std::clamp(mathY, -3.0f, 3.0f);
        }
        ImGui::PopID();

        // 5. Visual Feedback: Draw the dot and a vertical guide line
        draw_list->AddLine(ToScreen(t_val, -3.0f), ToScreen(t_val, 3.0f), IM_COL32(50, 50, 50, 255), 1.0f);
        draw_list->AddCircleFilled(pos, 6.0f, IM_COL32(255, 255, 0, 255));
        
        // Optional: Label the coefficient
        char buf[16]; sprintf(buf, "a%d", i);
        draw_list->AddText(ImVec2(pos.x + 8, pos.y - 8), IM_COL32_WHITE, buf);
    }

    ImGui::End();
}