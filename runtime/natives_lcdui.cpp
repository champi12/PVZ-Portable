// Software renderer behind javax.microedition.lcdui.Graphics / Image, plus Display natives.
#include "classes.h"
#include "platform.h"
#include "port.h"

#include <cmath>
#include <cstdlib>

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

typedef J_javax_microedition_lcdui_Graphics Gfx;
typedef J_javax_microedition_lcdui_Image Img;

struct Target {
    uint32_t* pix;
    int w, h;
    int cx0, cy0, cx1, cy1;  // clip, exclusive max
    int tx, ty;
    uint32_t color;
};

static bool target_of(JObject* g, Target& t) {
    Gfx* gfx = (Gfx*)JNN(g);
    Img* img = (Img*)gfx->f_img_Ljavax_microedition_lcdui_Image_;
    t.pix = jadata<uint32_t>(img->f_pixels_AI);
    t.w = img->f_width_I;
    t.h = img->f_height_I;
    t.cx0 = gfx->f_cx_I;
    t.cy0 = gfx->f_cy_I;
    t.cx1 = gfx->f_cx_I + gfx->f_cw_I;
    t.cy1 = gfx->f_cy_I + gfx->f_ch_I;
    if (t.cx0 < 0) t.cx0 = 0;
    if (t.cy0 < 0) t.cy0 = 0;
    if (t.cx1 > t.w) t.cx1 = t.w;
    if (t.cy1 > t.h) t.cy1 = t.h;
    t.tx = gfx->f_tx_I;
    t.ty = gfx->f_ty_I;
    t.color = 0xFF000000u | (uint32_t)gfx->f_color_I;
    return t.cx0 < t.cx1 && t.cy0 < t.cy1;
}

static inline uint32_t blend(uint32_t d, uint32_t s) {
    uint32_t a = s >> 24;
    if (a == 255) return s;
    if (a == 0) return d;
    uint32_t ia = 255 - a;
    uint32_t rb = (((s & 0xFF00FF) * a + (d & 0xFF00FF) * ia + 0x800080) >> 8) & 0xFF00FF;
    uint32_t g = (((s & 0x00FF00) * a + (d & 0x00FF00) * ia + 0x008000) >> 8) & 0x00FF00;
    return 0xFF000000u | rb | g;
}

static inline void plot(Target& t, int x, int y) {
    if (x >= t.cx0 && x < t.cx1 && y >= t.cy0 && y < t.cy1) t.pix[y * t.w + x] = t.color;
}

void M_javax_microedition_lcdui_Graphics__fillRect__IIII_V(JObject* g, int32_t x, int32_t y, int32_t w, int32_t h) {
    Target t;
    if (!target_of(g, t) || w <= 0 || h <= 0) return;
    int x0 = x + t.tx, y0 = y + t.ty, x1 = x0 + w, y1 = y0 + h;
    if (x0 < t.cx0) x0 = t.cx0;
    if (y0 < t.cy0) y0 = t.cy0;
    if (x1 > t.cx1) x1 = t.cx1;
    if (y1 > t.cy1) y1 = t.cy1;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = t.pix + yy * t.w;
        for (int xx = x0; xx < x1; xx++) row[xx] = t.color;
    }
}

void M_javax_microedition_lcdui_Graphics__drawLine__IIII_V(JObject* g, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    Target t;
    if (!target_of(g, t)) return;
    x1 += t.tx; x2 += t.tx; y1 += t.ty; y2 += t.ty;
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 100000; guard++) {
        plot(t, x1, y1);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void M_javax_microedition_lcdui_Graphics__fillArc__IIIIII_V(JObject* g, int32_t x, int32_t y, int32_t w, int32_t h,
                                                           int32_t start, int32_t arc) {
    Target t;
    if (!target_of(g, t) || w <= 0 || h <= 0 || arc == 0) return;
    x += t.tx;
    y += t.ty;
    bool full = arc >= 360 || arc <= -360;
    if (arc < 0) { start += arc; arc = -arc; }
    start %= 360;
    if (start < 0) start += 360;
    double cx = x + w / 2.0, cy = y + h / 2.0, rx = w / 2.0, ry = h / 2.0;
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            double nx = (xx + 0.5 - cx) / rx, ny = (yy + 0.5 - cy) / ry;
            if (nx * nx + ny * ny > 1.0) continue;
            if (!full) {
                double ang = atan2(-ny, nx) * 180.0 / M_PI;
                if (ang < 0) ang += 360;
                double rel = ang - start;
                if (rel < 0) rel += 360;
                if (rel > arc) continue;
            }
            plot(t, xx, yy);
        }
    }
}

