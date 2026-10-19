// language: C++, file: background.cpp
// *Cinco fondos animados: particles, nebula, waves, starfield, aurora.*

#include "background.h"
#include <cmath>
#include <cstdlib>

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

namespace Background {

    // ──────────── helpers ────────────

    static inline ImU32 TintA(ImU32 c, int a) {
        if (a < 0) a = 0; if (a > 255) a = 255;
        return (c & 0x00FFFFFF) | ((ImU32)a << 24);
    }

    static inline ImU32 MixColor(ImU32 c1, ImU32 c2, float t) {
        if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
        int a1 = (c1 >> 24) & 0xFF, r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
        int a2 = (c2 >> 24) & 0xFF, r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
        int a = (int)(a1 + (a2 - a1) * t);
        int r = (int)(r1 + (r2 - r1) * t);
        int g = (int)(g1 + (g2 - g1) * t);
        int b = (int)(b1 + (b2 - b1) * t);
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    // ──────────── estado persistente ────────────

    struct Particle { float x, y, vx, vy; };
    static Particle s_particles[80];
    static bool     s_particles_init = false;

    struct Star { float x, y, z; };
    static Star     s_stars[220];
    static bool     s_stars_init = false;

    // ── nebula ──
    struct NebulaBlob {
        float x, y;     // posición normalizada 0..1
        float r;        // radio base en px
        float phase;    // fase de oscilación
        int   variant;  // 0..3 color
    };
    static NebulaBlob s_blobs[5];
    static bool       s_blobs_init = false;

    // ─── variantes de color derivadas del acento ───
    struct RGBf { float r, g, b; };

    static RGBf VariantFromAccent(int variant, float ar, float ag, float ab) {
        static const RGBf fixed[4] = {
            { 1.00f, 0.35f, 0.95f },   // magenta / hot pink
            { 0.30f, 0.90f, 1.00f },   // cyan
            { 1.00f, 0.55f, 0.25f },   // warm orange
            { 0.75f, 0.35f, 1.00f },   // violet
        };
        RGBf f = fixed[variant % 4];
        RGBf out;
        out.r = ar * 0.40f + f.r * 0.60f;
        out.g = ag * 0.40f + f.g * 0.60f;
        out.b = ab * 0.40f + f.b * 0.60f;
        return out;
    }

    static void InitParticles() {
        for (int i = 0; i < 80; i++) {
            s_particles[i].x = (rand() % 10000) / 10000.0f;
            s_particles[i].y = (rand() % 10000) / 10000.0f;
            s_particles[i].vx = ((rand() % 200) - 100) / 140000.0f;
            s_particles[i].vy = ((rand() % 200) - 100) / 140000.0f;
        }
        s_particles_init = true;
    }

    static void InitStars() {
        for (int i = 0; i < 220; i++) {
            s_stars[i].x = ((rand() % 2000) - 1000) / 1000.0f;
            s_stars[i].y = ((rand() % 2000) - 1000) / 1000.0f;
            s_stars[i].z = (rand() % 950 + 50) / 1000.0f;
        }
        s_stars_init = true;
    }

    static void InitBlobs() {
        s_blobs[0] = { 0.22f, 0.32f, 300.0f, 0.0f, 0 };
        s_blobs[1] = { 0.68f, 0.52f, 380.0f, 1.3f, 1 };
        s_blobs[2] = { 0.42f, 0.78f, 260.0f, 2.7f, 2 };
        s_blobs[3] = { 0.85f, 0.22f, 210.0f, 4.1f, 3 };
        s_blobs[4] = { 0.12f, 0.68f, 200.0f, 5.5f, 0 };
        s_blobs_init = true;
    }

    // ═══════════════════════════════════════════════════════════════
    //  PARTICLES
    // ═══════════════════════════════════════════════════════════════

    static void BgParticles(ImDrawList* dl, ImVec2 p, ImVec2 s, float I, float S, ImU32 A) {
        if (!s_particles_init) InitParticles();
        const int N = 80;
        ImVec2 pts[N];
        float dt = ImGui::GetIO().DeltaTime * S;
        for (int i = 0; i < N; i++) {
            auto& pp = s_particles[i];
            pp.x += pp.vx * dt * 30.0f;
            pp.y += pp.vy * dt * 30.0f;
            if (pp.x < 0.0f) pp.x += 1.0f; else if (pp.x > 1.0f) pp.x -= 1.0f;
            if (pp.y < 0.0f) pp.y += 1.0f; else if (pp.y > 1.0f) pp.y -= 1.0f;
            pts[i] = ImVec2(p.x + pp.x * s.x, p.y + pp.y * s.y);
        }
        const float max_d = 130.0f;
        const float max_d2 = max_d * max_d;
        for (int i = 0; i < N; i++) {
            for (int j = i + 1; j < N; j++) {
                float dx = pts[i].x - pts[j].x;
                float dy = pts[i].y - pts[j].y;
                float d2 = dx * dx + dy * dy;
                if (d2 < max_d2) {
                    float a = 1.0f - sqrtf(d2) / max_d;
                    a = a * a * 0.35f * I;
                    dl->AddLine(pts[i], pts[j], TintA(A, (int)(a * 255)), 1.0f);
                }
            }
        }
        for (int i = 0; i < N; i++) {
            dl->AddCircleFilled(pts[i], 3.6f * I, TintA(A, 0x1a), 8);
            dl->AddCircleFilled(pts[i], 1.8f * I, TintA(A, 0xaa), 6);
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  NEBULA — nubes de gas, corrientes, núcleo, estrellas, viñeta
    // ═══════════════════════════════════════════════════════════════

    static void BgNebula(ImDrawList* dl, ImVec2 p, ImVec2 s, float I, float S, float t, ImU32 A) {
        if (!s_blobs_init) InitBlobs();

        const float ar = (float)((A >> 16) & 0xFF);
        const float ag = (float)((A >> 8) & 0xFF);
        const float ab = (float)(A & 0xFF);

        // ── 1. fondo profundo ──
        dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y),
            IM_COL32(3, 4, 7, (int)(215.0f * I)), 0.0f);

        // ── 2. nubes grandes de gas ──
        for (int i = 0; i < 5; i++) {
            auto& b = s_blobs[i];

            float bx = b.x
                + sinf(t * 0.035f * S + b.phase) * 0.045f
                + sinf(t * 0.017f * S + b.phase * 2.1f) * 0.020f;
            float by = b.y
                + cosf(t * 0.042f * S + b.phase * 1.3f) * 0.045f
                + cosf(t * 0.021f * S + b.phase * 0.7f) * 0.020f;

            float cx = p.x + bx * s.x;
            float cy = p.y + by * s.y;

            float pulse = 1.0f + sinf(t * 0.42f * S + b.phase) * 0.14f;
            float base_r = b.r * pulse * I;

            RGBf c = VariantFromAccent(b.variant, ar, ag, ab);

            const int LAYERS = 20;
            for (int L = 0; L < LAYERS; L++) {
                float u = (float)L / (float)(LAYERS - 1);
                float rr = base_r * (1.0f - u * 0.92f);
                float aa = (0.006f + u * u * 0.028f) * I;
                int a8 = (int)(aa * 255.0f);
                if (a8 <= 0 || rr < 0.5f) continue;

                int r = (int)c.r; if (r > 255) r = 255;
                int g = (int)c.g; if (g > 255) g = 255;
                int bl = (int)c.b; if (bl > 255) bl = 255;

                ImU32 col = ((ImU32)a8 << 24) | (r << 16) | (g << 8) | bl;
                dl->AddCircleFilled(ImVec2(cx, cy), rr, col, 12);
            }
        }

        // ── 3. corrientes de gas (wisps) ──
        const int WISPS = 5;
        const int PUFFS_PER_WISP = 18;

        for (int w = 0; w < WISPS; w++) {
            float wphase = w * 1.7f;

            RGBf c = VariantFromAccent(w, ar, ag, ab);
            int r = (int)c.r; if (r > 255) r = 255;
            int g = (int)c.g; if (g > 255) g = 255;
            int bl = (int)c.b; if (bl > 255) bl = 255;

            for (int k = 0; k < PUFFS_PER_WISP; k++) {
                float u = (float)k / (float)(PUFFS_PER_WISP - 1);

                float x = p.x + u * s.x;
                float y = p.y + s.y * (0.30f + w * 0.09f)
                    + sinf(u * 3.1f + wphase + t * 0.14f * S) * s.y * 0.16f
                    + sinf(u * 7.3f + wphase * 0.7f + t * 0.21f * S) * s.y * 0.06f;

                float env = sinf(u * IM_PI);
                if (env < 0.05f) continue;

                float r_puff = (14.0f + 24.0f * env) * I;

                const int LAYERS = 6;
                for (int L = 0; L < LAYERS; L++) {
                    float uu = (float)L / (float)(LAYERS - 1);
                    float rr = r_puff * (1.0f - uu * 0.88f);
                    float aa = (0.010f + uu * uu * 0.032f) * I * env;
                    int a8 = (int)(aa * 255.0f);
                    if (a8 <= 0 || rr < 0.5f) continue;

                    ImU32 col = ((ImU32)a8 << 24) | (r << 16) | (g << 8) | bl;
                    dl->AddCircleFilled(ImVec2(x, y), rr, col, 8);
                }
            }
        }

        // ── 4. núcleo brillante anclado a la nube mayor ──
        {
            auto& cb = s_blobs[1];
            float cbx = cb.x
                + sinf(t * 0.035f * S + cb.phase) * 0.045f
                + sinf(t * 0.017f * S + cb.phase * 2.1f) * 0.020f;
            float cby = cb.y
                + cosf(t * 0.042f * S + cb.phase * 1.3f) * 0.045f
                + cosf(t * 0.021f * S + cb.phase * 0.7f) * 0.020f;

            float core_x = p.x + (cbx + 0.02f) * s.x;
            float core_y = p.y + (cby - 0.04f) * s.y;

            float core_pulse = 1.0f + sinf(t * 0.75f * S) * 0.18f;

            RGBf cc = VariantFromAccent(cb.variant, ar, ag, ab);

            const int LAYERS = 22;
            float base_r = 200.0f * core_pulse * I;

            for (int L = 0; L < LAYERS; L++) {
                float u = (float)L / (float)(LAYERS - 1);
                float rr = base_r * (1.0f - u * 0.94f);
                if (rr < 0.5f) continue;

                float aa = (0.004f + u * u * u * 0.10f) * I;
                int a8 = (int)(aa * 255.0f);
                if (a8 <= 0) continue;

                float whiten = u * u;
                int r = (int)(cc.r * (1.0f - whiten) + 255.0f * whiten);
                int g = (int)(cc.g * (1.0f - whiten) + 255.0f * whiten);
                int bl = (int)(cc.b * (1.0f - whiten) + 255.0f * whiten);
                if (r > 255) r = 255; if (g > 255) g = 255; if (bl > 255) bl = 255;

                ImU32 col = ((ImU32)a8 << 24) | (r << 16) | (g << 8) | bl;
                dl->AddCircleFilled(ImVec2(core_x, core_y), rr, col, 16);
            }
        }

        // ── 5. campo de estrellas ──
        const int STARS_N = 130;
        for (int i = 0; i < STARS_N; i++) {
            uint32_t h1 = (uint32_t)(i * 2654435761u);
            uint32_t h2 = (uint32_t)(i * 1597334677u);
            uint32_t h3 = (uint32_t)(i * 40503u);

            float rx = (float)(h1 & 0xFFFF) / 65535.0f;
            float ry = (float)(h2 & 0xFFFF) / 65535.0f;
            float sz = (float)(h3 & 0xFF) / 255.0f;

            float sx = p.x + rx * s.x;
            float sy = p.y + ry * s.y;

            float tw = 0.55f + 0.45f * sinf(t * 1.8f * S + i * 1.7f);

            float star_r = (0.7f + sz * 1.6f) * I;
            int a8 = (int)((30.0f + 200.0f * sz) * tw * I);
            if (a8 > 255) a8 = 255;

            if (sz > 0.82f) {
                dl->AddCircleFilled(ImVec2(sx, sy), star_r * 3.0f,
                    TintA(A, (int)(a8 * 0.12f)), 8);
            }
            dl->AddCircleFilled(ImVec2(sx, sy), star_r,
                ((ImU32)a8 << 24) | 0xF0F0FF, 5);
        }

        // ── 6. viñeta — borde oscuro en los cuatro costados ──
        ImU32 solid = IM_COL32(0, 0, 0, (int)(180.0f * I));
        ImU32 transp = IM_COL32(0, 0, 0, 0);

        dl->AddRectFilledMultiColor(p, ImVec2(p.x + s.x, p.y + 90),
            solid, solid, transp, transp);
        dl->AddRectFilledMultiColor(ImVec2(p.x, p.y + s.y - 90), ImVec2(p.x + s.x, p.y + s.y),
            transp, transp, solid, solid);
        dl->AddRectFilledMultiColor(p, ImVec2(p.x + 70, p.y + s.y),
            solid, transp, transp, solid);
        dl->AddRectFilledMultiColor(ImVec2(p.x + s.x - 70, p.y), ImVec2(p.x + s.x, p.y + s.y),
            transp, solid, solid, transp);
    }

    // ═══════════════════════════════════════════════════════════════
    //  WAVES — capas con degradado bajo la curva y glow en la cresta
    // ═══════════════════════════════════════════════════════════════

    static void BgWaves(ImDrawList* dl, ImVec2 p, ImVec2 s, float I, float S, float t, ImU32 A) {
        const int LAYERS = 5;

        int ar = (A >> 16) & 0xFF;
        int ag = (A >> 8) & 0xFF;
        int ab = A & 0xFF;

        for (int w = 0; w < LAYERS; w++) {
            float layer_t = (float)w / (float)(LAYERS - 1);
            float base_y = p.y + s.y * (0.22f + layer_t * 0.22f);
            float amp = 26.0f + w * 14.0f;
            float freq = 0.55f + w * 0.22f;
            float speed = 0.35f + w * 0.22f;
            float phase = t * speed * S + w * 1.7f;

            float bright = 0.55f + layer_t * 0.55f;
            int r = (int)(ar * bright);
            int g = (int)(ag * bright);
            int b = (int)(ab * bright);
            if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;

            int line_a = (int)((0.75f - layer_t * 0.35f) * I * 255);
            int fill_a = (int)((0.18f - layer_t * 0.08f) * I * 255);

            ImU32 line_top = (line_a << 24) | (r << 16) | (g << 8) | b;
            ImU32 fill_col = (fill_a << 24) | (r / 3 << 16) | (g / 3 << 8) | (b / 3);

            const int STEPS = 72;
            ImVec2 prev;

            // cresta con glow
            for (int i = 0; i <= STEPS; i++) {
                float u = (float)i / (float)STEPS;
                float x = p.x + u * s.x;
                float y = base_y
                    + sinf(u * freq * IM_PI * 2.0f + phase) * amp
                    + sinf(u * freq * 2.7f * IM_PI * 2.0f + phase * 1.4f) * amp * 0.22f
                    + sinf(u * freq * 6.3f * IM_PI * 2.0f + phase * 0.8f) * amp * 0.08f;
                ImVec2 cur(x, y);
                if (i > 0) {
                    dl->AddLine(prev, cur,
                        (line_a / 5 << 24) | (r << 16) | (g << 8) | b, 4.0f);
                    dl->AddLine(prev, cur, line_top, 1.6f);
                }
                prev = cur;
            }

            // fill: trapecios hasta el fondo del frame
            prev = ImVec2(p.x, 0);
            ImVec2 prev_bot;
            for (int i = 0; i <= STEPS; i++) {
                float u = (float)i / (float)STEPS;
                float x = p.x + u * s.x;
                float y = base_y
                    + sinf(u * freq * IM_PI * 2.0f + phase) * amp
                    + sinf(u * freq * 2.7f * IM_PI * 2.0f + phase * 1.4f) * amp * 0.22f
                    + sinf(u * freq * 6.3f * IM_PI * 2.0f + phase * 0.8f) * amp * 0.08f;
                ImVec2 cur(x, y);
                ImVec2 bot(x, p.y + s.y);
                if (i > 0) {
                    dl->AddQuadFilled(prev, cur, bot, prev_bot, fill_col);
                }
                prev = cur;
                prev_bot = bot;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  STARFIELD
    // ═══════════════════════════════════════════════════════════════

    static void BgStarfield(ImDrawList* dl, ImVec2 p, ImVec2 s, float I, float S, ImU32 A) {
        if (!s_stars_init) InitStars();
        float cx = p.x + s.x * 0.5f;
        float cy = p.y + s.y * 0.5f;
        float half = (s.x < s.y ? s.x : s.y) * 0.5f;
        float spd = 0.15f * S;
        float dt = ImGui::GetIO().DeltaTime;
        for (int i = 0; i < 220; i++) {
            auto& st = s_stars[i];
            st.z -= spd * dt;
            if (st.z <= 0.05f) {
                st.z = 1.0f;
                st.x = ((rand() % 2000) - 1000) / 1000.0f;
                st.y = ((rand() % 2000) - 1000) / 1000.0f;
            }
            float sx = cx + (st.x / st.z) * half * 0.85f;
            float sy = cy + (st.y / st.z) * half * 0.85f;
            if (sx < p.x || sx > p.x + s.x || sy < p.y || sy > p.y + s.y) continue;
            float depth = 1.0f - st.z;
            float r = 0.6f + depth * 2.4f * I;
            int alpha = (int)(depth * 200 * I);
            float px = cx + (st.x / (st.z + 0.02f)) * half * 0.85f;
            float py = cy + (st.y / (st.z + 0.02f)) * half * 0.85f;
            dl->AddLine(ImVec2(px, py), ImVec2(sx, sy), TintA(A, alpha / 3), 1.0f);
            dl->AddCircleFilled(ImVec2(sx, sy), r, TintA(A, alpha), 6);
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  AURORA — cintas con 3 pasadas, ancho variable, partículas
    // ═══════════════════════════════════════════════════════════════

    static void BgAurora(ImDrawList* dl, ImVec2 p, ImVec2 s, float I, float S, float t, ImU32 A) {
        const int RIBBONS = 5;

        int ar = (A >> 16) & 0xFF;
        int ag = (A >> 8) & 0xFF;
        int ab = A & 0xFF;

        int br = ar, bg_ = ag, bb = ab;

        for (int r = 0; r < RIBBONS; r++) {
            float layer_t = (float)r / (float)(RIBBONS - 1);

            float base_y = p.y + s.y * (0.18f + layer_t * 0.55f);
            float phase = t * (0.28f + layer_t * 0.35f) * S + r * 1.9f;
            float amp_a = 34.0f + r * 10.0f;
            float amp_b = 18.0f + r * 6.0f;
            float amp_c = 8.0f + r * 3.0f;

            float bright = 0.55f + (1.0f - layer_t) * 0.55f;
            int c1_r = (int)(br * bright);
            int c1_g = (int)(bg_ * bright);
            int c1_b = (int)(bb * bright);
            int c2_r = (int)(br * (0.6f + layer_t * 0.4f));
            int c2_g = (int)(bg_ * (0.9f + (1.0f - layer_t) * 0.1f));
            int c2_b = (int)(bb * (1.0f + layer_t * 0.15f));
            if (c1_r > 255) c1_r = 255; if (c1_g > 255) c1_g = 255; if (c1_b > 255) c1_b = 255;
            if (c2_r > 255) c2_r = 255; if (c2_g > 255) c2_g = 255; if (c2_b > 255) c2_b = 255;

            const int STEPS = 56;

            ImVec2 pts[STEPS + 1];
            ImVec2 nrm[STEPS + 1];
            for (int i = 0; i <= STEPS; i++) {
                float u = (float)i / (float)STEPS;
                float x = p.x + u * s.x;
                float y = base_y
                    + sinf(u * IM_PI * 2.0f + phase) * amp_a
                    + sinf(u * IM_PI * 4.3f + phase * 1.27f) * amp_b
                    + sinf(u * IM_PI * 9.1f + phase * 0.68f) * amp_c;
                pts[i] = ImVec2(x, y);

                float du = 1.0f / (float)STEPS;
                float x2 = p.x + (u + du) * s.x;
                float y2 = base_y
                    + sinf((u + du) * IM_PI * 2.0f + phase) * amp_a
                    + sinf((u + du) * IM_PI * 4.3f + phase * 1.27f) * amp_b
                    + sinf((u + du) * IM_PI * 9.1f + phase * 0.68f) * amp_c;
                float tx = x2 - x, ty = y2 - y;
                float len = sqrtf(tx * tx + ty * ty);
                if (len < 0.0001f) len = 0.0001f;
                nrm[i] = ImVec2(-ty / len, tx / len);
            }

            struct Pass { float width; float alpha_scale; };
            Pass passes[] = {
                { 46.0f, 0.10f },
                { 22.0f, 0.18f },
                { 7.0f,  0.40f },
            };

            for (int pass = 0; pass < 3; pass++) {
                float pw = passes[pass].width;
                float alpha_scale = passes[pass].alpha_scale * I;

                for (int i = 0; i < STEPS; i++) {
                    ImVec2 p0 = pts[i];
                    ImVec2 p1 = pts[i + 1];
                    ImVec2 n0 = nrm[i];
                    ImVec2 n1 = nrm[i + 1];

                    float u = (float)i / (float)STEPS;
                    float envel = sinf(u * IM_PI);
                    float wl = pw * (0.35f + 0.65f * envel);

                    ImVec2 qa0(p0.x + n0.x * wl * 0.5f, p0.y + n0.y * wl * 0.5f);
                    ImVec2 qa1(p1.x + n1.x * wl * 0.5f, p1.y + n1.y * wl * 0.5f);
                    ImVec2 qb0(p0.x - n0.x * wl * 0.5f, p0.y - n0.y * wl * 0.5f);
                    ImVec2 qb1(p1.x - n1.x * wl * 0.5f, p1.y - n1.y * wl * 0.5f);

                    float mix = 0.5f + 0.5f * sinf(u * IM_PI * 1.5f + phase * 0.6f);
                    int rr = (int)(c1_r + (c2_r - c1_r) * mix);
                    int gg = (int)(c1_g + (c2_g - c1_g) * mix);
                    int bb_ = (int)(c1_b + (c2_b - c1_b) * mix);

                    int alpha = (int)(alpha_scale * envel * 255);
                    ImU32 col = ((ImU32)alpha << 24) | (rr << 16) | (gg << 8) | bb_;

                    dl->AddQuadFilled(qa0, qa1, qb1, qb0, col);
                }
            }
        }

        const int PARTICLES = 40;
        for (int i = 0; i < PARTICLES; i++) {
            float seed = (float)i / (float)PARTICLES;
            float px = p.x + fmodf(seed * s.x + t * 22.0f * S, s.x);
            float py = p.y + s.y * 0.35f
                + sinf(seed * 7.3f + t * 0.9f * S) * 70.0f
                + sinf(seed * 13.7f + t * 0.5f * S) * 30.0f;
            float flick = 0.5f + 0.5f * sinf(t * 3.0f + seed * 17.0f);
            int a = (int)(flick * 130.0f * I);
            dl->AddCircleFilled(ImVec2(px, py), 1.6f * I, TintA(A, a), 8);
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  PÚBLICO
    // ═══════════════════════════════════════════════════════════════

    const char* StyleName(int s) {
        switch (s) {
        case None:      return "none";
        case Particles: return "particles";
        case Nebula:    return "nebula";
        case Waves:     return "waves";
        case Starfield: return "starfield";
        case Aurora:    return "aurora";
        }
        return "unknown";
    }

    void Draw(ImDrawList* dl, ImVec2 pos, ImVec2 size,
        int style, float intensity, float speed, float time, ImU32 accent) {
        if (style == None || style == Count || intensity <= 0.0f) return;
        dl->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);
        switch (style) {
        case Particles: BgParticles(dl, pos, size, intensity, speed, accent); break;
        case Nebula:    BgNebula(dl, pos, size, intensity, speed, time, accent); break;
        case Waves:     BgWaves(dl, pos, size, intensity, speed, time, accent); break;
        case Starfield: BgStarfield(dl, pos, size, intensity, speed, accent); break;
        case Aurora:    BgAurora(dl, pos, size, intensity, speed, time, accent); break;
        }
        dl->PopClipRect();
    }

} // namespace Background