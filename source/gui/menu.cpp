#include "menu.hpp"
// #include "popups.hpp"
// #include "gui_debugger.hpp"


#include <imgui.h>


void menu_t::tick(gui_debugger_t &debugger, popups_t &popups) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open..", "Ctrl+O")) {
                // file_dialog.Open();
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */ }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Begin record", "Ctrl+r")) {
                // ctrl::begin_cmd("begin_record()");
                // ctrl::end_cmd();
            }
            if (ImGui::MenuItem("End record", "Ctrl+e")) {
                // ctrl::begin_cmd("end_record()");
                // ctrl::end_cmd();
            }
            if (ImGui::MenuItem("Make axes", "Ctrl+m")) {
                // popups.open_axes_popup = 1;
            }
            if (ImGui::MenuItem("Choose record information", "Ctrl+i")) {
                // popups.open_info_selection_popup = 1;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}