void M_javax_microedition_lcdui_Graphics__fillTriangle__IIIIII_V(JObject* g, int32_t x1, int32_t y1, int32_t x2,
                                                                int32_t y2, int32_t x3, int32_t y3) {
    Target t;
    if (!target_of(g, t)) return;
    x1 += t.tx; x2 += t.tx; x3 += t.tx; y1 += t.ty; y2 += t.ty; y3 += t.ty;
    int minx = std::min(x1, std::min(x2, x3)), maxx = std::max(x1, std::max(x2, x3));
    int miny = std::min(y1, std::min(y2, y3)), maxy = std::max(y1, std::max(y2, y3));
    if (minx < t.cx0) minx = t.cx0;
    if (miny < t.cy0) miny = t.cy0;
    if (maxx >= t.cx1) maxx = t.cx1 - 1;
    if (maxy >= t.cy1) maxy = t.cy1 - 1;
    long area = (long)(x2 - x1) * (y3 - y1) - (long)(y2 - y1) * (x3 - x1);
    if (area == 0) return;
    for (int yy = miny; yy <= maxy; yy++) {
        for (int xx = minx; xx <= maxx; xx++) {
            long w0 = (long)(x2 - x1) * (yy - y1) - (long)(y2 - y1) * (xx - x1);
            long w1 = (long)(x3 - x2) * (yy - y2) - (long)(y3 - y2) * (xx - x2);
            long w2 = (long)(x1 - x3) * (yy - y3) - (long)(y1 - y3) * (xx - x3);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) t.pix[yy * t.w + xx] = t.color;
        }
    }
}

// Maps a destination offset (u, v) inside the transformed region to the source offset.
static inline void inv_transform(int tr, int u, int v, int w, int h, int& x, int& y) {
    switch (tr) {
        default:
        case 0: x = u; y = v; break;                     // NONE
        case 1: x = u; y = h - 1 - v; break;             // MIRROR_ROT180
        case 2: x = w - 1 - u; y = v; break;             // MIRROR
        case 3: x = w - 1 - u; y = h - 1 - v; break;     // ROT180
        case 4: x = v; y = u; break;                     // MIRROR_ROT270
        case 5: x = v; y = h - 1 - u; break;             // ROT90
        case 6: x = w - 1 - v; y = u; break;             // ROT270
        case 7: x = w - 1 - v; y = h - 1 - u; break;     // MIRROR_ROT90
    }
}

void port_blit(uint32_t* dst, int dw, int dh, int cx0, int cy0, int cx1, int cy1, const uint32_t* src, int srcw,
               int sx, int sy, int w, int h, int tr, int x, int y) {
    int ow = (tr & 4) ? h : w, oh = (tr & 4) ? w : h;
    int x0 = x, y0 = y, x1 = x + ow, y1 = y + oh;
    if (cx0 < 0) cx0 = 0;
    if (cy0 < 0) cy0 = 0;
    if (cx1 > dw) cx1 = dw;
    if (cy1 > dh) cy1 = dh;
    if (x0 < cx0) x0 = cx0;
    if (y0 < cy0) y0 = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    if (x0 >= x1 || y0 >= y1) return;
    if (tr == 0 || tr == 2) {
        for (int yy = y0; yy < y1; yy++) {
            const uint32_t* s = src + (sy + yy - y) * srcw + sx;
            uint32_t* d = dst + yy * dw;
            if (tr == 0) {
                for (int xx = x0; xx < x1; xx++) d[xx] = blend(d[xx], s[xx - x]);
            } else {
                for (int xx = x0; xx < x1; xx++) d[xx] = blend(d[xx], s[w - 1 - (xx - x)]);
            }
        }
        return;
    }
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* d = dst + yy * dw;
        for (int xx = x0; xx < x1; xx++) {
            int u, v;
            inv_transform(tr, xx - x, yy - y, w, h, u, v);
            d[xx] = blend(d[xx], src[(sy + v) * srcw + sx + u]);
        }
    }
}

