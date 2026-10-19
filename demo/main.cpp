#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "vx.h"
#include <GLFW/glfw3.h>

int main()
{
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(1280, 800, "velox", nullptr, nullptr);
    if (!win) return 1;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    vx::Init();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    int tab = 0, sub = 0, preset = 0, target = 0, fov = 90, hitchance = 70;
    bool autoscope = true, limbs = false, baim = false, aimbot = true, silent = false, autofire = true, autostop = true, visibleOnly = false;
    bool glow = true, boxes = true, flagsOn[4] = { true, true, false, true }, antiaim = false;
    float smooth = 35.f, range = 120.f, pointscale = 90.f, col[4] = { 0.16f, 0.5f, 1.f, 1.f };
    ImGuiKey bind = ImGuiKey_V;
    bool open = true;
    const char* targets[]  = { "Closest", "Head", "Chest", "Legit" };
    const char* flagsN[]   = { "Name", "Health", "Distance", "Weapon" };
    const char* presets[]  = { "Blue", "Red", "Lime", "Violet" };

    while (!glfwWindowShouldClose(win) && open) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(60, 40), ImGuiCond_FirstUseEver);
        if (vx::BeginWindow("VELOX", ImVec2(1180, 700), &open)) {
            vx::Tab("RAGE", vx::Icon::Crosshair, 0, &tab);
            vx::Tab("LEGIT", vx::Icon::Pointer, 1, &tab);
            vx::Tab("VISUALS", vx::Icon::Eye, 2, &tab);
            vx::Tab("MISC", vx::Icon::Sliders, 3, &tab);
            vx::Tab("SKINS", vx::Icon::Gem, 4, &tab);
            vx::TabSeparator();
            vx::Tab("CFG", vx::Icon::Folder, 5, &tab);

            vx::BeginContent();
            if (tab == 0) {
                vx::BeginSubTabs(150);
                vx::SubTab("General", 0, &sub); vx::SubTab("Pistols", 1, &sub);
                vx::SubTab("Rifles", 2, &sub);  vx::SubTab("Snipers", 3, &sub);
                vx::EndSubTabs();

                float colw = (ImGui::GetContentRegionAvail().x - 24.f) / 3.f;
                if (vx::BeginGroup("Weapon", ImVec2(colw, 0))) {
                    vx::Checkbox("Enabled", &aimbot);
                    vx::Checkbox("Silent aim", &silent);
                    vx::Combo("Target", &target, targets, 4);
                    vx::SliderInt("Field of view", &fov, 1, 180, "%d deg");
                    vx::SliderInt("Hit chance", &hitchance, 0, 100, "%d%%");
                    vx::SliderFloat("Smoothing", &smooth, 0.f, 100.f, "%.0f%%");
                    vx::Keybind("Hold key", &bind);
                    vx::EndGroup();
                }
                ImGui::SameLine(0, 12);
                if (vx::BeginGroup("Extra", ImVec2(colw, 0))) {
                    vx::Checkbox("Autostop", &autostop);
                    vx::Checkbox("Autoscope", &autoscope);
                    vx::Checkbox("Ignore limbs on move", &limbs);
                    vx::SliderFloat("Pointscale", &pointscale, 0.f, 100.f, "%.0f%%");
                    vx::SliderFloat("Range", &range, 10.f, 500.f, "%.0fm");
                    vx::EndGroup();
                }
                ImGui::SameLine(0, 12);
                if (vx::BeginGroup("General", ImVec2(0, 0))) {
                    vx::Checkbox("Autofire", &autofire);
                    vx::Checkbox("Visible only", &visibleOnly);
                    vx::Checkbox("Force baim", &baim);
                    vx::Spacing(4);
                    if (vx::Button("Reset values")) vx::Notify("Values reset", 2);
                    vx::EndGroup();
                }
            } else if (tab == 2) {
                float colw = (ImGui::GetContentRegionAvail().x - 12.f) / 2.f;
                if (vx::BeginGroup("ESP", ImVec2(colw, 0))) {
                    vx::Checkbox("Boxes", &boxes);
                    vx::Checkbox("Glow", &glow);
                    vx::MultiCombo("Flags", flagsOn, flagsN, 4);
                    vx::ColorEdit("Enemy color", col);
                    vx::EndGroup();
                }
                ImGui::SameLine(0, 12);
                if (vx::BeginGroup("World", ImVec2(0, 0))) {
                    vx::Checkbox("Anti aim preview", &antiaim);
                    vx::SliderFloat("Range", &range, 10.f, 500.f, "%.0fm");
                    vx::EndGroup();
                }
            } else if (tab == 5) {
                float colw = (ImGui::GetContentRegionAvail().x - 12.f) / 2.f;
                if (vx::BeginGroup("Theme", ImVec2(colw, 0))) {
                    if (vx::Combo("Preset", &preset, presets, 4)) vx::SetPreset(preset);
                    if (vx::Button("Save config")) vx::Notify("Config saved", 1);
                    vx::Spacing(2);
                    if (vx::Button("Load config")) vx::Notify("Config loaded", 0);
                    vx::EndGroup();
                }
            }
            vx::EndContent();
        }
        vx::EndWindow();
        vx::RenderNotifications();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
