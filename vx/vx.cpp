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
    static const float kSide = 224.f, kHead = 84.f, kBottom = 36.f, kPad = 26.f, kRow = 36.f;

    static const Theme kPresets[] = {
        // toxic: graphite + lime/teal
        { {0.064f,0.068f,0.078f,1}, {0.034f,0.037f,0.044f,1}, {0.095f,0.101f,0.114f,1}, {0.078f,0.083f,0.095f,1},
          {0.130f,0.138f,0.156f,1}, {1,1,1,0.07f}, {0.93f,0.95f,0.96f,1}, {0.52f,0.56f,0.61f,1},
          {0.64f,1.00f,0.22f,1}, {0.10f,0.92f,0.72f,1} },
        // ember: warm graphite + amber/red
        { {0.078f,0.066f,0.064f,1}, {0.042f,0.035f,0.035f,1}, {0.112f,0.094f,0.090f,1}, {0.090f,0.076f,0.074f,1},
          {0.150f,0.128f,0.122f,1}, {1,1,1,0.07f}, {0.97f,0.94f,0.92f,1}, {0.62f,0.55f,0.52f,1},
          {1.00f,0.62f,0.10f,1}, {1.00f,0.25f,0.22f,1} },
        // ice: cold slate + cyan/periwinkle
        { {0.056f,0.066f,0.086f,1}, {0.030f,0.038f,0.054f,1}, {0.082f,0.098f,0.126f,1}, {0.066f,0.080f,0.104f,1},
          {0.112f,0.134f,0.172f,1}, {1,1,1,0.07f}, {0.93f,0.96f,1.00f,1}, {0.50f,0.58f,0.70f,1},
          {0.35f,0.82f,1.00f,1}, {0.58f,0.55f,1.00f,1} },
        // violet: neutral purple + orchid/blue
        { {0.072f,0.062f,0.098f,1}, {0.040f,0.034f,0.062f,1}, {0.106f,0.092f,0.140f,1}, {0.086f,0.074f,0.116f,1},
          {0.142f,0.124f,0.186f,1}, {1,1,1,0.07f}, {0.95f,0.93f,0.98f,1}, {0.58f,0.54f,0.68f,1},
          {0.78f,0.42f,1.00f,1}, {0.36f,0.56f,1.00f,1} },
    };
    static const char* kPresetNames[] = { "Toxic", "Ember", "Ice", "Violet" };
    static_assert(sizeof(Theme) == 10 * sizeof(ImVec4), "theme layout");

    static Theme  g_cur = kPresets[0], g_tgt = kPresets[0];
    static Fonts  g_fonts = {};
    static float  g_time = 0.f, g_dt = 0.016f, g_alpha = 0.f, g_page = 1.f;
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
    static ImVec4 Grad(float t) { return Mix(g_cur.acc1, g_cur.acc2, t); }
    static float  Clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
    static float  EaseOutCubic(float t) { t = 1.f - Clamp01(t); return 1.f - t * t * t; }
    static float  EaseOutBack(float t)
    {
        const float c1 = 1.70158f, c3 = c1 + 1.f;
        float u = t - 1.f;
        return 1.f + c3 * u * u * u + c1 * u * u;
    }

    // exponential approach (hovers)
    static float Anim(ImGuiID id, float target, int slot, float speed = 14.f)
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

    static void Txt(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 c, const char* s)
    {
        dl->AddText(f, f->FontSize, p, c, s);
    }
    static ImVec2 TxtSize(ImFont* f, const char* s) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.f, s); }

    static void GradText(ImDrawList* dl, ImFont* f, ImVec2 p, const char* s, float phase, float spacing, float alpha = 1.f)
    {
        float len = 0.f;
        for (const char* c = s; *c; ++c) len += TxtSize(f, std::string(1, *c).c_str()).x + spacing;
        float x = p.x, acc = 0.f;
        for (const char* c = s; *c; ++c) {
            char ch[2] = { *c, 0 };
            float t = 0.5f + 0.5f * sinf(phase + acc / (len + 1.f) * 4.2f);
            dl->AddText(f, f->FontSize, ImVec2(x, p.y), U32(A(Grad(t), alpha)), ch);
            float w = TxtSize(f, ch).x + spacing;
            x += w; acc += w;
        }
    }

    static void GradH(ImDrawList* dl, ImVec2 a, ImVec2 b, const ImVec4& cl, const ImVec4& cr, float r)
    {
        float w = b.x - a.x, h = b.y - a.y;
        if (w <= 0.f || h <= 0.f) return;
        r = fminf(r, fminf(w, h) * 0.5f);
        int n = (int)ceilf(w);
        for (int i = 0; i < n; ++i) {
            float x = a.x + i, x1 = fminf(x + 1.f, b.x), cx = x + 0.5f - a.x, inset = 0.f;
            if (cx < r)          { float d = r - cx;       inset = r - sqrtf(fmaxf(r * r - d * d, 0.f)); }
            else if (cx > w - r) { float d = cx - (w - r); inset = r - sqrtf(fmaxf(r * r - d * d, 0.f)); }
            dl->AddRectFilled(ImVec2(x, a.y + inset), ImVec2(x1, b.y - inset), U32(Mix(cl, cr, cx / w)));
        }
    }

    static void GradV(ImDrawList* dl, ImVec2 a, ImVec2 b, const ImVec4& ct, const ImVec4& cb, float r)
    {
        float w = b.x - a.x, h = b.y - a.y;
        if (w <= 0.f || h <= 0.f) return;
        r = fminf(r, fminf(w, h) * 0.5f);
        for (int i = 0; i < (int)ceilf(r); ++i) {
            float d = r - i - 0.5f, inset = r - sqrtf(fmaxf(r * r - d * d, 0.f));
            dl->AddRectFilled(ImVec2(a.x + inset, a.y + i), ImVec2(b.x - inset, a.y + i + 1.f), U32(Mix(ct, cb, (i + 0.5f) / h)));
            dl->AddRectFilled(ImVec2(a.x + inset, b.y - i - 1.f), ImVec2(b.x - inset, b.y - i), U32(Mix(ct, cb, 1.f - (i + 0.5f) / h)));
        }
        float t0 = r / h;
        dl->AddRectFilledMultiColor(ImVec2(a.x, a.y + ceilf(r)), ImVec2(b.x, b.y - ceilf(r)),
            U32(Mix(ct, cb, t0)), U32(Mix(ct, cb, t0)), U32(Mix(ct, cb, 1.f - t0)), U32(Mix(ct, cb, 1.f - t0)));
    }

    static void Glow(ImDrawList* dl, ImVec2 a, ImVec2 b, const ImVec4& c, float r, float k)
    {
        for (int i = 1; i <= 6; ++i)
            dl->AddRect(ImVec2(a.x - i * 1.6f, a.y - i * 1.6f), ImVec2(b.x + i * 1.6f, b.y + i * 1.6f),
                        U32(A(c, k * 0.075f * (7 - i) / 6.f)), r + i * 1.6f, 0, 2.f);
    }

    static void DrawIcon(ImDrawList* dl, Icon ic, ImVec2 c, float r, ImU32 col)
    {
        const float th = 1.8f;
        auto P = [&](float x, float y) { return ImVec2(c.x + x * r, c.y + y * r); };
        switch (ic) {
        case Icon::Crosshair:
            dl->AddCircle(c, r * 0.62f, col, 24, th);
            dl->AddLine(P(0, -1), P(0, -0.35f), col, th); dl->AddLine(P(0, 1), P(0, 0.35f), col, th);
            dl->AddLine(P(-1, 0), P(-0.35f, 0), col, th); dl->AddLine(P(1, 0), P(0.35f, 0), col, th);
            break;
        case Icon::Cursor: {
            ImVec2 pts[7] = { P(-0.55f,-0.95f), P(-0.55f,0.75f), P(-0.18f,0.38f), P(0.12f,1.0f), P(0.42f,0.86f), P(0.14f,0.28f), P(0.62f,0.22f) };
            dl->AddPolyline(pts, 7, col, ImDrawFlags_Closed, th);
        } break;
        case Icon::Eye:
            dl->AddBezierQuadratic(P(-1, 0), P(0, -1.35f), P(1, 0), col, th);
            dl->AddBezierQuadratic(P(-1, 0), P(0, 1.35f), P(1, 0), col, th);
            dl->AddCircleFilled(c, r * 0.36f, col, 16);
            break;
        case Icon::Cog:
            dl->AddCircle(c, r * 0.5f, col, 20, th);
            for (int i = 0; i < 8; ++i) {
                float a = i * IM_PI / 4.f;
                dl->AddLine(P(cosf(a) * 0.72f, sinf(a) * 0.72f), P(cosf(a) * 1.02f, sinf(a) * 1.02f), col, th + 0.8f);
            }
            dl->AddCircle(c, r * 0.76f, col, 24, th * 0.7f);
            break;
        case Icon::Diamond:
            dl->AddQuad(P(0, -1), P(0.85f, 0), P(0, 1), P(-0.85f, 0), col, th);
            dl->AddLine(P(-0.85f, 0), P(0.85f, 0), col, th * 0.7f);
            break;
        case Icon::Folder:
            dl->AddRect(P(-1, -0.55f), P(1, 0.8f), col, 3.f, 0, th);
            dl->AddLine(P(-1, -0.55f), P(-0.55f, -0.95f), col, th);
            dl->AddLine(P(-0.55f, -0.95f), P(-0.05f, -0.95f), col, th);
            dl->AddLine(P(-0.05f, -0.95f), P(0.2f, -0.55f), col, th);
            break;
        }
    }

    // ------------------------------------------------------------------ setup
    static void ApplyStyle()
    {
        ImGuiStyle& s = ImGui::GetStyle();
        const Theme& t = g_cur;
        s.PopupRounding = 10.f; s.PopupBorderSize = 1.f; s.ChildBorderSize = 0.f; s.WindowBorderSize = 0.f;
        s.FrameRounding = 6.f; s.GrabRounding = 6.f; s.ScrollbarSize = 5.f; s.ScrollbarRounding = 5.f;
        s.ItemSpacing = ImVec2(10, 6);

        ImVec4* c = s.Colors;
        c[ImGuiCol_Text]            = t.text;
        c[ImGuiCol_TextDisabled]    = t.text_dim;
        c[ImGuiCol_WindowBg]        = t.bg_top;
        c[ImGuiCol_PopupBg]         = Mix(t.panel_top, t.bg_top, 0.35f);
        c[ImGuiCol_Border]          = A(Mix(t.border, t.acc1, 0.25f), 1.f);
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
        float k = 1.f - expf(-g_dt * 7.f);
        for (int i = 0; i < 10; ++i) c[i] = Mix(c[i], t[i], k);
    }

    void Init()
    {
        ImGuiIO& io = ImGui::GetIO();
        static const ImWchar ranges[] = { 0x20, 0x17F, 0 };
        ImFontConfig cfg;
        cfg.OversampleH = 3; cfg.OversampleV = 2; cfg.GlyphRanges = ranges;
        g_fonts.body  = io.Fonts->AddFontFromMemoryCompressedTTF(vx_poppins_medium_compressed_data,   vx_poppins_medium_compressed_size,   22.f, &cfg);
        g_fonts.bold  = io.Fonts->AddFontFromMemoryCompressedTTF(vx_poppins_semibold_compressed_data, vx_poppins_semibold_compressed_size, 22.f, &cfg);
        g_fonts.title = io.Fonts->AddFontFromMemoryCompressedTTF(vx_poppins_semibold_compressed_data, vx_poppins_semibold_compressed_size, 38.f, &cfg);
        g_fonts.small = io.Fonts->AddFontFromMemoryCompressedTTF(vx_poppins_medium_compressed_data,   vx_poppins_medium_compressed_size,   18.f, &cfg);
        io.FontDefault = g_fonts.body;
        g_cur = g_tgt = kPresets[0];
        ApplyStyle();
    }

    // ----------------------------------------------------------------- window
    static ImVec4 OnAccent()
    {
        ImVec4 c = Grad(0.5f);
        float l = 0.299f * c.x + 0.587f * c.y + 0.114f * c.z;
        return l > 0.55f ? ImVec4(0.04f, 0.05f, 0.06f, 1) : ImVec4(1, 1, 1, 1);
    }

    bool BeginWindow(const char* title, ImVec2 size, bool* open)
    {
        ImGuiIO& io = ImGui::GetIO();
        g_dt = io.DeltaTime > 0.05f ? 0.05f : io.DeltaTime;
        g_time += g_dt;
        ThemeStep();
        ApplyStyle();
        g_alpha = Clamp01(g_alpha + g_dt * 3.2f);
        g_page  = Clamp01(g_page + g_dt * 3.6f);
        g_open  = open;

        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, EaseOutCubic(g_alpha));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
        bool vis = ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
        if (!vis) return false;
        g_base_alpha = ImGui::GetStyle().Alpha;

        const Theme& t = g_cur;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos(), e(p.x + size.x, p.y + size.y);
        g_wpos = p; g_wsize = size;

        Glow(dl, p, e, t.acc1, 10.f, 0.22f);
        GradV(dl, p, e, t.bg_top, t.bg_bot, 10.f);

        // sidebar (same shape, clipped)
        ImVec4 black(0, 0, 0, 1);
        dl->PushClipRect(p, ImVec2(p.x + kSide, e.y), true);
        GradV(dl, p, e, Mix(t.bg_bot, black, 0.30f), Mix(t.bg_bot, black, 0.55f), 10.f);
        dl->PopClipRect();
        dl->AddLine(ImVec2(p.x + kSide, p.y + 1), ImVec2(p.x + kSide, e.y - 1), U32(t.border), 1.f);

        // content backdrop: glow + wave-lit dot grid
        dl->PushClipRect(ImVec2(p.x + kSide + 1, p.y + 1), ImVec2(e.x - 1, e.y - 1), true);
        for (int i = 0; i < 40; ++i) {
            float rr = 40.f + i * 9.f;
            dl->AddCircleFilled(ImVec2(e.x - 90, p.y + 10), rr, U32(A(t.acc2, 0.0042f)), 48);
            dl->AddCircleFilled(ImVec2(p.x + kSide + 60, e.y - 10), rr, U32(A(t.acc1, 0.0034f)), 48);
        }
        for (float y = p.y + 10.f; y < e.y - 4.f; y += 24.f) {
            for (float x = p.x + kSide + 14.f; x < e.x - 4.f; x += 24.f) {
                float w = sinf((x - p.x) * 0.011f + (y - p.y) * 0.007f - g_time * 0.9f);
                w = w > 0.f ? w * w * w * w * w * w : 0.f;
                float u = (x - p.x - kSide) / (size.x - kSide);
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 2.f, y + 2.f), U32(A(Grad(u), 0.04f + 0.16f * w)));
            }
        }
        dl->PopClipRect();

        // logo
        {
            ImVec2 lp(p.x + 22, p.y + 22);
            GradH(dl, lp, ImVec2(lp.x + 40, lp.y + 40), t.acc1, t.acc2, 11.f);
            ImU32 dk = U32(OnAccent());
            dl->AddLine(ImVec2(lp.x + 11, lp.y + 12), ImVec2(lp.x + 20, lp.y + 29), dk, 3.2f);
            dl->AddLine(ImVec2(lp.x + 20, lp.y + 29), ImVec2(lp.x + 29, lp.y + 12), dk, 3.2f);
            ImFont* f = g_fonts.title;
            GradText(dl, f, ImVec2(lp.x + 52, lp.y + 20 - f->FontSize * 0.5f - 1), "velox", g_time * 1.2f, 1.f);
            Txt(dl, g_fonts.small, ImVec2(p.x + 26, p.y + 82), U32(A(t.text_dim, 0.75f)), "NAVIGATION");
        }

        // user card
        {
            ImFont* fb = g_fonts.bold; ImFont* fs = g_fonts.small;
            ImVec2 a(p.x + 14, e.y - kBottom - 62), b(p.x + kSide - 14, e.y - kBottom - 6);
            GradH(dl, a, b, A(t.frame, 0.8f), A(t.frame, 0.5f), 12.f);
            dl->AddRect(a, b, U32(t.border), 12.f);
            ImVec2 c(a.x + 30, (a.y + b.y) * 0.5f);
            GradH(dl, ImVec2(c.x - 18, c.y - 18), ImVec2(c.x + 18, c.y + 18), t.acc1, t.acc2, 10.f);
            Txt(dl, fb, ImVec2(c.x - TxtSize(fb, "V").x * 0.5f, c.y - fb->FontSize * 0.5f), U32(OnAccent()), "V");
            dl->AddCircleFilled(ImVec2(c.x + 15, c.y + 15), 5.f, U32(ImVec4(0.2f, 0.9f, 0.5f, 1)), 12);
            dl->AddCircle(ImVec2(c.x + 15, c.y + 15), 5.f, U32(t.bg_bot), 12, 2.f);
            Txt(dl, fb, ImVec2(c.x + 28, c.y - fb->FontSize * 0.5f - 8), U32(t.text), "velox");
            Txt(dl, fs, ImVec2(c.x + 28, c.y - fs->FontSize * 0.5f + 12), U32(t.text_dim), "lifetime");
        }

        // close
        {
            ImVec2 cc(e.x - 38, p.y + kHead * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(cc.x - 15, cc.y - 15));
            ImGui::InvisibleButton("##vx_close", ImVec2(30, 30));
            float h = Anim(ImGui::GetID("##vx_close"), ImGui::IsItemHovered() ? 1.f : 0.f, 1);
            GradH(dl, ImVec2(cc.x - 15, cc.y - 15), ImVec2(cc.x + 15, cc.y + 15), A(t.acc1, 0.22f * h), A(t.acc2, 0.22f * h), 8.f);
            ImU32 xc = U32(Mix(t.text_dim, t.text, h));
            dl->AddLine(ImVec2(cc.x - 5, cc.y - 5), ImVec2(cc.x + 5, cc.y + 5), xc, 2.f);
            dl->AddLine(ImVec2(cc.x + 5, cc.y - 5), ImVec2(cc.x - 5, cc.y + 5), xc, 2.f);
            if (ImGui::IsItemClicked() && g_open) *g_open = false;
        }

        // status
        {
            ImFont* fs = g_fonts.small;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f fps", io.Framerate);
            Txt(dl, fs, ImVec2(p.x + kSide + kPad, e.y - kBottom + 8), U32(A(t.text_dim, 0.8f)), "velox  -  build 1.0.0");
            Txt(dl, fs, ImVec2(e.x - kPad - TxtSize(fs, buf).x, e.y - kBottom + 8), U32(A(t.text_dim, 0.8f)), buf);
        }

        // thin top edge + border
        dl->AddRectFilledMultiColor(ImVec2(p.x + 10, p.y), ImVec2(e.x - 10, p.y + 1.5f), U32(A(t.acc1, 0.0f)), U32(A(t.acc1, 0.9f)), U32(A(t.acc2, 0.9f)), U32(A(t.acc2, 0.0f)));
        dl->AddRect(p, e, U32(t.border), 10.f);

        // drag handles
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##vx_drag_a", ImVec2(kSide, 80));
        bool drag = ImGui::IsItemActive();
        ImGui::SetCursorPos(ImVec2(kSide, 0));
        ImGui::InvisibleButton("##vx_drag_b", ImVec2(size.x - kSide - 80, kHead));
        drag = drag || ImGui::IsItemActive();
        if (drag) ImGui::SetWindowPos(ImVec2(p.x + io.MouseDelta.x, p.y + io.MouseDelta.y));

        g_tabY = 112.f;
        return true;
    }

    void EndWindow()
    {
        ImGui::End();
        ImGui::PopStyleVar(3);
    }

    bool Tab(const char* label, Icon icon, int idx, int* current)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.bold;
        float w = kSide - 28.f, h = 46.f;

        ImGui::SetCursorPos(ImVec2(14.f, g_tabY));
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiID id = ImGui::GetID(label);
        ImGui::InvisibleButton(label, ImVec2(w, h));
        bool clicked = ImGui::IsItemClicked();
        if (clicked && *current != idx) { *current = idx; g_page = 0.f; }

        bool sel = (*current == idx);
        float a  = Tween(id, sel ? 1.f : 0.f, 1, 0.3f);
        float hv = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 2);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (sel) {
            ImVec2 wp = ImGui::GetWindowPos();
            float ty = p.y - wp.y;
            if (!g_ind_init) { g_ind_y = ty; g_ind_init = true; }
            g_ind_y += (ty - g_ind_y) * (1.f - expf(-g_dt * 16.f));
            ImVec2 a0(p.x, wp.y + g_ind_y), a1(p.x + w, wp.y + g_ind_y + h);
            Glow(dl, a0, a1, t.acc1, 12.f, 0.25f);
            GradH(dl, a0, a1, A(t.acc1, 0.17f), A(t.acc2, 0.08f), 12.f);
            dl->AddRect(a0, a1, U32(A(Grad(0.f), 0.55f)), 12.f, 0, 1.5f);
        } else if (hv > 0.01f) {
            GradH(dl, p, ImVec2(p.x + w, p.y + h), A(t.text, 0.05f * hv), A(t.text, 0.0f), 12.f);
        }

        ImVec4 base = Mix(Mix(t.text_dim, t.text, hv * 0.8f), t.text, a);
        DrawIcon(dl, icon, ImVec2(p.x + 27, p.y + h * 0.5f), 9.f, U32(Mix(base, Grad(0.f), a)));
        Txt(dl, f, ImVec2(p.x + 52 + 3.f * a, p.y + h * 0.5f - f->FontSize * 0.5f), U32(base), label);
        if (sel) snprintf(g_title, sizeof(g_title), "%s", label);
        g_tabY += h + 4.f;
        return clicked;
    }

    void BeginContent()
    {
        const Theme& t = g_cur;
        float a = EaseOutCubic(g_page);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        ImFont* f = g_fonts.title;
        float tx = p.x + kSide + kPad + (1.f - a) * 14.f;
        Txt(dl, f, ImVec2(tx, p.y + 22), U32(A(t.text, a)), g_title);
        Txt(dl, g_fonts.small, ImVec2(tx + TxtSize(f, g_title).x + 12.f, p.y + 22 + f->FontSize - 24.f), U32(A(t.text_dim, a * 0.8f)), "module");
        GradH(dl, ImVec2(tx, p.y + kHead - 8.f), ImVec2(tx + 36.f, p.y + kHead - 6.f), A(t.acc1, a), A(t.acc2, a), 1.f);

        ImGui::SetCursorPos(ImVec2(kSide + kPad, kHead + 10.f + (1.f - a) * 16.f));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_base_alpha * a);
        ImGui::BeginChild("##vx_content", ImVec2(g_wsize.x - kSide - kPad * 2, g_wsize.y - kHead - kBottom - 14.f), 0,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    void EndContent()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

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
        ImGui::InvisibleButton(label, ImVec2(w, 36));
        bool clicked = ImGui::IsItemClicked();
        if (clicked) *current = idx;
        float a = Tween(id, *current == idx ? 1.f : 0.f, 1, 0.25f);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 2);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (a > 0.01f) {
            GradH(dl, p, ImVec2(p.x + w, p.y + 36), A(t.acc1, 0.20f * a), A(t.acc1, 0.0f), 8.f);
            GradV(dl, ImVec2(p.x, p.y + 8), ImVec2(p.x + 3, p.y + 28), A(t.acc1, a), A(t.acc2, a), 1.5f);
        }
        ImVec4 col = Mix(Mix(t.text_dim, t.text, h * 0.8f), t.text, a);
        Txt(dl, f, ImVec2(p.x + 14 + 5 * a, p.y + 18 - f->FontSize * 0.5f), U32(col), label);
        return clicked;
    }

    void EndSubTabs()
    {
        ImGui::EndChild();
        ImGui::SameLine(0, 16);
    }

    // ------------------------------------------------------------------ group
    bool BeginGroup(const char* title, ImVec2 size)
    {
        const Theme& t = g_cur;
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        bool vis = ImGui::BeginChild(title, size, ImGuiChildFlags_AlwaysUseWindowPadding,
                                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize(), e(p.x + sz.x, p.y + sz.y);
        dl->AddRectFilled(p, e, U32(A(t.panel_top, 0.90f)), 8.f);
        dl->AddRectFilled(p, ImVec2(e.x, p.y + 46), U32(ImVec4(1, 1, 1, 0.025f)), 8.f, ImDrawFlags_RoundCornersTop);
        dl->AddRect(p, e, U32(t.border), 8.f);
        dl->AddLine(ImVec2(p.x + 1, p.y + 46), ImVec2(e.x - 1, p.y + 46), U32(t.border), 1.f);

        ImFont* f = g_fonts.bold;
        GradV(dl, ImVec2(p.x + 14, p.y + 15), ImVec2(p.x + 17.5f, p.y + 31), t.acc1, t.acc2, 1.5f);
        Txt(dl, f, ImVec2(p.x + 27, p.y + 23 - f->FontSize * 0.5f), U32(t.text), title);
        for (int i = 0; i < 3; ++i)
            dl->AddCircleFilled(ImVec2(e.x - 22 - i * 9, p.y + 23), 2.f, U32(A(Grad(i / 2.f), 0.55f)), 8);

        ImGui::SetCursorPos(ImVec2(20, 58));
        return vis;
    }

    void EndGroup() { ImGui::EndChild(); }

    void Spacing(float h) { ImGui::Dummy(ImVec2(1, h)); }

    void Separator()
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(ImVec2(p.x, p.y + 3), ImVec2(p.x + w, p.y + 4),
            U32(A(g_cur.acc1, 0.5f)), U32(A(g_cur.acc2, 0.0f)), U32(A(g_cur.acc2, 0.0f)), U32(A(g_cur.acc1, 0.5f)));
        ImGui::Dummy(ImVec2(w, 8));
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

        float a = Tween(id, *v ? 1.f : 0.f, 1, 0.26f);
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 2);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImVec2 b0(p.x, p.y + (kRow - 24.f) * 0.5f), b1(b0.x + 24.f, b0.y + 24.f);
        GradH(dl, b0, b1, t.frame, t.frame, 7.f);
        dl->AddRect(b0, b1, U32(Mix(t.border, Grad(0.f), h * 0.8f)), 7.f);
        if (a > 0.01f) {
            Glow(dl, b0, b1, t.acc1, 7.f, a * 0.55f);
            GradH(dl, b0, b1, A(t.acc1, a), A(t.acc2, a), 7.f);
            float s = a * 17.f;
            ImVec2 c0(b0.x + 6.f, b0.y + 12.5f), c1(b0.x + 10.5f, b0.y + 17.f), c2(b0.x + 18.f, b0.y + 7.f);
            ImU32 ck = U32(A(OnAccent(), a));
            float f1 = (s < 6.4f ? s : 6.4f) / 6.4f;
            dl->AddLine(c0, ImVec2(c0.x + (c1.x - c0.x) * f1, c0.y + (c1.y - c0.y) * f1), ck, 2.6f);
            if (s > 6.4f) {
                float f2 = Clamp01((s - 6.4f) / 10.6f);
                dl->AddLine(c1, ImVec2(c1.x + (c2.x - c1.x) * f2, c1.y + (c2.y - c1.y) * f2), ck, 2.6f);
            }
        }
        Txt(dl, f, ImVec2(p.x + 38.f, p.y + kRow * 0.5f - f->FontSize * 0.5f), U32(LabelCol(a > h ? a : h)), label);
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
        float f01 = Anim(id, Clamp01((*v - lo) / (hi - lo)), 1, 18.f);
        float h   = Anim(id, (ImGui::IsItemHovered() || ImGui::IsItemActive()) ? 1.f : 0.f, 2);
        float act = Anim(id, ImGui::IsItemActive() ? 1.f : 0.f, 3, 20.f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        Txt(dl, f, ImVec2(p.x, p.y + 4), U32(LabelCol(h)), label);
        ImFont* fb = g_fonts.bold;
        Txt(dl, fb, ImVec2(p.x + w - TxtSize(fb, val).x, p.y + 4), U32(Mix(t.text, Grad(f01), 0.7f + 0.3f * act)), val);

        for (int i = 0; i <= 10; ++i) {
            float x = bx0 + (bx1 - bx0) * i / 10.f;
            dl->AddLine(ImVec2(x, by + 8), ImVec2(x, by + (i % 5 == 0 ? 13.f : 11.f)), U32(A(t.text_dim, i / 10.f <= f01 ? 0.55f : 0.22f)), 1.f);
        }
        GradH(dl, ImVec2(bx0, by - 2.5f), ImVec2(bx1, by + 2.5f), t.frame, t.frame, 2.5f);
        float fx = bx0 + (bx1 - bx0) * f01;
        if (fx - bx0 > 1.f) {
            Glow(dl, ImVec2(bx0, by - 2.5f), ImVec2(fx, by + 2.5f), t.acc1, 2.5f, 0.35f + 0.5f * h);
            GradH(dl, ImVec2(bx0, by - 2.5f), ImVec2(fx, by + 2.5f), t.acc1, Grad(f01), 2.5f);
        }
        float s = 9.f * (1.f + 0.12f * h + 0.22f * act);
        ImVec2 q[4] = { ImVec2(fx, by - s), ImVec2(fx + s, by), ImVec2(fx, by + s), ImVec2(fx - s, by) };
        dl->AddCircle(ImVec2(fx, by), s + 7.f * act, U32(A(Grad(f01), 0.28f * act)), 4, 2.f);
        dl->AddConvexPolyFilled(q, 4, U32(t.text));
        dl->AddPolyline(q, 4, U32(Grad(f01)), ImDrawFlags_Closed, 2.f);
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
        GradH(dl, a, b, Mix(t.frame, t.acc1, 0.10f * hover + 0.08f * open), Mix(t.frame, t.acc2, 0.10f * hover + 0.08f * open), 8.f);
        dl->AddRect(a, b, U32(Mix(t.border, t.acc1, open * 0.9f)), 8.f);
    }

    template <class Fn>
    static bool DropdownPopup(const char* label, ImVec2 b0, float bw, int count, Fn rows)
    {
        const Theme& t = g_cur;
        ImGuiID pid = ImGui::GetID(label);
        bool opened = ImGui::IsPopupOpen(label);
        float o = EaseOutCubic(Tween(pid, opened ? 1.f : 0.f, 9, 0.18f));
        ImGui::SetNextWindowPos(ImVec2(b0.x, b0.y + 4.f - (1.f - o) * 8.f));
        ImGui::SetNextWindowSize(ImVec2(bw, count * 32.f + 10.f));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_base_alpha * o);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.f);
        bool changed = false;
        if (ImGui::BeginPopup(label, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) {
            Glow(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), ImVec2(ImGui::GetWindowPos().x + bw, ImGui::GetWindowPos().y + count * 32.f + 10.f), t.acc1, 10.f, 0.3f);
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
        ImGui::InvisibleButton(text, ImVec2(w, 32));
        bool clicked = ImGui::IsItemClicked();
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float s = Tween(id, selected ? 1.f : 0.f, 2, 0.2f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        GradH(dl, p, ImVec2(p.x + w, p.y + 32), A(t.acc1, 0.14f * h + 0.08f * s), A(t.acc2, 0.14f * h + 0.08f * s), 7.f);
        if (s > 0.01f) GradV(dl, ImVec2(p.x + 2, p.y + 8), ImVec2(p.x + 5, p.y + 24), A(t.acc1, s), A(t.acc2, s), 1.5f);
        ImVec4 col = Mix(Mix(t.text_dim, t.text, 0.5f + 0.5f * h), Grad(0.f), s);
        Txt(dl, f, ImVec2(p.x + 14 + 3 * h, p.y + 16 - f->FontSize * 0.5f), U32(col), text);
        if (checkStyle && s > 0.01f) {
            ImVec2 c(p.x + w - 16, p.y + 16);
            dl->AddLine(ImVec2(c.x - 4, c.y), ImVec2(c.x - 1, c.y + 3), U32(A(Grad(0.f), s)), 2.f);
            dl->AddLine(ImVec2(c.x - 1, c.y + 3), ImVec2(c.x + 5, c.y - 4), U32(A(Grad(0.f), s)), 2.f);
        }
        return clicked;
    }

    static void DrawChevron(ImDrawList* dl, ImVec2 c, float open, ImU32 col)
    {
        float d = 3.5f * (1.f - 2.f * open);
        dl->AddLine(ImVec2(c.x - 4.5f, c.y - d * 0.5f), ImVec2(c.x, c.y + d * 0.5f), col, 2.f);
        dl->AddLine(ImVec2(c.x, c.y + d * 0.5f), ImVec2(c.x + 4.5f, c.y - d * 0.5f), col, 2.f);
    }

    bool Combo(const char* label, int* cur, const char* const* items, int count)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowW(), bw = 168.f;
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
        DrawChevron(dl, ImVec2(b1.x - 16, (b0.y + b1.y) * 0.5f), o, U32(Mix(t.text_dim, Grad(0.f), o)));

        return DropdownPopup(label, ImVec2(b0.x, b1.y), bw, count, [&]() {
            bool ch = false;
            for (int i = 0; i < count; ++i)
                if (DropRow(items[i], i == *cur, bw - 10, false)) { *cur = i; ch = true; ImGui::CloseCurrentPopup(); }
            return ch;
        });
    }

    bool MultiCombo(const char* label, bool* states, const char* const* items, int count)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.body;
        ImGuiID id = ImGui::GetID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = RowW(), bw = 168.f;
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
        DrawChevron(dl, ImVec2(b1.x - 16, (b0.y + b1.y) * 0.5f), o, U32(Mix(t.text_dim, Grad(0.f), o)));

        return DropdownPopup(label, ImVec2(b0.x, b1.y), bw, count, [&]() {
            bool ch = false;
            for (int i = 0; i < count; ++i)
                if (DropRow(items[i], states[i], bw - 10, true)) { states[i] = !states[i]; ch = true; }
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
        if (size.y <= 0.f) size.y = 40.f;
        ImGui::InvisibleButton(label, size);
        bool clicked = ImGui::IsItemClicked();
        ImGuiStorage* st = ImGui::GetStateStorage();
        if (clicked) {
            ImVec2 m = ImGui::GetIO().MousePos;
            st->SetFloat(id + 5, g_time); st->SetFloat(id + 6, m.x - p.x); st->SetFloat(id + 7, m.y - p.y);
        }
        float h = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 1);
        float d = Anim(id, ImGui::IsItemActive() ? 1.f : 0.f, 2, 24.f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 e(p.x + size.x, p.y + size.y);
        GradH(dl, p, e, t.frame, t.frame, 7.f);
        if (h > 0.01f) Glow(dl, p, e, t.acc1, 7.f, h * 0.7f);
        GradH(dl, p, e, A(t.acc1, 0.06f + 0.94f * h - 0.12f * d), A(t.acc2, 0.06f + 0.94f * h - 0.12f * d), 7.f);
        dl->AddRect(p, e, U32(A(Grad(0.5f), 0.75f)), 7.f, 0, 1.5f);

        float rt = (g_time - st->GetFloat(id + 5, -10.f)) / 0.65f;
        if (rt >= 0.f && rt < 1.f) {
            dl->PushClipRect(p, e, true);
            ImVec2 c(p.x + st->GetFloat(id + 6, 0.f), p.y + st->GetFloat(id + 7, 0.f));
            dl->AddCircleFilled(c, EaseOutCubic(rt) * size.x * 0.9f, U32(ImVec4(1, 1, 1, 0.26f * (1.f - rt))), 48);
            dl->PopClipRect();
        }
        ImVec2 ts = TxtSize(f, label);
        Txt(dl, f, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f + d), U32(Mix(t.text, OnAccent(), h)), label);
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
        ImVec2 s0(p.x + w - 52, p.y + (kRow - 24) * 0.5f), s1(s0.x + 52, s0.y + 24);
        ImVec4 c(col[0], col[1], col[2], 1.f);
        Glow(dl, s0, s1, c, 8.f, 0.4f * h);
        dl->AddRectFilled(s0, s1, ImGui::ColorConvertFloat4ToU32(c), 8.f);
        dl->AddRect(s0, s1, U32(Mix(t.border, ImVec4(1, 1, 1, 0.6f), h)), 8.f);

        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(s1.x - 230, s1.y + 6));
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
        float w = RowW(), bw = 100.f;
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
        Txt(dl, f, ImVec2(b0.x + (bw - ts.x) * 0.5f, (b0.y + b1.y) * 0.5f - f->FontSize * 0.5f), U32(waiting ? Grad(0.f) : t.text), buf);
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
        float y = io.DisplaySize.y - 24.f;
        for (size_t i = 0; i < g_toasts.size();) {
            Toast& n = g_toasts[i];
            n.age += dt;
            if (n.age >= n.life + 0.4f) { g_toasts.erase(g_toasts.begin() + i); continue; }

            float in = EaseOutBack(Clamp01(n.age / 0.45f));
            float out = n.age > n.life ? EaseOutCubic((n.age - n.life) / 0.4f) : 0.f;
            float W = 340.f, H = 62.f;
            float x = io.DisplaySize.x - 24.f - W + (1.f - in) * (W + 40.f) + out * (W + 40.f);
            y -= H;
            ImVec2 a(x, y), b(x + W, y + H);
            ImVec4 kc = n.kind == 1 ? ImVec4(0.20f, 0.92f, 0.55f, 1) : (n.kind == 2 ? ImVec4(1.f, 0.72f, 0.20f, 1) : t.acc1);

            Glow(dl, a, b, kc, 12.f, 0.5f);
            GradV(dl, a, b, A(t.panel_top, 0.97f), A(t.panel_bot, 0.97f), 12.f);
            dl->AddRect(a, b, U32(A(kc, 0.35f)), 12.f);
            ImVec2 c(a.x + 32, a.y + H * 0.5f - 1);
            dl->AddCircleFilled(c, 14.f, U32(A(kc, 0.18f)), 24);
            dl->AddCircle(c, 14.f, U32(A(kc, 0.8f)), 24, 1.8f);
            if (n.kind == 1) {
                dl->AddLine(ImVec2(c.x - 5, c.y), ImVec2(c.x - 1, c.y + 4), U32(kc), 2.2f);
                dl->AddLine(ImVec2(c.x - 1, c.y + 4), ImVec2(c.x + 6, c.y - 4), U32(kc), 2.2f);
            } else {
                dl->AddLine(ImVec2(c.x, c.y - 6), ImVec2(c.x, c.y + 1), U32(kc), 2.2f);
                dl->AddCircleFilled(ImVec2(c.x, c.y + 6), 1.8f, U32(kc), 8);
            }
            Txt(dl, f, ImVec2(a.x + 58, a.y + H * 0.5f - f->FontSize * 0.5f - 2), U32(t.text), n.text.c_str());
            float k = Clamp01(1.f - n.age / n.life);
            GradH(dl, ImVec2(a.x + 14, b.y - 6), ImVec2(a.x + 14 + (W - 28) * k, b.y - 3), t.acc1, t.acc2, 1.5f);
            y -= 10.f;
            ++i;
        }
    }
}