void M_javax_microedition_lcdui_Graphics__blit__AIIIIIIIII_V(JObject* g, JObject* src, int32_t srcw, int32_t sx,
                                                             int32_t sy, int32_t w, int32_t h, int32_t tr, int32_t x,
                                                             int32_t y) {
    Target t;
    if (!target_of(g, t)) return;
    port_blit(t.pix, t.w, t.h, t.cx0, t.cy0, t.cx1, t.cy1, jadata<uint32_t>(JNN(src)), srcw, sx, sy, w, h, tr,
              x + t.tx, y + t.ty);
}

void M_javax_microedition_lcdui_Graphics__transformRegion__AIIIIIIIAIAI_V(JObject* src, int32_t srcw, int32_t sx,
                                                                          int32_t sy, int32_t w, int32_t h,
                                                                          int32_t tr, JObject* out, JObject* wh) {
    int ow = (tr & 4) ? h : w, oh = (tr & 4) ? w : h;
    uint32_t* o = jadata<uint32_t>(JNN(out));
    const uint32_t* s = jadata<uint32_t>(JNN(src));
    for (int v = 0; v < oh; v++)
        for (int u = 0; u < ow; u++) {
            int x, y;
            inv_transform(tr, u, v, w, h, x, y);
            o[v * ow + u] = s[(sy + y) * srcw + sx + x];
        }
    jaset<int32_t>(wh, 0, ow);
    jaset<int32_t>(wh, 1, oh);
}

void M_javax_microedition_lcdui_Graphics__drawRGB__AIIIIIIIZ_V(JObject* g, JObject* rgb, int32_t off, int32_t scan,
                                                               int32_t x, int32_t y, int32_t w, int32_t h,
                                                               int32_t alpha) {
    Target t;
    if (!target_of(g, t) || w <= 0 || h <= 0) return;
    x += t.tx;
    y += t.ty;
    const uint32_t* s = jadata<uint32_t>(JNN(rgb));
    int32_t len = ((JArray*)rgb)->length;
    for (int v = 0; v < h; v++) {
        int yy = y + v;
        if (yy < t.cy0 || yy >= t.cy1) continue;
        for (int u = 0; u < w; u++) {
            int xx = x + u;
            if (xx < t.cx0 || xx >= t.cx1) continue;
            int i = off + v * scan + u;
            if (i < 0 || i >= len) jthrow_aioobe(i);
            uint32_t p = s[i];
            uint32_t* d = &t.pix[yy * t.w + xx];
            *d = alpha ? blend(*d, p) : (p | 0xFF000000u);
        }
    }
}

JObject* M_javax_microedition_lcdui_Image__decode__ABIIAI_AI(JObject* data, int32_t off, int32_t len, JObject* wh) {
    JNN(data);
    if (off < 0 || len < 0 || off + len > ((JArray*)data)->length) jthrow_aioobe(off);
    int w, h, n;
    unsigned char* px = stbi_load_from_memory(jadata<uint8_t>(data) + off, len, &w, &h, &n, 4);
    if (px == nullptr) return nullptr;
    JObject* out = jnewarray(&AC_AI, w * h);
    uint32_t* o = jadata<uint32_t>(out);
    for (int i = 0; i < w * h; i++) {
        const unsigned char* p = px + i * 4;
        o[i] = ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    }
    stbi_image_free(px);
    jaset<int32_t>(wh, 0, w);
    jaset<int32_t>(wh, 1, h);
    return out;
}

// ---- Display ----

int32_t M_javax_microedition_lcdui_Display__screenWidth___I() { return SCREEN_W; }
int32_t M_javax_microedition_lcdui_Display__screenHeight___I() { return SCREEN_H; }

int32_t M_javax_microedition_lcdui_Display__pollEvents__AI_I(JObject* buf) {
    return port_poll_events(jadata<int32_t>(JNN(buf)), ((JArray*)buf)->length / 3);
}

void M_javax_microedition_lcdui_Display__present__AIII_V(JObject* pixels, int32_t w, int32_t h) {
    if (w != SCREEN_W || h != SCREEN_H) return;
    port_present(jadata<uint32_t>(JNN(pixels)));
}
