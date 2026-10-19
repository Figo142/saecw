#include "vx.h"
#include "imgui_internal.h"
#include "vx_fonts.h"
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace vx
{
    // ------------------------------------------------------------------ state
    static const float kSide = 196.f, kHead = 42.f, kBottom = 32.f, kPad = 22.f, kRow = 34.f;

    static const Theme kPresets[] = {
        // blue
        { {0.066f,0.074f,0.094f,1}, {0.045f,0.052f,0.068f,1}, {0.090f,0.102f,0.130f,1}, {0.075f,0.086f,0.112f,1},
          {0.110f,0.125f,0.158f,1}, {1,1,1,0.09f}, {0.90f,0.93f,0.98f,1}, {0.48f,0.54f,0.65f,1},
          {0.19f,0.52f,1.00f,1}, {0.32f,0.62f,1.00f,1} },
        // red
        { {0.075f,0.068f,0.070f,1}, {0.050f,0.045f,0.047f,1}, {0.100f,0.090f,0.092f,1}, {0.085f,0.076f,0.078f,1},
          {0.125f,0.112f,0.115f,1}, {1,1,1,0.09f}, {0.95f,0.92f,0.92f,1}, {0.58f,0.52f,0.53f,1},
          {0.92f,0.17f,0.20f,1}, {1.00f,0.32f,0.33f,1} },
        // lime
        { {0.068f,0.072f,0.078f,1}, {0.046f,0.050f,0.055f,1}, {0.092f,0.098f,0.108f,1}, {0.078f,0.083f,0.092f,1},
          {0.118f,0.126f,0.140f,1}, {1,1,1,0.09f}, {0.93f,0.95f,0.95f,1}, {0.52f,0.56f,0.58f,1},
          {0.62f,0.95f,0.22f,1}, {0.72f,1.00f,0.36f,1} },
        // violet
        { {0.074f,0.066f,0.094f,1}, {0.050f,0.044f,0.068f,1}, {0.100f,0.090f,0.130f,1}, {0.085f,0.076f,0.112f,1},
          {0.130f,0.116f,0.170f,1}, {1,1,1,0.09f}, {0.94f,0.92f,0.98f,1}, {0.56f,0.52f,0.66f,1},
          {0.62f,0.38f,1.00f,1}, {0.72f,0.50f,1.00f,1} },
    };
    static const char* kPresetNames[] = { "Blue", "Red", "Lime", "Violet" };
    static_assert(sizeof(Theme) == 10 * sizeof(ImVec4), "theme layout");

    static Theme  g_cur = kPresets[0], g_tgt = kPresets[0];
    static Fonts  g_fonts = {};
    static float  g_time = 0.f, g_dt = 0.016f;
    static float  g_tabY = 0.f, g_ind_y = 0.f, g_base_alpha = 1.f;
    static bool   g_ind_init = false;
    static char   g_title[32] = "";
    static ImVec2 g_wpos, g_wsize;
    static bool*  g_open = nullptr;

    Theme& GetTheme() { return g_cur; }
    Fonts& GetFonts() { return g_fonts; }
    int    PresetCount() { return (int)(sizeof(kPresets) / sizeof(kPresets[0])); }
    const char* PresetName(int i) { return kPresetNames[i]; }
    void   SetPreset(int i) { g_tgt = kPresets[i]; }
    void   SetAccent(const ImVec4& a, const ImVec4& b) { g_tgt.acc1 = a; g_tgt.acc2 = b; }

    // ---------------------------------------------------------------- helpers
    static ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
    {
        return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
    }
    static ImVec4 A(const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, c.w * a); }
    static ImU32  U32(const ImVec4& c) { return ImGui::GetColorU32(c); }
    static float  Clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
    static float  EaseOutCubic(float t) { t = 1.f - Clamp01(t); return 1.f - t * t * t; }

    static ImVec4 OnAccent()
    {
        const ImVec4& c = g_cur.acc1;
        float l = 0.299f * c.x + 0.587f * c.y + 0.114f * c.z;
        return l > 0.55f ? ImVec4(0.04f, 0.05f, 0.06f, 1) : ImVec4(1, 1, 1, 1);
    }

    // exponential approach (hovers)
    static float Anim(ImGuiID id, float target, int slot, float speed = 16.f)
    {
        float& v = *ImGui::GetStateStorage()->GetFloatRef(id + (ImGuiID)slot * 7919u, target);
        float s = g_dt * speed;
        v += (target - v) * (s > 1.f ? 1.f : s);
        if (fabsf(target - v) < 0.001f) v = target;
        return v;
    }
    // linear tween, fixed duration (toggles)
    static float Tween(ImGuiID id, float target, int slot, float dur)
    {
        float& v = *ImGui::GetStateStorage()->GetFloatRef(id + (ImGuiID)slot * 7919u, target);
        float step = g_dt / dur;
        float d = target - v;
        v += d > step ? step : (d < -step ? -step : d);
        return v;
    }

    static void Txt(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 c, const char* s) { dl->AddText(f, f->FontSize, p, c, s); }
    static ImVec2 TxtSize(ImFont* f, const char* s) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.f, s); }

    // ------------------------------------------------------------------ setup
    static void ApplyStyle()
    {
        ImGuiStyle& s = ImGui::GetStyle();
        const Theme& t = g_cur;
        s.PopupRounding = 3.f; s.PopupBorderSize = 1.f; s.ChildBorderSize = 0.f; s.WindowBorderSize = 0.f;
        s.FrameRounding = 3.f; s.GrabRounding = 3.f; s.ScrollbarSize = 4.f; s.ScrollbarRounding = 2.f;
        s.ItemSpacing = ImVec2(10, 6);

        ImVec4* c = s.Colors;
        c[ImGuiCol_Text]            = t.text;
        c[ImGuiCol_TextDisabled]    = t.text_dim;
        c[ImGuiCol_WindowBg]        = t.bg_top;
        c[ImGuiCol_PopupBg]         = t.panel_top;
        c[ImGuiCol_Border]          = A(t.text, 0.12f);
        c[ImGuiCol_FrameBg]         = t.frame;
        c[ImGuiCol_FrameBgHovered]  = Mix(t.frame, t.acc1, 0.15f);
        c[ImGuiCol_FrameBgActive]   = Mix(t.frame, t.acc1, 0.25f);
        c[ImGuiCol_ScrollbarBg]     = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab]   = A(t.acc1, 0.55f);
        c[ImGuiCol_ScrollbarGrabHovered] = A(t.acc1, 0.8f);
        c[ImGuiCol_ScrollbarGrabActive]  = t.acc1;
        c[ImGuiCol_Header]          = A(t.acc1, 0.20f);
        c[ImGuiCol_HeaderHovered]   = A(t.acc1, 0.30f);
        c[ImGuiCol_HeaderActive]    = A(t.acc1, 0.40f);
        c[ImGuiCol_SliderGrab]      = t.acc1;
        c[ImGuiCol_SliderGrabActive] = t.acc2;
    }

    static void ThemeStep()
    {
        ImVec4* c = (ImVec4*)&g_cur;
        const ImVec4* t = (const ImVec4*)&g_tgt;
        float k = 1.f - expf(-g_dt * 8.f);
        for (int i = 0; i < 10; ++i) c[i] = Mix(c[i], t[i], k);
    }

    void Init()
    {
        ImGuiIO& io = ImGui::GetIO();
        static const ImWchar ranges[] = { 0x20, 0x17F, 0 };
        ImFontConfig cfg;
        cfg.OversampleH = 3; cfg.OversampleV = 2; cfg.GlyphRanges = ranges;
        g_fonts.body  = io.Fonts->AddFontFromMemoryCompressedTTF(vx_poppins_medium_compressed_data,   vx_poppins_medium_compressed_size,   21.f, &cfg);
        g_fonts.bold  = io.Fonts->AddFontFromMemoryCompressedTTF(vx_poppins_semibold_compressed_data, vx_poppins_semibold_compressed_size, 21.f, &cfg);
        g_fonts.title = io.Fonts->AddFontFromMemoryCompressedTTF(vx_poppins_semibold_compressed_data, vx_poppins_semibold_compressed_size, 32.f, &cfg);
        g_fonts.small = io.Fonts->AddFontFromMemoryCompressedTTF(vx_poppins_medium_compressed_data,   vx_poppins_medium_compressed_size,   17.f, &cfg);
        io.FontDefault = g_fonts.body;
        g_cur = g_tgt = kPresets[0];
        ApplyStyle();
    }

    // ----------------------------------------------------------------- window
    bool BeginWindow(const char* title, ImVec2 size, bool* open)
    {
        ImGuiIO& io = ImGui::GetIO();
        g_dt = io.DeltaTime > 0.05f ? 0.05f : io.DeltaTime;
        g_time += g_dt;
        ThemeStep();
        ApplyStyle();
        g_open = open;

        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f);
        bool vis = ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
        if (!vis) return false;
        g_base_alpha = ImGui::GetStyle().Alpha;

        const Theme& t = g_cur;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos(), e(p.x + size.x, p.y + size.y);
        g_wpos = p; g_wsize = size;

        dl->AddRectFilled(p, e, U32(t.bg_top), 4.f);
        dl->AddRectFilled(p, ImVec2(p.x + kSide, e.y), U32(t.bg_bot), 4.f, ImDrawFlags_RoundCornersLeft);
        dl->AddLine(ImVec2(p.x + kSide, p.y), ImVec2(p.x + kSide, e.y), U32(A(t.text, 0.08f)), 1.f);
        dl->AddLine(ImVec2(p.x + kSide, p.y + kHead), ImVec2(e.x, p.y + kHead), U32(A(t.text, 0.08f)), 1.f);

        // wordmark
        ImFont* ft = g_fonts.title;
        Txt(dl, ft, ImVec2(p.x + 22, p.y + 16), U32(t.text), "VELOX");
        dl->AddRectFilled(ImVec2(p.x + 22, p.y + 16 + ft->FontSize + 4), ImVec2(p.x + 44, p.y + 16 + ft->FontSize + 6), U32(t.acc1));

        // user line at the bottom of the sidebar
        {
            ImFont* fb = g_fonts.bold; ImFont* fs = g_fonts.small;
            float y = e.y - kBottom - 46;
            dl->AddLine(ImVec2(p.x + 22, y - 10), ImVec2(p.x + kSide - 22, y - 10), U32(A(t.text, 0.08f)), 1.f);
            Txt(dl, fb, ImVec2(p.x + 22, y), U32(t.text), "velox");
            Txt(dl, fs, ImVec2(p.x + 22, y + 24), U32(t.text_dim), "lifetime");
            dl->AddCircleFilled(ImVec2(p.x + kSide - 28, y + 20), 4.f, U32(ImVec4(0.25f, 0.85f, 0.45f, 1)), 10);
        }

        // close
        {
            ImVec2 cc(e.x - 26, p.y + kHead * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(cc.x - 14, cc.y - 14));
            ImGui::InvisibleButton("##vx_close", ImVec2(28, 28));
            float h = Anim(ImGui::GetID("##vx_close"), ImGui::IsItemHovered() ? 1.f : 0.f, 1);
            ImU32 xc = U32(Mix(t.text_dim, t.text, h));
            dl->AddLine(ImVec2(cc.x - 5, cc.y - 5), ImVec2(cc.x + 5, cc.y + 5), xc, 1.8f);
            dl->AddLine(ImVec2(cc.x + 5, cc.y - 5), ImVec2(cc.x - 5, cc.y + 5), xc, 1.8f);
            if (ImGui::IsItemClicked() && g_open) *g_open = false;
        }

        // footer of the content area
        {
            ImFont* fs = g_fonts.small;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f fps", io.Framerate);
            Txt(dl, fs, ImVec2(p.x + kSide + kPad, e.y - kBottom + 7), U32(A(t.text_dim, 0.8f)), "build 1.0.0");
            Txt(dl, fs, ImVec2(e.x - kPad - TxtSize(fs, buf).x, e.y - kBottom + 7), U32(A(t.text_dim, 0.8f)), buf);
        }
        dl->AddRect(p, e, U32(A(t.text, 0.10f)), 4.f);

        // drag handles
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##vx_drag_a", ImVec2(kSide, 76));
        bool drag = ImGui::IsItemActive();
        ImGui::SetCursorPos(ImVec2(kSide, 0));
        ImGui::InvisibleButton("##vx_drag_b", ImVec2(size.x - kSide - 40, kHead));
        drag = drag || ImGui::IsItemActive();
        if (drag) ImGui::SetWindowPos(ImVec2(p.x + io.MouseDelta.x, p.y + io.MouseDelta.y));

        g_tabY = 92.f;
        return true;
    }

    void EndWindow()
    {
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    bool Tab(const char* label, int idx, int* current)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.bold;
        float w = kSide - 1.f, h = 38.f;

        ImGui::SetCursorPos(ImVec2(0.f, g_tabY));
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiID id = ImGui::GetID(label);
        ImGui::InvisibleButton(label, ImVec2(w, h));
        bool clicked = ImGui::IsItemClicked();
        if (clicked && *current != idx) *current = idx;

        bool sel = (*current == idx);
        float a  = Tween(id, sel ? 1.f : 0.f, 1, 0.18f);
        float hv = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 2);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (sel) {
            ImVec2 wp = ImGui::GetWindowPos();
            float ty = p.y - wp.y;
            if (!g_ind_init) { g_ind_y = ty; g_ind_init = true; }
            g_ind_y += (ty - g_ind_y) * (1.f - expf(-g_dt * 18.f));
            ImVec2 a0(p.x, wp.y + g_ind_y), a1(p.x + w, wp.y + g_ind_y + h);
            dl->AddRectFilled(a0, a1, U32(A(t.acc1, 0.10f)));
            dl->AddRectFilled(a0, ImVec2(a0.x + 3, a1.y), U32(t.acc1));
        } else if (hv > 0.01f) {
            dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U32(A(t.text, 0.035f * hv)));
        }
        ImVec4 col = Mix(Mix(t.text_dim, t.text, hv * 0.8f), t.acc1, a);
        Txt(dl, f, ImVec2(p.x + 22, p.y + h * 0.5f - f->FontSize * 0.5f), U32(col), label);
        if (sel) snprintf(g_title, sizeof(g_title), "%s", label);
        g_tabY += h + 2.f;
        return clicked;
    }

    void BeginContent()
    {
        const Theme& t = g_cur;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        ImFont* f = g_fonts.bold;
        Txt(dl, f, ImVec2(p.x + kSide + kPad, p.y + kHead * 0.5f - f->FontSize * 0.5f), U32(t.text), g_title);

        ImGui::SetCursorPos(ImVec2(kSide + kPad, kHead + 16.f));
        ImGui::BeginChild("##vx_content", ImVec2(g_wsize.x - kSide - kPad * 2, g_wsize.y - kHead - kBottom - 20.f), 0,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    void EndContent() { ImGui::EndChild(); }

    void BeginSubTabs(float width)
    {
        ImGui::BeginChild("##vx_sub", ImVec2(width, 0), 0,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    bool SubTab(const char* label, int idx, int* current)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImGui::InvisibleButton(label, ImVec2(w, 32));
        bool clicked = ImGui::IsItemClicked();
        if (clicked) *current = idx;
        float a = Tween(id, *current == idx ? 1.f : 0.f, 1, 0.18f);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 2);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (a > 0.01f) dl->AddRectFilled(ImVec2(p.x, p.y + 6), ImVec2(p.x + 3, p.y + 26), U32(A(t.acc1, a)));
        ImVec4 col = Mix(Mix(t.text_dim, t.text, h * 0.8f), t.text, a);
        Txt(dl, f, ImVec2(p.x + 14, p.y + 16 - f->FontSize * 0.5f), U32(col), label);
        return clicked;
    }

    void EndSubTabs()
    {
        ImGui::EndChild();
        ImGui::SameLine(0, 14);
    }

    // ------------------------------------------------------------------ group
    // outlined box, title sits on the border line
    bool BeginGroup(const char* title, ImVec2 size)
    {
        const Theme& t = g_cur;
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 26));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        bool vis = ImGui::BeginChild(title, size, ImGuiChildFlags_AlwaysUseWindowPadding,
                                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize(), e(p.x + sz.x, p.y + sz.y);
        ImFont* f = g_fonts.bold;
        ImVec2 ts = TxtSize(f, title);
        dl->AddRect(ImVec2(p.x, p.y + 11), ImVec2(e.x, e.y), U32(A(t.text, 0.12f)), 3.f);
        dl->AddRectFilled(ImVec2(p.x + 12, p.y), ImVec2(p.x + 12 + ts.x + 14, p.y + 22), U32(t.bg_top));
        Txt(dl, f, ImVec2(p.x + 19, p.y + 11 - f->FontSize * 0.5f), U32(t.text), title);

        ImGui::SetCursorPos(ImVec2(16, 28));
        return vis;
    }

    void EndGroup() { ImGui::EndChild(); }

    void Spacing(float h) { ImGui::Dummy(ImVec2(1, h)); }

    void Separator()
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + 4), ImVec2(p.x + w, p.y + 4), U32(A(g_cur.text, 0.10f)), 1.f);
        ImGui::Dummy(ImVec2(w, 9));
    }

    // ---------------------------------------------------------------- widgets
    static float RowW() { return ImGui::GetContentRegionAvail().x; }
    static ImVec4 LabelCol(float k) { return Mix(g_cur.text_dim, g_cur.text, 0.55f + 0.45f * k); }

    bool Checkbox(const char* label, bool* v)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowW();
        ImGui::InvisibleButton(label, ImVec2(w, kRow));
        bool changed = false;
        if (ImGui::IsItemClicked()) { *v = !*v; changed = true; }

        float a = Tween(id, *v ? 1.f : 0.f, 1, 0.16f);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 2);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImVec2 b0(p.x, p.y + (kRow - 22.f) * 0.5f), b1(b0.x + 22.f, b0.y + 22.f);
        dl->AddRectFilled(b0, b1, U32(Mix(t.frame, t.acc1, a)), 4.f);
        dl->AddRect(b0, b1, U32(Mix(A(t.text, 0.12f + 0.12f * h), t.acc1, a)), 4.f);
        if (a > 0.01f) {
            float s = a * 15.f;
            ImVec2 c0(b0.x + 5.5f, b0.y + 11.5f), c1(b0.x + 9.5f, b0.y + 15.5f), c2(b0.x + 16.5f, b0.y + 6.5f);
            ImU32 ck = U32(OnAccent());
            float f1 = (s < 5.7f ? s : 5.7f) / 5.7f;
            dl->AddLine(c0, ImVec2(c0.x + (c1.x - c0.x) * f1, c0.y + (c1.y - c0.y) * f1), ck, 2.4f);
            if (s > 5.7f) {
                float f2 = Clamp01((s - 5.7f) / 9.3f);
                dl->AddLine(c1, ImVec2(c1.x + (c2.x - c1.x) * f2, c1.y + (c2.y - c1.y) * f2), ck, 2.4f);
            }
        }
        Txt(dl, f, ImVec2(p.x + 34.f, p.y + kRow * 0.5f - f->FontSize * 0.5f), U32(LabelCol(a > h ? a : h)), label);
        return changed;
    }

    static bool SliderImpl(const char* label, float* v, float lo, float hi, const char* val, bool isInt)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowW(), H = 54.f;
        ImGui::InvisibleButton(label, ImVec2(w, H));

        float bx0 = p.x + 8.f, bx1 = p.x + w - 8.f, by = p.y + 41.f;
        bool changed = false;
        if (ImGui::IsItemActive()) {
            float k = Clamp01((ImGui::GetIO().MousePos.x - bx0) / (bx1 - bx0));
            float nv = lo + (hi - lo) * k;
            if (isInt) nv = roundf(nv);
            if (nv != *v) { *v = nv; changed = true; }
        }
        float f01 = Anim(id, Clamp01((*v - lo) / (hi - lo)), 1, 20.f);
        float h   = Anim(id, (ImGui::IsItemHovered() || ImGui::IsItemActive()) ? 1.f : 0.f, 2);
        float act = Anim(id, ImGui::IsItemActive() ? 1.f : 0.f, 3, 22.f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        Txt(dl, f, ImVec2(p.x, p.y + 4), U32(LabelCol(h)), label);
        ImFont* fb = g_fonts.bold;
        Txt(dl, fb, ImVec2(p.x + w - TxtSize(fb, val).x, p.y + 4), U32(Mix(t.text, t.acc1, 0.7f + 0.3f * act)), val);

        for (int i = 0; i <= 10; ++i) {
            float x = bx0 + (bx1 - bx0) * i / 10.f;
            dl->AddLine(ImVec2(x, by + 8), ImVec2(x, by + (i % 5 == 0 ? 13.f : 11.f)), U32(A(t.text_dim, i / 10.f <= f01 ? 0.55f : 0.22f)), 1.f);
        }
        dl->AddRectFilled(ImVec2(bx0, by - 2.5f), ImVec2(bx1, by + 2.5f), U32(t.frame), 2.5f);
        float fx = bx0 + (bx1 - bx0) * f01;
        if (fx - bx0 > 1.f) dl->AddRectFilled(ImVec2(bx0, by - 2.5f), ImVec2(fx, by + 2.5f), U32(t.acc1), 2.5f);
        float s = 9.f * (1.f + 0.12f * h + 0.22f * act);
        ImVec2 q[4] = { ImVec2(fx, by - s), ImVec2(fx + s, by), ImVec2(fx, by + s), ImVec2(fx - s, by) };
        dl->AddCircle(ImVec2(fx, by), s + 7.f * act, U32(A(t.acc1, 0.28f * act)), 4, 2.f);
        dl->AddConvexPolyFilled(q, 4, U32(t.text));
        dl->AddPolyline(q, 4, U32(t.acc1), ImDrawFlags_Closed, 2.f);
        return changed;
    }

    bool SliderFloat(const char* label, float* v, float lo, float hi, const char* fmt)
    {
        char b[32]; snprintf(b, sizeof(b), fmt, *v);
        return SliderImpl(label, v, lo, hi, b, false);
    }

    bool SliderInt(const char* label, int* v, int lo, int hi, const char* fmt)
    {
        float f = (float)*v; char b[32]; snprintf(b, sizeof(b), fmt, *v);
        bool c = SliderImpl(label, &f, (float)lo, (float)hi, b, true);
        if (c) *v = (int)f;
        return c;
    }

    static void Field(ImDrawList* dl, ImVec2 a, ImVec2 b, float hover, float open)
    {
        const Theme& t = g_cur;
        dl->AddRectFilled(a, b, U32(Mix(t.frame, t.acc1, 0.08f * hover)), 3.f);
        dl->AddRect(a, b, U32(Mix(A(t.text, 0.12f + 0.10f * hover), t.acc1, open)), 3.f);
    }

    template <class Fn>
    static bool DropdownPopup(const char* label, ImVec2 b0, float bw, int count, Fn rows)
    {
        ImGuiID pid = ImGui::GetID(label);
        bool opened = ImGui::IsPopupOpen(label);
        float o = EaseOutCubic(Tween(pid, opened ? 1.f : 0.f, 9, 0.12f));
        ImGui::SetNextWindowPos(ImVec2(b0.x, b0.y + 3.f));
        ImGui::SetNextWindowSize(ImVec2(bw, count * 30.f + 8.f));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_base_alpha * o);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 3.f);
        bool changed = false;
        if (ImGui::BeginPopup(label, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) {
            changed = rows();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(4);
        return changed;
    }

    static bool DropRow(const char* text, bool selected, float w, bool checkStyle)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(text);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(text, ImVec2(w, 30));
        bool clicked = ImGui::IsItemClicked();
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float s = Tween(id, selected ? 1.f : 0.f, 2, 0.12f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 30), U32(A(t.acc1, 0.12f * h + 0.06f * s)), 2.f);
        ImVec4 col = Mix(Mix(t.text_dim, t.text, 0.5f + 0.5f * h), t.acc1, s);
        Txt(dl, f, ImVec2(p.x + 12, p.y + 15 - f->FontSize * 0.5f), U32(col), text);
        if (checkStyle && s > 0.01f) {
            ImVec2 c(p.x + w - 16, p.y + 15);
            dl->AddLine(ImVec2(c.x - 4, c.y), ImVec2(c.x - 1, c.y + 3), U32(A(t.acc1, s)), 2.f);
            dl->AddLine(ImVec2(c.x - 1, c.y + 3), ImVec2(c.x + 5, c.y - 4), U32(A(t.acc1, s)), 2.f);
        }
        return clicked;
    }

    static void DrawChevron(ImDrawList* dl, ImVec2 c, float open, ImU32 col)
    {
        float d = 3.5f * (1.f - 2.f * open);
        dl->AddLine(ImVec2(c.x - 4.5f, c.y - d * 0.5f), ImVec2(c.x, c.y + d * 0.5f), col, 1.8f);
        dl->AddLine(ImVec2(c.x, c.y + d * 0.5f), ImVec2(c.x + 4.5f, c.y - d * 0.5f), col, 1.8f);
    }

    bool Combo(const char* label, int* cur, const char* const* items, int count)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowW(), bw = 160.f;
        ImGui::InvisibleButton(label, ImVec2(w, kRow));
        if (ImGui::IsItemClicked()) ImGui::OpenPopup(label);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float o = Anim(id, ImGui::IsPopupOpen(label) ? 1.f : 0.f, 2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        Txt(dl, f, ImVec2(p.x, p.y + kRow * 0.5f - f->FontSize * 0.5f), U32(LabelCol(h)), label);
        ImVec2 b0(p.x + w - bw, p.y + 2), b1(p.x + w, p.y + kRow - 2);
        Field(dl, b0, b1, h, o);
        dl->PushClipRect(b0, ImVec2(b1.x - 28, b1.y), true);
        Txt(dl, f, ImVec2(b0.x + 12, (b0.y + b1.y) * 0.5f - f->FontSize * 0.5f), U32(t.text), items[*cur]);
        dl->PopClipRect();
        DrawChevron(dl, ImVec2(b1.x - 16, (b0.y + b1.y) * 0.5f), o, U32(Mix(t.text_dim, t.acc1, o)));

        return DropdownPopup(label, ImVec2(b0.x, b1.y), bw, count, [&]() {
            bool ch = false;
            for (int i = 0; i < count; ++i)
                if (DropRow(items[i], i == *cur, bw - 8, false)) { *cur = i; ch = true; ImGui::CloseCurrentPopup(); }
            return ch;
        });
    }

    bool MultiCombo(const char* label, bool* states, const char* const* items, int count)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowW(), bw = 160.f;
        ImGui::InvisibleButton(label, ImVec2(w, kRow));
        if (ImGui::IsItemClicked()) ImGui::OpenPopup(label);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float o = Anim(id, ImGui::IsPopupOpen(label) ? 1.f : 0.f, 2);

        char sum[96] = {};
        int n = 0;
        for (int i = 0; i < count; ++i) if (states[i]) {
            size_t l = strlen(sum);
            if (l + strlen(items[i]) + 3 < sizeof(sum)) snprintf(sum + l, sizeof(sum) - l, n ? ", %s" : "%s", items[i]);
            ++n;
        }
        if (!n) snprintf(sum, sizeof(sum), "none");

        ImDrawList* dl = ImGui::GetWindowDrawList();
        Txt(dl, f, ImVec2(p.x, p.y + kRow * 0.5f - f->FontSize * 0.5f), U32(LabelCol(h)), label);
        ImVec2 b0(p.x + w - bw, p.y + 2), b1(p.x + w, p.y + kRow - 2);
        Field(dl, b0, b1, h, o);
        dl->PushClipRect(b0, ImVec2(b1.x - 28, b1.y), true);
        Txt(dl, f, ImVec2(b0.x + 12, (b0.y + b1.y) * 0.5f - f->FontSize * 0.5f), U32(t.text), sum);
        dl->PopClipRect();
        DrawChevron(dl, ImVec2(b1.x - 16, (b0.y + b1.y) * 0.5f), o, U32(Mix(t.text_dim, t.acc1, o)));

        return DropdownPopup(label, ImVec2(b0.x, b1.y), bw, count, [&]() {
            bool ch = false;
            for (int i = 0; i < count; ++i)
                if (DropRow(items[i], states[i], bw - 8, true)) { states[i] = !states[i]; ch = true; }
            return ch;
        });
    }

    bool Button(const char* label, ImVec2 size)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.bold;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        if (size.x <= 0.f) size.x = RowW();
        if (size.y <= 0.f) size.y = 38.f;
        ImGui::InvisibleButton(label, size);
        bool clicked = ImGui::IsItemClicked();
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float d = Anim(id, ImGui::IsItemActive() ? 1.f : 0.f, 2, 24.f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 e(p.x + size.x, p.y + size.y);
        dl->AddRectFilled(p, e, U32(Mix(t.frame, t.acc1, h * 0.9f - 0.15f * d)), 3.f);
        dl->AddRect(p, e, U32(Mix(A(t.text, 0.14f), t.acc1, 0.6f + 0.4f * h)), 3.f);
        ImVec2 ts = TxtSize(f, label);
        Txt(dl, f, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), U32(Mix(t.text, OnAccent(), h)), label);
        return clicked;
    }

    bool ColorEdit(const char* label, float col[4])
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowW();
        ImGui::InvisibleButton(label, ImVec2(w, kRow));
        if (ImGui::IsItemClicked()) ImGui::OpenPopup(label);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        Txt(dl, f, ImVec2(p.x, p.y + kRow * 0.5f - f->FontSize * 0.5f), U32(LabelCol(h)), label);
        ImVec2 s0(p.x + w - 46, p.y + (kRow - 22) * 0.5f), s1(s0.x + 46, s0.y + 22);
        dl->AddRectFilled(s0, s1, ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 1.f)), 3.f);
        dl->AddRect(s0, s1, U32(Mix(A(t.text, 0.14f), t.text, h)), 3.f);

        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(s1.x - 230, s1.y + 5));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
        if (ImGui::BeginPopup(label)) {
            changed = ImGui::ColorPicker4("##vx_pick", col,
                ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar);
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        return changed;
    }

    bool Keybind(const char* label, ImGuiKey* key)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowW(), bw = 96.f;
        ImGui::InvisibleButton(label, ImVec2(w, kRow));
        ImGuiStorage* st = ImGui::GetStateStorage();
        bool waiting = st->GetInt(id + 3, 0) != 0, changed = false;

        if (ImGui::IsItemClicked() && !waiting) {
            waiting = true;
        } else if (waiting) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                *key = ImGuiKey_None; changed = true; waiting = false;
            } else {
                for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                    if (k >= ImGuiKey_GamepadStart && k <= ImGuiKey_GamepadRStickDown) continue;
                    if (k == ImGuiKey_MouseWheelX || k == ImGuiKey_MouseWheelY) continue;
                    if (ImGui::IsKeyPressed((ImGuiKey)k, false)) { *key = (ImGuiKey)k; changed = true; waiting = false; break; }
                }
            }
            if (waiting && ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()) waiting = false;
        }
        st->SetInt(id + 3, waiting ? 1 : 0);

        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float o = Anim(id, waiting ? 1.f : 0.f, 2);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        Txt(dl, f, ImVec2(p.x, p.y + kRow * 0.5f - f->FontSize * 0.5f), U32(LabelCol(h)), label);
        ImVec2 b0(p.x + w - bw, p.y + 2), b1(p.x + w, p.y + kRow - 2);
        Field(dl, b0, b1, h, o);
        char buf[32];
        if (waiting) snprintf(buf, sizeof(buf), "%s", (int)(g_time * 3) % 2 ? "..." : " . ");
        else snprintf(buf, sizeof(buf), "%s", *key == ImGuiKey_None ? "none" : ImGui::GetKeyName(*key));
        ImVec2 ts = TxtSize(f, buf);
        Txt(dl, f, ImVec2(b0.x + (bw - ts.x) * 0.5f, (b0.y + b1.y) * 0.5f - f->FontSize * 0.5f), U32(waiting ? t.acc1 : t.text), buf);
        return changed;
    }

    // ---------------------------------------------------------------- toasts
    struct Toast { std::string text; int kind; float life, age; };
    static std::vector<Toast> g_toasts;

    void Notify(const char* text, int kind, float seconds)
    {
        g_toasts.push_back({ text, kind, seconds, 0.f });
        if (g_toasts.size() > 6) g_toasts.erase(g_toasts.begin());
    }

    void RenderNotifications()
    {
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime > 0.05f ? 0.05f : io.DeltaTime;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        float y = io.DisplaySize.y - 20.f;
        for (size_t i = 0; i < g_toasts.size();) {
            Toast& n = g_toasts[i];
            n.age += dt;
            if (n.age >= n.life + 0.25f) { g_toasts.erase(g_toasts.begin() + i); continue; }

            float in = EaseOutCubic(Clamp01(n.age / 0.22f));
            float out = n.age > n.life ? EaseOutCubic((n.age - n.life) / 0.25f) : 0.f;
            float W = 300.f, H = 48.f;
            float x = io.DisplaySize.x - 20.f - W + (1.f - in) * (W + 30.f) + out * (W + 30.f);
            y -= H;
            ImVec2 a(x, y), b(x + W, y + H);
            ImVec4 kc = n.kind == 1 ? ImVec4(0.25f, 0.85f, 0.45f, 1) : (n.kind == 2 ? ImVec4(1.f, 0.70f, 0.20f, 1) : t.acc1);

            dl->AddRectFilled(a, b, U32(A(t.panel_top, 0.98f)), 3.f);
            dl->AddRect(a, b, U32(A(t.text, 0.12f)), 3.f);
            dl->AddRectFilled(a, ImVec2(a.x + 3, b.y), U32(kc), 3.f, ImDrawFlags_RoundCornersLeft);
            Txt(dl, f, ImVec2(a.x + 18, a.y + H * 0.5f - f->FontSize * 0.5f - 1), U32(t.text), n.text.c_str());
            float k = Clamp01(1.f - n.age / n.life);
            dl->AddRectFilled(ImVec2(a.x + 3, b.y - 2), ImVec2(a.x + 3 + (W - 3) * k, b.y), U32(A(kc, 0.8f)));
            y -= 8.f;
            ++i;
        }
    }
}
