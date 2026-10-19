#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "nx.h"
#include <GLFW/glfw3.h>

int main()
{
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(1000, 680, "nx", nullptr, nullptr);
    if (!win) return 1;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    nx::ApplyStyle();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    int tab = 0, aimMode = 0, fov = 90;
    bool enabled = true, visible = false, glow = true, multi[3] = { true, false, true };
    float smooth = 35.f, range = 120.f, col[4] = { 0.06f, 0.61f, 1.f, 1.f };
    ImGuiKey bind = ImGuiKey_V;
    bool open = true;
    const char* modes[] = { "Closest", "Head", "Chest", "Legit" };
    const char* flags[] = { "Name", "Health", "Distance" };

    while (!glfwWindowShouldClose(win) && open) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(80, 60), ImGuiCond_FirstUseEver);
        if (nx::BeginWindow("NEXUS", "dev build", ImVec2(840, 560), &open)) {
            nx::SidebarLabel("COMBAT");
            nx::SidebarTab("Aimbot", 0, &tab);
            nx::SidebarTab("Visuals", 1, &tab);
            nx::SidebarLabel("MISC");
            nx::SidebarTab("Settings", 2, &tab);

            nx::BeginContent();
            if (tab == 0) {
                if (nx::BeginGroup("Aimbot", ImVec2(380, 300))) {
                    nx::Checkbox("Enabled", &enabled);
                    nx::Combo("Target", &aimMode, modes, 4);
                    nx::SliderInt("Field of view", &fov, 1, 180, "%d deg");
                    nx::SliderFloat("Smoothing", &smooth, 0.f, 100.f, "%.0f%%");
                    nx::Keybind("Hold key", &bind);
                    nx::EndGroup();
                }
                ImGui::SameLine(0, 12);
                if (nx::BeginGroup("Extra", ImVec2(0, 300))) {
                    nx::Checkbox("Visible only", &visible);
                    nx::SliderFloat("Range", &range, 10.f, 500.f, "%.0fm");
                    nx::Button("Reset");
                    nx::EndGroup();
                }
            } else if (tab == 1) {
                if (nx::BeginGroup("ESP", ImVec2(380, 300))) {
                    nx::Checkbox("Glow", &glow);
                    nx::MultiCombo("Flags", multi, flags, 3);
                    nx::ColorEdit("Accent", col);
                    nx::EndGroup();
                }
            } else {
                if (nx::BeginGroup("Theme", ImVec2(380, 200))) {
                    if (nx::ColorEdit("Accent color", col)) nx::SetAccent(ImVec4(col[0], col[1], col[2], 1.f));
                    nx::EndGroup();
                }
            }
            nx::EndContent();
        }
        nx::EndWindow();

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
