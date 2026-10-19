#pragma once
#include "imgui.h"

// velox: imgui widget kit. square frame, svg icon rail, hairline gradients.
// backend agnostic. needs imgui (1.91.x) only. fonts are embedded.
namespace vx
{
    struct Theme
    {
        ImVec4 bg_top, bg_bot;          // window / sidebar base
        ImVec4 panel_top, panel_bot;    // popups
        ImVec4 frame, border;
        ImVec4 text, text_dim;
        ImVec4 acc1, acc2, acc3;        // accent line / highlights (3 stops)
    };

    struct Fonts { ImFont *body, *bold, *title, *small; };

    enum class Icon { Crosshair, Pointer, Eye, Settings, Gem, Folder, User, Shield, Sliders, Swords, Target };

    void   Init();                       // after ImGui::CreateContext(): loads fonts + style
    Fonts& GetFonts();
    Theme& GetTheme();                   // live (animated) theme
    int    PresetCount();
    const char* PresetName(int i);
    void   SetPreset(int i);             // theme fades to the preset
    void   SetAccent(const ImVec4& a, const ImVec4& b);

    // window. always call EndWindow(), even if BeginWindow returned false.
    bool BeginWindow(const char* title, ImVec2 size, bool* open = nullptr);
    void EndWindow();
    bool Tab(const char* label, Icon icon, int idx, int* current);   // rail tab with svg icon
    void TabSeparator();
    void BeginContent();
    void EndContent();

    void BeginSubTabs(float width);
    bool SubTab(const char* label, int idx, int* current);
    void EndSubTabs();

    bool BeginGroup(const char* title, ImVec2 size);
    void EndGroup();

    bool Checkbox(const char* label, bool* v);
    bool SliderFloat(const char* label, float* v, float lo, float hi, const char* fmt = "%.1f");
    bool SliderInt(const char* label, int* v, int lo, int hi, const char* fmt = "%d");
    bool Combo(const char* label, int* cur, const char* const* items, int count);
    bool MultiCombo(const char* label, bool* states, const char* const* items, int count);
    bool Button(const char* label, ImVec2 size = ImVec2(-1, 0));
    bool ColorEdit(const char* label, float col[4]);
    bool Keybind(const char* label, ImGuiKey* key);
    void Separator();
    void Spacing(float h = 8.f);

    // toasts, bottom right. 0 = info, 1 = success, 2 = warning
    void Notify(const char* text, int kind = 0, float seconds = 3.f);
    void RenderNotifications();          // call once per frame, even when the menu is hidden
}
