#pragma once
#include "imgui.h"

// nx: imgui widget kit, neverlose / fatality inspired.
// backend agnostic. needs imgui.h + imgui_internal.h only.
namespace nx
{
    struct Theme
    {
        ImVec4 accent     = ImVec4(0.06f, 0.61f, 1.00f, 1.00f);
        ImVec4 window     = ImVec4(0.039f, 0.043f, 0.059f, 1.00f);
        ImVec4 sidebar    = ImVec4(0.027f, 0.031f, 0.043f, 1.00f);
        ImVec4 group      = ImVec4(0.055f, 0.063f, 0.086f, 1.00f);
        ImVec4 frame      = ImVec4(0.086f, 0.098f, 0.133f, 1.00f);
        ImVec4 border     = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
        ImVec4 text       = ImVec4(0.92f, 0.93f, 0.96f, 1.00f);
        ImVec4 text_dim   = ImVec4(0.45f, 0.48f, 0.56f, 1.00f);
        float  rounding   = 8.f;
        float  sidebar_w  = 170.f;
        float  anim_speed = 14.f;
    };

    Theme& GetTheme();
    void   SetAccent(const ImVec4& c);
    void   ApplyStyle();                   // call once after CreateContext

    // window frame: sidebar + content region
    bool BeginWindow(const char* title, const char* subtitle, ImVec2 size, bool* open = nullptr);
    void EndWindow();
    void SidebarLabel(const char* label);
    bool SidebarTab(const char* label, int idx, int* current);
    void BeginContent();                   // call after the sidebar tabs
    void EndContent();

    // group box (child with title)
    bool BeginGroup(const char* title, ImVec2 size);
    void EndGroup();

    // widgets
    bool Checkbox(const char* label, bool* v);
    bool SliderFloat(const char* label, float* v, float lo, float hi, const char* fmt = "%.1f");
    bool SliderInt(const char* label, int* v, int lo, int hi, const char* fmt = "%d");
    bool Combo(const char* label, int* cur, const char* const* items, int count);
    bool MultiCombo(const char* label, bool* states, const char* const* items, int count);
    bool Button(const char* label, ImVec2 size = ImVec2(-1, 26));
    bool ColorEdit(const char* label, float col[4]);
    bool Keybind(const char* label, ImGuiKey* key);
    void Separator();
}
