#include "nx.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cmath>
#include <cstring>

namespace nx
{
    static Theme g_theme;
    Theme& GetTheme() { return g_theme; }

    static ImU32 U32(const ImVec4& c, float a = 1.f)
    {
        return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * a));
    }
    static ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
    {
        return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                      a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
    }
    static float Anim(ImGuiID id, float target, int slot = 0)
    {
        ImGuiStorage* st = ImGui::GetStateStorage();
        float& v = *st->GetFloatRef(id + (ImGuiID)slot, 0.f);
        float step = ImGui::GetIO().DeltaTime * g_theme.anim_speed;
        v += (target - v) * (step > 1.f ? 1.f : step);
        if (fabsf(target - v) < 0.001f) v = target;
        return v;
    }

    void SetAccent(const ImVec4& c) { g_theme.accent = c; ApplyStyle(); }

    void ApplyStyle()
    {
        ImGuiStyle& s = ImGui::GetStyle();
        const Theme& t = g_theme;
        s.WindowPadding     = ImVec2(0, 0);
        s.WindowBorderSize  = 0.f;
        s.ChildBorderSize   = 0.f;
        s.PopupBorderSize   = 1.f;
        s.PopupRounding     = 6.f;
        s.ChildRounding     = 6.f;
        s.FrameRounding     = 4.f;
        s.ItemSpacing       = ImVec2(8, 6);
        s.ScrollbarSize     = 4.f;
        s.ScrollbarRounding = 4.f;
        s.AntiAliasedLines  = true;
        s.AntiAliasedFill   = true;

        ImVec4* c = s.Colors;
        c[ImGuiCol_Text]             = t.text;
        c[ImGuiCol_TextDisabled]     = t.text_dim;
        c[ImGuiCol_WindowBg]         = t.window;
        c[ImGuiCol_ChildBg]          = t.group;
        c[ImGuiCol_PopupBg]          = Mix(t.frame, t.window, 0.35f);
        c[ImGuiCol_Border]           = t.border;
        c[ImGuiCol_FrameBg]          = t.frame;
        c[ImGuiCol_FrameBgHovered]   = Mix(t.frame, t.accent, 0.10f);
        c[ImGuiCol_FrameBgActive]    = Mix(t.frame, t.accent, 0.18f);
        c[ImGuiCol_ScrollbarBg]      = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab]    = Mix(t.frame, t.accent, 0.5f);
        c[ImGuiCol_Header]           = Mix(t.frame, t.accent, 0.20f);
        c[ImGuiCol_HeaderHovered]    = Mix(t.frame, t.accent, 0.30f);
        c[ImGuiCol_HeaderActive]     = Mix(t.frame, t.accent, 0.40f);
    }

    // ---------------------------------------------------------------- window
    bool BeginWindow(const char* title, const char* subtitle, ImVec2 size, bool* open)
    {
        const Theme& t = g_theme;
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, t.rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        bool vis = ImGui::Begin(title, open,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
        if (!vis) return false;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        ImVec2 e = ImVec2(p.x + size.x, p.y + size.y);

        // drop glow + body
        for (int i = 4; i >= 1; --i)
            dl->AddRect(ImVec2(p.x - i * 2, p.y - i * 2), ImVec2(e.x + i * 2, e.y + i * 2),
                        U32(t.accent, 0.035f), t.rounding + i * 2, 0, 2.f);
        dl->AddRectFilled(p, e, U32(t.window), t.rounding);
        dl->AddRectFilled(p, ImVec2(p.x + t.sidebar_w, e.y), U32(t.sidebar), t.rounding,
                          ImDrawFlags_RoundCornersLeft);
        dl->AddLine(ImVec2(p.x + t.sidebar_w, p.y), ImVec2(p.x + t.sidebar_w, e.y), U32(t.border), 1.f);
        dl->AddRect(p, e, U32(t.border), t.rounding);

        // accent top edge
        dl->AddRectFilledMultiColor(ImVec2(p.x + t.rounding, p.y), ImVec2(e.x - t.rounding, p.y + 2),
            U32(t.accent, 0.f), U32(t.accent), U32(t.accent), U32(t.accent, 0.f));

        // logo block
        dl->AddText(ImVec2(p.x + 22, p.y + 20), U32(t.text), title);
        dl->AddText(ImVec2(p.x + 22, p.y + 38), U32(t.accent), subtitle);
        dl->AddLine(ImVec2(p.x + 18, p.y + 62), ImVec2(p.x + t.sidebar_w - 18, p.y + 62), U32(t.border), 1.f);

        // drag by sidebar header
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##nx_drag", ImVec2(size.x, 62));
        if (ImGui::IsItemActive()) ImGui::SetWindowPos(
            ImVec2(p.x + ImGui::GetIO().MouseDelta.x, p.y + ImGui::GetIO().MouseDelta.y));

        ImGui::SetCursorPos(ImVec2(0, 74));
        return true;
    }

    void EndWindow()
    {
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void SidebarLabel(const char* label)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + 22, p.y + 4), U32(g_theme.text_dim, 0.7f), label);
        ImGui::Dummy(ImVec2(g_theme.sidebar_w, 26));
    }

    bool SidebarTab(const char* label, int idx, int* current)
    {
        const Theme& t = g_theme;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 sz(t.sidebar_w, 34);
        ImGui::InvisibleButton(label, sz);
        bool clicked = ImGui::IsItemClicked();
        if (clicked) *current = idx;

        bool sel = (*current == idx);
        float a  = Anim(id, sel ? 1.f : 0.f, 1);
        float h  = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (a > 0.01f) {
            dl->AddRectFilledMultiColor(p, ImVec2(p.x + sz.x, p.y + sz.y),
                U32(t.accent, 0.16f * a), U32(t.accent, 0.f), U32(t.accent, 0.f), U32(t.accent, 0.16f * a));
            dl->AddRectFilled(ImVec2(p.x, p.y + 7), ImVec2(p.x + 3, p.y + sz.y - 7), U32(t.accent, a), 2.f);
        }
        ImVec4 col = Mix(Mix(t.text_dim, t.text, h * 0.7f), t.text, a);
        dl->AddText(ImVec2(p.x + 22 + 4 * a, p.y + 9), U32(col), label);
        return clicked;
    }

    void BeginContent()
    {
        const Theme& t = g_theme;
        ImVec2 ws = ImGui::GetWindowSize();
        ImGui::SetCursorPos(ImVec2(t.sidebar_w + 16, 16));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("##nx_content", ImVec2(ws.x - t.sidebar_w - 32, ws.y - 32), false,
                          ImGuiWindowFlags_NoBackground);
    }

    void EndContent()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    // ----------------------------------------------------------------- group
    bool BeginGroup(const char* title, ImVec2 size)
    {
        const Theme& t = g_theme;
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, t.group);
        bool vis = ImGui::BeginChild(title, size, false, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        float w = ImGui::GetWindowWidth();
        dl->AddRect(p, ImVec2(p.x + w, p.y + ImGui::GetWindowHeight()), U32(t.border), 8.f);
        dl->AddText(ImVec2(p.x + 14, p.y + 10), U32(t.text), title);
        dl->AddRectFilled(ImVec2(p.x + 14, p.y + 28), ImVec2(p.x + 34, p.y + 30), U32(t.accent), 1.f);
        dl->AddLine(ImVec2(p.x + 34, p.y + 29), ImVec2(p.x + w - 14, p.y + 29), U32(t.border), 1.f);

        ImGui::SetCursorPos(ImVec2(14, 42));
        return vis;
    }

    void EndGroup() { ImGui::EndChild(); }

    void Separator()
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + 2), ImVec2(p.x + w, p.y + 2), U32(g_theme.border), 1.f);
        ImGui::Dummy(ImVec2(w, 6));
    }

    // ---------------------------------------------------------------- widgets
    static float RowWidth() { return ImGui::GetContentRegionAvail().x - 14.f; }

    bool Checkbox(const char* label, bool* v)
    {
        const Theme& t = g_theme;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowWidth();
        ImVec2 sz(w, 22);
        ImGui::InvisibleButton(label, sz);
        bool changed = false;
        if (ImGui::IsItemClicked()) { *v = !*v; changed = true; }

        float a = Anim(id, *v ? 1.f : 0.f, 1);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddText(ImVec2(p.x, p.y + 3), U32(Mix(t.text_dim, t.text, 0.4f + 0.6f * (a > h ? a : h))), label);

        ImVec2 s0(p.x + w - 30, p.y + 4), s1(p.x + w, p.y + 18);
        dl->AddRectFilled(s0, s1, U32(Mix(t.frame, t.accent, a)), 7.f);
        dl->AddRect(s0, s1, U32(Mix(t.border, t.accent, a * 0.6f)), 7.f);
        float kx = s0.x + 7.f + 16.f * a;
        dl->AddCircleFilled(ImVec2(kx, p.y + 11), 4.5f, U32(Mix(t.text_dim, ImVec4(1, 1, 1, 1), a)), 16);
        return changed;
    }

    static bool SliderImpl(const char* label, float* v, float lo, float hi, const char* valText, bool isInt)
    {
        const Theme& t = g_theme;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowWidth();
        ImVec2 sz(w, 34);
        ImGui::InvisibleButton(label, sz);

        bool changed = false;
        float bx0 = p.x, bx1 = p.x + w, by = p.y + 24;
        if (ImGui::IsItemActive()) {
            float f = (ImGui::GetIO().MousePos.x - bx0) / (bx1 - bx0);
            f = f < 0.f ? 0.f : (f > 1.f ? 1.f : f);
            float nv = lo + (hi - lo) * f;
            if (isInt) nv = roundf(nv);
            if (nv != *v) { *v = nv; changed = true; }
        }

        float target = (*v - lo) / (hi - lo);
        float f = Anim(id, target, 1);
        float h = Anim(id, (ImGui::IsItemHovered() || ImGui::IsItemActive()) ? 1.f : 0.f, 2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddText(ImVec2(p.x, p.y + 1), U32(Mix(t.text_dim, t.text, 0.5f + 0.5f * h)), label);
        ImVec2 ts = ImGui::CalcTextSize(valText);
        dl->AddText(ImVec2(bx1 - ts.x, p.y + 1), U32(t.accent), valText);

        dl->AddRectFilled(ImVec2(bx0, by - 2), ImVec2(bx1, by + 2), U32(t.frame), 2.f);
        float fx = bx0 + (bx1 - bx0) * f;
        dl->AddRectFilled(ImVec2(bx0, by - 2), ImVec2(fx, by + 2), U32(t.accent), 2.f);
        dl->AddCircleFilled(ImVec2(fx, by), 4.f + 1.5f * h, U32(ImVec4(1, 1, 1, 1)), 16);
        dl->AddCircle(ImVec2(fx, by), 6.f + 2.f * h, U32(t.accent, 0.25f * h), 16, 2.f);
        return changed;
    }

    bool SliderFloat(const char* label, float* v, float lo, float hi, const char* fmt)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), fmt, *v);
        return SliderImpl(label, v, lo, hi, buf, false);
    }

    bool SliderInt(const char* label, int* v, int lo, int hi, const char* fmt)
    {
        float f = (float)*v;
        char buf[32];
        snprintf(buf, sizeof(buf), fmt, *v);
        bool c = SliderImpl(label, &f, (float)lo, (float)hi, buf, true);
        if (c) *v = (int)f;
        return c;
    }

    static void FieldBox(ImDrawList* dl, ImVec2 a, ImVec2 b, float hover, float open)
    {
        const Theme& t = g_theme;
        dl->AddRectFilled(a, b, U32(Mix(t.frame, t.accent, 0.08f * hover)), 5.f);
        dl->AddRect(a, b, U32(Mix(t.border, t.accent, open * 0.8f)), 5.f);
    }

    bool Combo(const char* label, int* cur, const char* const* items, int count)
    {
        const Theme& t = g_theme;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowWidth();
        ImGui::InvisibleButton(label, ImVec2(w, 22));
        if (ImGui::IsItemClicked()) ImGui::OpenPopup(label);

        bool opened = ImGui::IsPopupOpen(label);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float o = Anim(id, opened ? 1.f : 0.f, 2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float bw = 118.f;
        ImVec2 b0(p.x + w - bw, p.y), b1(p.x + w, p.y + 22);
        dl->AddText(ImVec2(p.x, p.y + 4), U32(Mix(t.text_dim, t.text, 0.5f + 0.5f * h)), label);
        FieldBox(dl, b0, b1, h, o);
        dl->PushClipRect(b0, ImVec2(b1.x - 18, b1.y), true);
        dl->AddText(ImVec2(b0.x + 8, p.y + 4), U32(t.text), items[*cur]);
        dl->PopClipRect();
        // chevron
        float cx = b1.x - 12, cy = p.y + 11;
        float d = 3.f * (1.f - 2.f * o);
        dl->AddLine(ImVec2(cx - 3, cy - d * 0.5f), ImVec2(cx, cy + d * 0.5f), U32(t.text_dim), 1.5f);
        dl->AddLine(ImVec2(cx, cy + d * 0.5f), ImVec2(cx + 3, cy - d * 0.5f), U32(t.text_dim), 1.5f);

        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(b0.x, b1.y + 3));
        ImGui::SetNextWindowSize(ImVec2(bw, count * 24.f + 8.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        if (ImGui::BeginPopup(label, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
            for (int i = 0; i < count; ++i) {
                ImGui::PushStyleColor(ImGuiCol_Text, i == *cur ? t.accent : t.text);
                if (ImGui::Selectable(items[i], i == *cur, 0, ImVec2(0, 24))) { *cur = i; changed = true; }
                ImGui::PopStyleColor();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        return changed;
    }

    bool MultiCombo(const char* label, bool* states, const char* const* items, int count)
    {
        const Theme& t = g_theme;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowWidth();
        ImGui::InvisibleButton(label, ImVec2(w, 22));
        if (ImGui::IsItemClicked()) ImGui::OpenPopup(label);

        bool opened = ImGui::IsPopupOpen(label);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float o = Anim(id, opened ? 1.f : 0.f, 2);

        char sum[96] = {};
        int n = 0;
        for (int i = 0; i < count; ++i) if (states[i]) {
            size_t l = strlen(sum);
            if (l + strlen(items[i]) + 3 < sizeof(sum)) snprintf(sum + l, sizeof(sum) - l, n ? ", %s" : "%s", items[i]);
            ++n;
        }
        if (!n) snprintf(sum, sizeof(sum), "none");

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float bw = 118.f;
        ImVec2 b0(p.x + w - bw, p.y), b1(p.x + w, p.y + 22);
        dl->AddText(ImVec2(p.x, p.y + 4), U32(Mix(t.text_dim, t.text, 0.5f + 0.5f * h)), label);
        FieldBox(dl, b0, b1, h, o);
        dl->PushClipRect(b0, ImVec2(b1.x - 8, b1.y), true);
        dl->AddText(ImVec2(b0.x + 8, p.y + 4), U32(t.text), sum);
        dl->PopClipRect();

        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(b0.x, b1.y + 3));
        ImGui::SetNextWindowSize(ImVec2(bw, count * 24.f + 8.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        if (ImGui::BeginPopup(label, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
            for (int i = 0; i < count; ++i) {
                ImGui::PushStyleColor(ImGuiCol_Text, states[i] ? t.accent : t.text);
                if (ImGui::Selectable(items[i], states[i], ImGuiSelectableFlags_NoAutoClosePopups, ImVec2(0, 24))) {
                    states[i] = !states[i];
                    changed = true;
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        return changed;
    }

    bool Button(const char* label, ImVec2 size)
    {
        const Theme& t = g_theme;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        if (size.x <= 0) size.x = RowWidth() + size.x + 1;
        if (size.y <= 0) size.y = 26;
        ImGui::InvisibleButton(label, size);
        bool clicked = ImGui::IsItemClicked();
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float d = Anim(id, ImGui::IsItemActive() ? 1.f : 0.f, 2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 e(p.x + size.x, p.y + size.y);
        dl->AddRectFilled(p, e, U32(Mix(t.frame, t.accent, 0.12f + 0.25f * h + 0.2f * d)), 5.f);
        dl->AddRect(p, e, U32(Mix(t.border, t.accent, 0.4f + 0.6f * h)), 5.f);
        ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
        dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), U32(t.text), label);
        return clicked;
    }

    bool ColorEdit(const char* label, float col[4])
    {
        const Theme& t = g_theme;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowWidth();
        ImGui::InvisibleButton(label, ImVec2(w, 22));
        if (ImGui::IsItemClicked()) ImGui::OpenPopup(label);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddText(ImVec2(p.x, p.y + 3), U32(Mix(t.text_dim, t.text, 0.5f + 0.5f * h)), label);
        ImVec2 s0(p.x + w - 30, p.y + 4), s1(p.x + w, p.y + 18);
        dl->AddRectFilled(s0, s1, ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 1.f)), 4.f);
        dl->AddRect(s0, s1, U32(Mix(t.border, t.accent, h)), 4.f);

        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(s1.x - 200, s1.y + 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
        if (ImGui::BeginPopup(label)) {
            changed = ImGui::ColorPicker4("##pick", col,
                ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        return changed;
    }

    bool Keybind(const char* label, ImGuiKey* key)
    {
        const Theme& t = g_theme;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowWidth();
        ImGui::InvisibleButton(label, ImVec2(w, 22));
        ImGuiStorage* st = ImGui::GetStateStorage();
        bool waiting = st->GetInt(id + 3, 0) != 0;
        bool changed = false;

        if (ImGui::IsItemClicked() && !waiting) {
            waiting = true;
        } else if (waiting) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                *key = ImGuiKey_None; changed = true; waiting = false;
            } else {
                for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                    if (k >= ImGuiKey_GamepadStart && k < ImGuiKey_GamepadRStickDown + 1) continue;
                    if (k == ImGuiKey_MouseWheelX || k == ImGuiKey_MouseWheelY) continue;
                    if (ImGui::IsKeyPressed((ImGuiKey)k, false)) {
                        *key = (ImGuiKey)k; changed = true; waiting = false;
                        break;
                    }
                }
            }
            if (waiting && ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()) waiting = false;
        }
        st->SetInt(id + 3, waiting ? 1 : 0);

        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float o = Anim(id, waiting ? 1.f : 0.f, 2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddText(ImVec2(p.x, p.y + 4), U32(Mix(t.text_dim, t.text, 0.5f + 0.5f * h)), label);
        float bw = 76.f;
        ImVec2 b0(p.x + w - bw, p.y), b1(p.x + w, p.y + 22);
        FieldBox(dl, b0, b1, h, o);
        char buf[32];
        if (waiting) snprintf(buf, sizeof(buf), "...");
        else snprintf(buf, sizeof(buf), "%s", *key == ImGuiKey_None ? "none" : ImGui::GetKeyName(*key));
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(b0.x + (bw - ts.x) * 0.5f, p.y + 4), U32(waiting ? t.accent : t.text), buf);
        return changed;
    }
}
