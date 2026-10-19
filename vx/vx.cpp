#include "vx.h"
#include "imgui_internal.h"
#include "vx_fonts.h"
#include "vx_icons.h"
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace vx
{
    // ------------------------------------------------------------------ state
    static const float kHead = 70.f, kBottom = 32.f, kPad = 22.f, kRow = 34.f;

    static const Theme kPresets[] = {
        // blue
        { {0.066f,0.074f,0.094f,1}, {0.045f,0.052f,0.068f,1}, {0.090f,0.102f,0.130f,1}, {0.075f,0.086f,0.112f,1},
          {0.110f,0.125f,0.158f,1}, {1,1,1,0.09f}, {0.90f,0.93f,0.98f,1}, {0.48f,0.54f,0.65f,1},
          {0.19f,0.52f,1.00f,1}, {0.32f,0.62f,1.00f,1}, {0.62f,0.45f,1.00f,1} },
        // red
        { {0.075f,0.068f,0.070f,1}, {0.050f,0.045f,0.047f,1}, {0.100f,0.090f,0.092f,1}, {0.085f,0.076f,0.078f,1},
          {0.125f,0.112f,0.115f,1}, {1,1,1,0.09f}, {0.95f,0.92f,0.92f,1}, {0.58f,0.52f,0.53f,1},
          {0.92f,0.17f,0.20f,1}, {1.00f,0.32f,0.33f,1}, {1.00f,0.62f,0.22f,1} },
        // lime
        { {0.068f,0.072f,0.078f,1}, {0.046f,0.050f,0.055f,1}, {0.092f,0.098f,0.108f,1}, {0.078f,0.083f,0.092f,1},
          {0.118f,0.126f,0.140f,1}, {1,1,1,0.09f}, {0.93f,0.95f,0.95f,1}, {0.52f,0.56f,0.58f,1},
          {0.62f,0.95f,0.22f,1}, {0.72f,1.00f,0.36f,1}, {0.20f,0.90f,0.85f,1} },
        // violet
        { {0.074f,0.066f,0.094f,1}, {0.050f,0.044f,0.068f,1}, {0.100f,0.090f,0.130f,1}, {0.085f,0.076f,0.112f,1},
          {0.130f,0.116f,0.170f,1}, {1,1,1,0.09f}, {0.94f,0.92f,0.98f,1}, {0.56f,0.52f,0.66f,1},
          {0.62f,0.38f,1.00f,1}, {0.72f,0.50f,1.00f,1}, {1.00f,0.42f,0.72f,1} },
    };
    static const char* kPresetNames[] = { "Blue", "Red", "Lime", "Violet" };
    static_assert(sizeof(Theme) == 11 * sizeof(ImVec4), "theme layout");

    static Theme  g_cur = kPresets[0], g_tgt = kPresets[0];
    static Fonts  g_fonts = {};
    static float  g_time = 0.f, g_dt = 0.016f;
    static float  g_base_alpha = 1.f, g_alpha = 0.f;
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


    // -------------------------------------------------------------- svg icons
    // tiny svg reader: <circle>, <line>, <path d> (M L H V C S Q A Z, abs + rel) -> polylines in a 24x24 box
    struct Poly { std::vector<ImVec2> pts; bool closed = false; };
    struct IconData { std::vector<Poly> polys; float total = 0.f; };
    static IconData g_icons[sizeof(kIconSvg) / sizeof(kIconSvg[0])];
    static bool     g_icons_ready = false;

    static void SvgArc(Poly& poly, ImVec2 p0, float rx, float ry, float phiDeg, int fa, int fs, ImVec2 p1)
    {
        if (rx == 0.f || ry == 0.f) { poly.pts.push_back(p1); return; }
        rx = fabsf(rx); ry = fabsf(ry);
        float phi = phiDeg * IM_PI / 180.f, c = cosf(phi), sn = sinf(phi);
        float dx = (p0.x - p1.x) * 0.5f, dy = (p0.y - p1.y) * 0.5f;
        float x1p = c * dx + sn * dy, y1p = -sn * dx + c * dy;
        float lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
        if (lam > 1.f) { float sl = sqrtf(lam); rx *= sl; ry *= sl; }
        float num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
        float den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
        float co = sqrtf(fmaxf(0.f, num / den));
        if (fa == fs) co = -co;
        float cxp = co * rx * y1p / ry, cyp = -co * ry * x1p / rx;
        float cx = c * cxp - sn * cyp + (p0.x + p1.x) * 0.5f, cy = sn * cxp + c * cyp + (p0.y + p1.y) * 0.5f;
        float ux = (x1p - cxp) / rx, uy = (y1p - cyp) / ry, vx = (-x1p - cxp) / rx, vy = (-y1p - cyp) / ry;
        float th1 = atan2f(uy, ux);
        float dth = atan2f(ux * vy - uy * vx, ux * vx + uy * vy);
        if (!fs && dth > 0.f) dth -= 2.f * IM_PI; else if (fs && dth < 0.f) dth += 2.f * IM_PI;
        int n = (int)ceilf(fabsf(dth) / (IM_PI / 14.f));
        if (n < 3) n = 3;
        for (int i = 1; i <= n; ++i) {
            float th = th1 + dth * i / n;
            poly.pts.push_back(ImVec2(cx + c * rx * cosf(th) - sn * ry * sinf(th), cy + sn * rx * cosf(th) + c * ry * sinf(th)));
        }
    }

    static void SvgPath(const char* d, std::vector<Poly>& out)
    {
        const char* s = d;
        ImVec2 cur(0, 0), start(0, 0), lastC(0, 0);
        char cmd = 0;
        Poly poly;
        auto skip = [&]() { while (*s == ' ' || *s == ',' || *s == '\n' || *s == '\t' || *s == '\r') ++s; };
        auto num  = [&]() -> float { skip(); char* e; float v = strtof(s, &e); s = e; return v; };
        auto pt   = [&](ImVec2 base) -> ImVec2 { float x = num(); float y = num(); return ImVec2(base.x + x, base.y + y); };
        auto flag = [&]() -> int { skip(); int v = (*s == '1'); if (*s == '0' || *s == '1') ++s; return v; };
        auto flush = [&](bool closed) { if (poly.pts.size() > 1) { poly.closed = closed; out.push_back(poly); } poly.pts.clear(); poly.closed = false; };
        while (true) {
            skip();
            if (!*s) break;
            if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) cmd = *s++;
            else if (!cmd) break;
            bool rel = cmd >= 'a';
            char C = rel ? (char)(cmd - 32) : cmd;
            ImVec2 o = rel ? cur : ImVec2(0, 0);
            if (C == 'Z') { flush(true); cur = start; poly.pts.push_back(start); cmd = 0; continue; }
            if (C == 'M') {
                flush(false);
                ImVec2 q = pt(o);
                cur = start = q; poly.pts.push_back(q);
                cmd = rel ? 'l' : 'L';
            } else if (C == 'L') {
                ImVec2 q = pt(o); poly.pts.push_back(q); cur = q;
            } else if (C == 'H') {
                ImVec2 q((rel ? cur.x : 0.f) + num(), cur.y); poly.pts.push_back(q); cur = q;
            } else if (C == 'V') {
                ImVec2 q(cur.x, (rel ? cur.y : 0.f) + num()); poly.pts.push_back(q); cur = q;
            } else if (C == 'C' || C == 'S') {
                ImVec2 c1 = (C == 'S') ? ImVec2(2 * cur.x - lastC.x, 2 * cur.y - lastC.y) : pt(o);
                ImVec2 c2 = pt(o);
                ImVec2 e = pt(o);
                for (int i = 1; i <= 14; ++i) {
                    float t = i / 14.f, u = 1.f - t;
                    poly.pts.push_back(ImVec2(u*u*u*cur.x + 3*u*u*t*c1.x + 3*u*t*t*c2.x + t*t*t*e.x,
                                              u*u*u*cur.y + 3*u*u*t*c1.y + 3*u*t*t*c2.y + t*t*t*e.y));
                }
                lastC = c2; cur = e;
            } else if (C == 'Q') {
                ImVec2 c1 = pt(o);
                ImVec2 e = pt(o);
                for (int i = 1; i <= 10; ++i) {
                    float t = i / 10.f, u = 1.f - t;
                    poly.pts.push_back(ImVec2(u*u*cur.x + 2*u*t*c1.x + t*t*e.x, u*u*cur.y + 2*u*t*c1.y + t*t*e.y));
                }
                cur = e;
            } else if (C == 'A') {
                float rx = num(), ry = num(), rot = num();
                int fa = flag(), fs = flag();
                ImVec2 e = pt(o);
                SvgArc(poly, cur, rx, ry, rot, fa, fs, e);
                cur = e;
            } else break;
        }
        flush(false);
    }

    static bool SvgAttr(const std::string& tag, const char* name, std::string& out)
    {
        std::string key = std::string(" ") + name + "=\"";
        size_t i = tag.find(key);
        if (i == std::string::npos) return false;
        i += key.size();
        size_t j = tag.find('"', i);
        if (j == std::string::npos) return false;
        out = tag.substr(i, j - i);
        return true;
    }
    static float SvgNum(const std::string& tag, const char* name) { std::string v; return SvgAttr(tag, name, v) ? (float)atof(v.c_str()) : 0.f; }

    static void BuildIcons()
    {
        for (size_t k = 0; k < sizeof(kIconSvg) / sizeof(kIconSvg[0]); ++k) {
            IconData& ic = g_icons[k];
            std::string src = kIconSvg[k];
            size_t pos = 0;
            while ((pos = src.find('<', pos)) != std::string::npos) {
                size_t end = src.find('>', pos);
                if (end == std::string::npos) break;
                std::string tag = src.substr(pos, end - pos);
                pos = end + 1;
                if (tag.compare(0, 7, "<circle") == 0) {
                    float cx = SvgNum(tag, "cx"), cy = SvgNum(tag, "cy"), r = SvgNum(tag, "r");
                    Poly po; po.closed = true;
                    for (int i = 0; i < 40; ++i) po.pts.push_back(ImVec2(cx + cosf(i * IM_PI / 20.f) * r, cy + sinf(i * IM_PI / 20.f) * r));
                    ic.polys.push_back(po);
                } else if (tag.compare(0, 5, "<line") == 0) {
                    Poly po;
                    po.pts.push_back(ImVec2(SvgNum(tag, "x1"), SvgNum(tag, "y1")));
                    po.pts.push_back(ImVec2(SvgNum(tag, "x2"), SvgNum(tag, "y2")));
                    ic.polys.push_back(po);
                } else if (tag.compare(0, 5, "<path") == 0) {
                    std::string d;
                    if (SvgAttr(tag, "d", d)) SvgPath(d.c_str(), ic.polys);
                }
            }
            for (const Poly& po : ic.polys) {
                size_t n = po.pts.size();
                for (size_t i = 1; i < n; ++i) { float dx = po.pts[i].x - po.pts[i-1].x, dy = po.pts[i].y - po.pts[i-1].y; ic.total += sqrtf(dx*dx + dy*dy); }
                if (po.closed && n > 1) { float dx = po.pts[0].x - po.pts[n-1].x, dy = po.pts[0].y - po.pts[n-1].y; ic.total += sqrtf(dx*dx + dy*dy); }
            }
        }
        g_icons_ready = true;
    }

    // progress < 1 draws the strokes in order, like a pen
    static void DrawSvgIcon(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 col, float progress)
    {
        if (!g_icons_ready) BuildIcons();
        const IconData& ic = g_icons[(int)icon];
        float sc = size / 24.f, th = 2.f * sc;
        float budget = progress * ic.total;
        std::vector<ImVec2> pts;
        for (const Poly& po : ic.polys) {
            if (budget <= 0.f) break;
            pts.clear();
            size_t n = po.pts.size();
            size_t cnt = n + (po.closed ? 1 : 0);
            bool full = true;
            for (size_t i = 0; i < cnt; ++i) {
                ImVec2 q = po.pts[i % n];
                ImVec2 m(c.x + (q.x - 12.f) * sc, c.y + (q.y - 12.f) * sc);
                if (i > 0) {
                    ImVec2 pr = po.pts[(i - 1) % n];
                    float dx = q.x - pr.x, dy = q.y - pr.y, len = sqrtf(dx*dx + dy*dy);
                    if (len > budget) {
                        float f = len > 0.f ? budget / len : 0.f;
                        pts.push_back(ImVec2(c.x + (pr.x + dx * f - 12.f) * sc, c.y + (pr.y + dy * f - 12.f) * sc));
                        budget = 0.f; full = false;
                        break;
                    }
                    budget -= len;
                }
                pts.push_back(m);
            }
            if (pts.size() > 1) dl->AddPolyline(pts.data(), (int)pts.size(), col, 0, th);
            if (!po.closed || !full) {
                dl->AddCircleFilled(pts.front(), th * 0.5f, col, 8);
                dl->AddCircleFilled(pts.back(), th * 0.5f, col, 8);
            }
        }
    }

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
        for (int i = 0; i < 11; ++i) c[i] = Mix(c[i], t[i], k);
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
    static float g_tabX = 0.f;

    bool BeginWindow(const char* title, ImVec2 size, bool* open)
    {
        ImGuiIO& io = ImGui::GetIO();
        g_dt = io.DeltaTime > 0.05f ? 0.05f : io.DeltaTime;
        g_time += g_dt;
        ThemeStep();
        ApplyStyle();
        g_alpha = Clamp01(g_alpha + g_dt * 6.f);
        g_open = open;

        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, EaseOutCubic(g_alpha));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        bool vis = ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
        if (!vis) return false;
        g_base_alpha = ImGui::GetStyle().Alpha;

        const Theme& t = g_cur;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos(), e(p.x + size.x, p.y + size.y);
        g_wpos = p; g_wsize = size;
        ImVec4 black(0, 0, 0, 1);

        // body + top bar
        dl->AddRect(ImVec2(p.x - 1, p.y - 1), ImVec2(e.x + 1, e.y + 1), U32(A(black, 0.75f)));
        dl->AddRectFilledMultiColor(p, e, U32(t.bg_top), U32(t.bg_top), U32(t.bg_bot), U32(t.bg_bot));
        ImVec4 bar = Mix(t.bg_bot, black, 0.35f);
        dl->AddRectFilled(p, ImVec2(e.x, p.y + kHead), U32(bar));
        dl->AddLine(ImVec2(p.x, p.y + kHead), ImVec2(e.x, p.y + kHead), U32(A(t.text, 0.09f)));
        dl->AddRect(p, e, U32(A(t.text, 0.14f)));

        // wordmark
        ImFont* ft = g_fonts.title;
        Txt(dl, ft, ImVec2(p.x + 24, p.y + (kHead - ft->FontSize) * 0.5f + 1), U32(t.text), "VELOX");
        dl->AddRectFilled(ImVec2(p.x + 24 + TxtSize(ft, "VELOX").x + 4, p.y + kHead * 0.5f + 8), ImVec2(p.x + 24 + TxtSize(ft, "VELOX").x + 10, p.y + kHead * 0.5f + 14), U32(t.acc1));

        // user + close
        {
            ImFont* fb = g_fonts.bold; ImFont* fs = g_fonts.small;
            float rx = e.x - 56.f;
            Txt(dl, fb, ImVec2(rx - TxtSize(fb, "velox").x, p.y + 15), U32(t.text), "velox");
            Txt(dl, fs, ImVec2(rx - TxtSize(fs, "expires: never").x, p.y + 38), U32(t.text_dim), "expires: never");

            ImVec2 cc(e.x - 28, p.y + kHead * 0.5f + 1);
            ImGui::SetCursorScreenPos(ImVec2(cc.x - 14, cc.y - 14));
            ImGui::InvisibleButton("##vx_close", ImVec2(28, 28));
            float h = Anim(ImGui::GetID("##vx_close"), ImGui::IsItemHovered() ? 1.f : 0.f, 1);
            ImU32 xc = U32(Mix(t.text_dim, t.text, h));
            dl->AddLine(ImVec2(cc.x - 5, cc.y - 5), ImVec2(cc.x + 5, cc.y + 5), xc, 1.8f);
            dl->AddLine(ImVec2(cc.x + 5, cc.y - 5), ImVec2(cc.x - 5, cc.y + 5), xc, 1.8f);
            if (ImGui::IsItemClicked() && g_open) *g_open = false;
        }

        // footer
        {
            ImFont* fs = g_fonts.small;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f fps", io.Framerate);
            dl->AddLine(ImVec2(p.x, e.y - kBottom), ImVec2(e.x, e.y - kBottom), U32(A(t.text, 0.07f)));
            Txt(dl, fs, ImVec2(p.x + kPad, e.y - kBottom + 7), U32(A(t.text_dim, 0.8f)), "velox  build 1.0.0");
            Txt(dl, fs, ImVec2(e.x - kPad - TxtSize(fs, buf).x, e.y - kBottom + 7), U32(A(t.text_dim, 0.8f)), buf);
        }

        // drag handle (tabs are submitted after, so they win the hover)
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##vx_drag", ImVec2(size.x - 190, kHead));
        if (ImGui::IsItemActive()) ImGui::SetWindowPos(ImVec2(p.x + io.MouseDelta.x, p.y + io.MouseDelta.y));

        g_tabX = 190.f;
        return true;
    }

    void EndWindow()
    {
        ImGui::End();
        ImGui::PopStyleVar(3);
    }

    void TabSeparator() { g_tabX += 18.f; }

    bool Tab(const char* label, Icon icon, int idx, int* current)
    {
        const Theme& t = g_cur;
        ImFont* f = g_fonts.bold;
        ImVec2 ts = TxtSize(f, label);
        float w = 18.f + 18.f + 8.f + ts.x + 18.f, h = kHead - 3.f;

        ImGui::SetCursorPos(ImVec2(g_tabX, 3.f));
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiID id = ImGui::GetID(label);
        ImGui::InvisibleButton(label, ImVec2(w, h));
        bool clicked = ImGui::IsItemClicked();
        if (clicked && *current != idx) *current = idx;

        bool sel = (*current == idx);
        float a  = Tween(id, sel ? 1.f : 0.f, 1, 0.12f);
        float hv = Anim(id, ImGui::IsItemHovered() ? 1.f : 0.f, 2);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (a > 0.01f) {
            // folder tab: same color as the page, so it joins the content area
            ImVec2 a0(p.x, p.y), a1(p.x + w, p.y + h + 1.f);
            dl->AddRectFilled(a0, a1, U32(A(t.bg_top, a)));
            dl->AddLine(ImVec2(a0.x, a0.y), ImVec2(a0.x, a1.y), U32(A(t.text, 0.10f * a)));
            dl->AddLine(ImVec2(a1.x - 1, a0.y), ImVec2(a1.x - 1, a1.y), U32(A(t.text, 0.10f * a)));
            dl->AddRectFilled(ImVec2(a0.x, a0.y), ImVec2(a1.x, a0.y + 2), U32(A(t.acc1, a)));
        } else if (hv > 0.01f) {
            dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U32(A(t.text, 0.03f * hv)));
        }
        ImVec4 txt = Mix(Mix(t.text_dim, t.text, hv * 0.85f), t.text, a);
        ImVec4 ico = Mix(Mix(t.text_dim, t.text, hv * 0.85f), t.acc1, a);
        DrawSvgIcon(dl, icon, ImVec2(p.x + 18 + 9, p.y + h * 0.5f + 1.f), 20.f, U32(ico), 1.f);
        Txt(dl, f, ImVec2(p.x + 18 + 18 + 8, p.y + h * 0.5f - f->FontSize * 0.5f + 1.f), U32(txt), label);
        g_tabX += w + 2.f;
        return clicked;
    }

    void BeginContent()
    {
        ImGui::SetCursorPos(ImVec2(kPad, kHead + 18.f));
        ImGui::BeginChild("##vx_content", ImVec2(g_wsize.x - kPad * 2, g_wsize.y - kHead - kBottom - 26.f), 0,
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
        dl->AddRectFilled(b0, b1, U32(t.frame), 3.f);
        if (a > 0.01f) {
            ImVec4 hi = A(Mix(t.acc1, ImVec4(1, 1, 1, 1), 0.28f), a), lo = A(Mix(t.acc1, ImVec4(0, 0, 0, 1), 0.30f), a);
            dl->AddRectFilledMultiColor(ImVec2(b0.x + 1, b0.y + 1), ImVec2(b1.x - 1, b1.y - 1), U32(hi), U32(hi), U32(lo), U32(lo));
        }
        dl->AddRect(b0, b1, U32(Mix(A(ImVec4(0, 0, 0, 1), 0.55f), A(t.acc1, 0.9f), h * (1.f - a))), 3.f);
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
    static void Glow(ImDrawList* dl, ImVec2 a, ImVec2 b, const ImVec4& c, float r, float k)
    {
        for (int i = 1; i <= 7; ++i)
            dl->AddRect(ImVec2(a.x - i * 1.7f, a.y - i * 1.7f), ImVec2(b.x + i * 1.7f, b.y + i * 1.7f),
                        U32(A(c, k * 0.07f * (8 - i) / 7.f)), r + i * 1.7f, 0, 2.f);
    }

    struct Toast { std::string title, text; int kind; float life, age, y; bool placed; };
    static std::vector<Toast> g_toasts;

    void Notify(const char* title, const char* text, int kind, float seconds)
    {
        g_toasts.push_back({ title, text, kind, seconds, 0.f, 0.f, false });
        if (g_toasts.size() > 5) g_toasts.erase(g_toasts.begin());
    }

    void Notify(const char* text, int kind, float seconds)
    {
        Notify(kind == 1 ? "Success" : (kind == 2 ? "Warning" : "Info"), text, kind, seconds);
    }

    static float EaseOutBack(float t)
    {
        const float c1 = 1.70158f, c3 = c1 + 1.f;
        float u = t - 1.f;
        return 1.f + c3 * u * u * u + c1 * u * u;
    }

    void RenderNotifications()
    {
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime > 0.05f ? 0.05f : io.DeltaTime;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const Theme& t = g_cur;
        ImFont* fb = g_fonts.bold; ImFont* fs = g_fonts.small;
        const float W = 352.f, H = 82.f, gap = 12.f;
        int n = (int)g_toasts.size();

        for (int i = 0; i < n;) {
            Toast& k = g_toasts[i];
            k.age += dt;
            if (k.age >= k.life + 0.4f) { g_toasts.erase(g_toasts.begin() + i); --n; continue; }
            ++i;
        }
        n = (int)g_toasts.size();
        for (int i = 0; i < n; ++i) {
            Toast& k = g_toasts[i];
            float target = io.DisplaySize.y - 26.f - H - (n - 1 - i) * (H + gap);
            if (!k.placed) { k.y = target; k.placed = true; }
            k.y += (target - k.y) * (1.f - expf(-dt * 14.f));

            ImVec4 c1, c2;
            int icon;
            if (k.kind == 1)      { c1 = ImVec4(0.18f, 0.88f, 0.52f, 1); c2 = ImVec4(0.12f, 0.74f, 0.90f, 1); icon = (int)Icon::Check; }
            else if (k.kind == 2) { c1 = ImVec4(1.00f, 0.72f, 0.18f, 1); c2 = ImVec4(1.00f, 0.38f, 0.24f, 1); icon = (int)Icon::Warn; }
            else                  { c1 = t.acc1;                          c2 = t.acc3;                          icon = (int)Icon::Info; }

            float in  = EaseOutBack(Clamp01(k.age / 0.55f));
            float out = k.age > k.life ? EaseOutCubic((k.age - k.life) / 0.4f) : 0.f;
            float fade = Clamp01(k.age / 0.2f) * (1.f - out);
            float x = io.DisplaySize.x - 26.f - W + (1.f - in) * (W + 60.f) + out * (W + 60.f);
            ImVec2 a(x, k.y), b(x + W, k.y + H);

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade);
            Glow(dl, a, b, c1, 12.f, 0.55f);
            dl->AddRectFilled(a, b, U32(A(t.panel_top, 0.98f)), 12.f);
            GradH(dl, a, b, A(c1, 0.20f), A(c2, 0.02f), 12.f);
            dl->AddRect(a, b, U32(A(c1, 0.45f)), 12.f);
            dl->AddLine(ImVec2(a.x + 14, a.y + 1), ImVec2(b.x - 14, a.y + 1), U32(ImVec4(1, 1, 1, 0.10f)));

            // sweep that runs once when the toast arrives
            if (k.age < 0.9f) {
                float u = k.age / 0.9f, x0 = a.x - 60.f + (W + 120.f) * EaseOutCubic(u);
                dl->PushClipRect(a, b, true);
                dl->AddQuadFilled(ImVec2(x0, a.y), ImVec2(x0 + 46, a.y), ImVec2(x0 + 6, b.y), ImVec2(x0 - 40, b.y), U32(ImVec4(1, 1, 1, 0.13f * (1.f - u))));
                dl->PopClipRect();
            }

            // badge
            ImVec2 b0(a.x + 18, a.y + (H - 46.f) * 0.5f - 3.f), b1(b0.x + 46, b0.y + 46);
            Glow(dl, b0, b1, c1, 14.f, 0.45f);
            GradH(dl, b0, b1, c1, c2, 14.f);
            DrawSvgIcon(dl, (Icon)icon, ImVec2((b0.x + b1.x) * 0.5f, (b0.y + b1.y) * 0.5f), 26.f, U32(ImVec4(1, 1, 1, 1)), 1.f);
            if (k.age < 0.8f) {
                float u = k.age / 0.8f;
                dl->AddCircle(ImVec2((b0.x + b1.x) * 0.5f, (b0.y + b1.y) * 0.5f), 24.f + 18.f * EaseOutCubic(u), U32(A(c1, 0.55f * (1.f - u))), 40, 2.f);
            }

            Txt(dl, fb, ImVec2(a.x + 78, a.y + 13), U32(t.text), k.title.c_str());
            dl->PushClipRect(ImVec2(a.x + 78, a.y), ImVec2(b.x - 14, b.y), true);
            Txt(dl, fs, ImVec2(a.x + 78, a.y + 38), U32(Mix(t.text_dim, t.text, 0.35f)), k.text.c_str());
            dl->PopClipRect();

            // time bar
            float pk = Clamp01(1.f - k.age / k.life);
            ImVec2 p0(a.x + 78, b.y - 15), p1(b.x - 18, b.y - 12);
            GradH(dl, p0, p1, A(t.text, 0.09f), A(t.text, 0.09f), 1.5f);
            if (pk > 0.f) {
                ImVec2 pe(p0.x + (p1.x - p0.x) * pk, p1.y);
                GradH(dl, p0, pe, c1, c2, 1.5f);
                dl->AddCircleFilled(ImVec2(pe.x, (p0.y + p1.y) * 0.5f), 5.f, U32(A(c2, 0.30f)), 16);
                dl->AddCircleFilled(ImVec2(pe.x, (p0.y + p1.y) * 0.5f), 2.5f, U32(ImVec4(1, 1, 1, 0.95f)), 12);
            }
            ImGui::PopStyleVar();
        }
    }
}
