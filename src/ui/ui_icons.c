/*
 * アイコン。画面モック v2 の SVG（24×24、線の太さ 1.9）を、LVGL の線・円弧・三角形で描く。
 * 色は部品の text_color（親から引き継ぐ）を使う。
 */
#include "ui_internal.h"

#include <math.h>
#include <stdint.h>

typedef struct pen {
    lv_layer_t *layer;
    float ox;
    float oy;
    float s; /* 1 単位（24 分の 1）あたりの px */
    lv_color_t color;
    lv_opa_t opa;
    int32_t width;
} pen;

static lv_value_precise_t px_x(const pen *p, float x) {
    return (lv_value_precise_t)lroundf(p->ox + x * p->s);
}

static lv_value_precise_t px_y(const pen *p, float y) {
    return (lv_value_precise_t)lroundf(p->oy + y * p->s);
}

static void set_stroke(pen *p, float units) {
    float w = units * p->s;
    p->width = w < 1.0f ? 1 : (int32_t)lroundf(w);
}

static void line(const pen *p, float x1, float y1, float x2, float y2) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = p->color;
    d.opa = p->opa;
    d.width = p->width;
    d.round_start = 1;
    d.round_end = 1;
    d.p1.x = px_x(p, x1);
    d.p1.y = px_y(p, y1);
    d.p2.x = px_x(p, x2);
    d.p2.y = px_y(p, y2);
    lv_draw_line(p->layer, &d);
}

/* 折れ線。pts は x, y の組を n 個 */
static void poly(const pen *p, const float *pts, int n) {
    for (int i = 0; i + 1 < n; i++) {
        line(p, pts[i * 2], pts[i * 2 + 1], pts[i * 2 + 2], pts[i * 2 + 3]);
    }
}

/* 線の円弧。角度は 3 時の方向が 0 度で、時計回りに増える */
static void arc(const pen *p, float cx, float cy, float r, float a0, float a1) {
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.color = p->color;
    d.opa = p->opa;
    d.width = p->width;
    d.rounded = 1;
    d.center.x = (int32_t)lroundf(p->ox + cx * p->s);
    d.center.y = (int32_t)lroundf(p->oy + cy * p->s);
    d.radius = (uint16_t)lroundf(r * p->s + (float)p->width / 2.0f);
    d.start_angle = (lv_value_precise_t)a0;
    d.end_angle = (lv_value_precise_t)a1;
    lv_draw_arc(p->layer, &d);
}

static void circle(const pen *p, float cx, float cy, float r) {
    arc(p, cx, cy, r, 0.0f, 360.0f);
}

static void fill_circle(const pen *p, float cx, float cy, float r, lv_color_t color) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    d.bg_opa = p->opa;
    d.radius = LV_RADIUS_CIRCLE;
    lv_area_t a;
    a.x1 = (int32_t)lroundf(p->ox + (cx - r) * p->s);
    a.y1 = (int32_t)lroundf(p->oy + (cy - r) * p->s);
    a.x2 = (int32_t)lroundf(p->ox + (cx + r) * p->s) - 1;
    a.y2 = (int32_t)lroundf(p->oy + (cy + r) * p->s) - 1;
    lv_draw_rect(p->layer, &d, &a);
}

/* 角丸の四角の線 */
static void rrect(const pen *p, float x, float y, float w, float h, float r) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.border_color = p->color;
    d.border_opa = p->opa;
    d.border_width = p->width;
    d.radius = (int32_t)lroundf(r * p->s + (float)p->width / 2.0f);
    float half = (float)p->width / 2.0f;
    lv_area_t a;
    a.x1 = (int32_t)lroundf(p->ox + x * p->s - half);
    a.y1 = (int32_t)lroundf(p->oy + y * p->s - half);
    a.x2 = (int32_t)lroundf(p->ox + (x + w) * p->s + half) - 1;
    a.y2 = (int32_t)lroundf(p->oy + (y + h) * p->s + half) - 1;
    lv_draw_rect(p->layer, &d, &a);
}

static void fill_triangle(const pen *p, float x1, float y1, float x2, float y2, float x3, float y3,
                          lv_color_t color) {
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.color = color;
    d.opa = p->opa;
    d.p[0].x = px_x(p, x1);
    d.p[0].y = px_y(p, y1);
    d.p[1].x = px_x(p, x2);
    d.p[1].y = px_y(p, y2);
    d.p[2].x = px_x(p, x3);
    d.p[2].y = px_y(p, y3);
    lv_draw_triangle(p->layer, &d);
}

/* 3 次ベジェ曲線を線分に分けて描く */
static void cubic(const pen *p, float x0, float y0, float x1, float y1, float x2, float y2,
                  float x3, float y3) {
    const int steps = 8;
    float px = x0;
    float py = y0;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float u = 1.0f - t;
        float x = u * u * u * x0 + 3.0f * u * u * t * x1 + 3.0f * u * t * t * x2 + t * t * t * x3;
        float y = u * u * u * y0 + 3.0f * u * u * t * y1 + 3.0f * u * t * t * y2 + t * t * t * y3;
        line(p, px, py, x, y);
        px = x;
        py = y;
    }
}

static void sea_wave(const pen *p, float y) {
    float h = y - 1.5f;
    cubic(p, 3.0f, y, 5.0f, y, 5.0f, h, 7.5f, h);
    cubic(p, 7.5f, h, 10.0f, h, 9.5f, y, 12.0f, y);
    cubic(p, 12.0f, y, 14.5f, y, 14.5f, h, 16.5f, h);
    cubic(p, 16.5f, h, 18.5f, h, 19.0f, y, 21.0f, y);
}

static void draw_logo(pen *p) {
    /* 三日月（ブランドの色）と、ねむのきの芽（グラフの色）。元の図は 28×28 */
    p->s = p->s * 24.0f / 28.0f;
    fill_circle(p, 18.09f, 13.99f, 9.5f, NK_BRAND);
    fill_circle(p, 21.0f, 13.15f, 9.0f, NK_BG);
    fill_triangle(p, 9.0f, 25.5f, 13.5f, 19.4f, 12.0f, 25.5f, NK_SERIES);
    fill_triangle(p, 9.0f, 25.5f, 4.5f, 19.4f, 6.0f, 25.5f, NK_SERIES);
}

static void draw_shape(pen *p, nk_icon icon) {
    switch (icon) {
    case NK_ICON_HOME: {
        static const float pts[] = {4,    11.5f, 12,    5,  20,    11.5f, 20,   19,
                                    19,   20,    14.5f, 20, 14.5f, 14.5f, 9.5f, 14.5f,
                                    9.5f, 20,    5,     20, 4,     19,    4,    11.5f};
        poly(p, pts, 12);
        break;
    }
    case NK_ICON_CHART:
        set_stroke(p, 2.4f);
        line(p, 5, 20, 5, 13);
        line(p, 12, 20, 12, 5);
        line(p, 19, 20, 19, 10);
        break;
    case NK_ICON_MOON:
        arc(p, 12.64f, 11.36f, 8.0f, 23.1f, 246.9f);
        arc(p, 16.86f, 7.14f, 8.0f, 66.9f, 203.1f);
        break;
    case NK_ICON_CURTAIN:
        line(p, 3, 4, 21, 4);
        line(p, 5, 4, 5, 20);
        line(p, 19, 4, 19, 20);
        cubic(p, 5, 4, 5, 10, 7.5f, 15, 11, 20);
        cubic(p, 19, 4, 19, 10, 16.5f, 15, 13, 20);
        break;
    case NK_ICON_GEAR:
        circle(p, 12, 12, 3);
        line(p, 12, 2.8f, 12, 5.2f);
        line(p, 12, 18.8f, 12, 21.2f);
        line(p, 2.8f, 12, 5.2f, 12);
        line(p, 18.8f, 12, 21.2f, 12);
        line(p, 5.5f, 5.5f, 7.2f, 7.2f);
        line(p, 16.8f, 16.8f, 18.5f, 18.5f);
        line(p, 5.5f, 18.5f, 7.2f, 16.8f);
        line(p, 16.8f, 7.2f, 18.5f, 5.5f);
        break;
    case NK_ICON_PHONE:
        rrect(p, 7, 3, 10, 18, 2.2f);
        line(p, 11, 17.5f, 13, 17.5f);
        break;
    case NK_ICON_SPARK: {
        static const float star[] = {12, 3,  13.8f, 8.2f,  19, 10, 13.8f, 11.8f,
                                     12, 17, 10.2f, 11.8f, 5,  10, 10.2f, 8.2f};
        for (int i = 0; i < 8; i++) {
            int j = (i + 1) % 8;
            fill_triangle(p, 12, 10, star[i * 2], star[i * 2 + 1], star[j * 2], star[j * 2 + 1],
                          p->color);
        }
        break;
    }
    case NK_ICON_MIC:
        rrect(p, 9, 3, 6, 11, 3);
        arc(p, 12, 11, 6.5f, 0, 180);
        line(p, 12, 17.5f, 12, 21);
        break;
    case NK_ICON_LOCK:
        rrect(p, 5, 10.5f, 14, 10, 2.2f);
        line(p, 8, 10.5f, 8, 8);
        arc(p, 12, 8, 4, 180, 360);
        line(p, 16, 8, 16, 10.5f);
        break;
    case NK_ICON_TRASH: {
        static const float lid[] = {9.5f, 7, 9.5f, 4.5f, 14.5f, 4.5f, 14.5f, 7};
        static const float can[] = {6.5f, 7, 7.5f, 20, 16.5f, 20, 17.5f, 7};
        line(p, 4.5f, 7, 19.5f, 7);
        poly(p, lid, 4);
        poly(p, can, 4);
        break;
    }
    case NK_ICON_SUN:
        circle(p, 12, 12, 4);
        line(p, 12, 2.5f, 12, 5);
        line(p, 12, 19, 12, 21.5f);
        line(p, 2.5f, 12, 5, 12);
        line(p, 19, 12, 21.5f, 12);
        line(p, 5.3f, 5.3f, 7.1f, 7.1f);
        line(p, 16.9f, 16.9f, 18.7f, 18.7f);
        line(p, 5.3f, 18.7f, 7.1f, 16.9f);
        line(p, 16.9f, 7.1f, 18.7f, 5.3f);
        break;
    case NK_ICON_INFO:
        circle(p, 12, 12, 9);
        line(p, 12, 11, 12, 16.5f);
        line(p, 12, 7.6f, 12, 7.8f);
        break;
    case NK_ICON_LEFT: {
        static const float pts[] = {15, 6, 9, 12, 15, 18};
        poly(p, pts, 3);
        break;
    }
    case NK_ICON_RIGHT: {
        static const float pts[] = {9, 6, 15, 12, 9, 18};
        poly(p, pts, 3);
        break;
    }
    case NK_ICON_CHECK: {
        static const float pts[] = {5, 12.5f, 9.5f, 17, 19, 7};
        set_stroke(p, 2.6f);
        poly(p, pts, 3);
        break;
    }
    case NK_ICON_CHECK_CIRCLE: {
        static const float pts[] = {7.5f, 12.5f, 10.5f, 15.5f, 16.5f, 9};
        fill_circle(p, 12, 12, 10, p->color);
        p->color = NK_CARD;
        set_stroke(p, 2.4f);
        poly(p, pts, 3);
        break;
    }
    case NK_ICON_WAVE: {
        static const float pts[] = {3, 12, 5, 12, 7, 7, 10, 17, 13, 5, 16, 14, 18, 12, 21, 12};
        poly(p, pts, 8);
        break;
    }
    case NK_ICON_RAIN:
        arc(p, 7.06f, 10.5f, 4.5f, 91.0f, 282.0f);
        arc(p, 13.01f, 8.37f, 5.5f, 204.4f, 356.1f);
        arc(p, 18.25f, 11.5f, 3.51f, 274.1f, 94.1f);
        line(p, 18, 15, 7, 15);
        line(p, 8, 18, 7, 20.5f);
        line(p, 12, 18, 11, 20.5f);
        line(p, 16, 18, 15, 20.5f);
        break;
    case NK_ICON_SEA:
        sea_wave(p, 9);
        sea_wave(p, 14);
        sea_wave(p, 19);
        break;
    case NK_ICON_MUSIC: {
        static const float pts[] = {9, 18, 9, 5, 20, 3, 20, 16};
        poly(p, pts, 4);
        circle(p, 6.5f, 18, 2.5f);
        circle(p, 17.5f, 16, 2.5f);
        break;
    }
    case NK_ICON_PAUSE:
        set_stroke(p, 3.0f);
        line(p, 9, 6, 9, 18);
        line(p, 15, 6, 15, 18);
        break;
    case NK_ICON_PLAY:
        fill_triangle(p, 8, 5, 19.5f, 12, 8, 19, p->color);
        break;
    case NK_ICON_VOL: {
        static const float pts[] = {4,     9.5f, 7.5f,  9.5f, 12,    5.5f, 12,
                                    18.5f, 7.5f, 14.5f, 4,    14.5f, 4,    9.5f};
        poly(p, pts, 7);
        arc(p, 13.35f, 12, 4.0f, 311.4f, 48.6f);
        arc(p, 13.4f, 12, 7.5f, 312.8f, 47.2f);
        break;
    }
    case NK_ICON_CLOCK: {
        static const float pts[] = {12, 7.5f, 12, 12, 15, 14};
        circle(p, 12, 12, 8.5f);
        poly(p, pts, 3);
        break;
    }
    case NK_ICON_ALARM: {
        static const float pts[] = {12, 9.5f, 12, 13, 14.5f, 14.5f};
        circle(p, 12, 13, 7.5f);
        poly(p, pts, 3);
        line(p, 4, 5.5f, 6.5f, 3);
        line(p, 20, 5.5f, 17.5f, 3);
        break;
    }
    case NK_ICON_BELL:
        arc(p, 12, 10.5f, 5, 180, 360);
        line(p, 7, 10.5f, 7, 16);
        line(p, 17, 10.5f, 17, 16);
        line(p, 5, 17, 19, 17);
        line(p, 12, 3.5f, 12, 5.5f);
        arc(p, 12, 19, 2, 0, 180);
        break;
    case NK_ICON_PLUG:
        line(p, 9, 3, 9, 7);
        line(p, 15, 3, 15, 7);
        rrect(p, 6, 7, 12, 6, 2);
        arc(p, 12, 13, 4, 0, 180);
        line(p, 12, 17, 12, 21);
        break;
    case NK_ICON_LOGO:
        draw_logo(p);
        break;
    case NK_ICON_NONE:
    default:
        break;
    }
}

void nk_ui_icon_draw(lv_layer_t *layer, nk_icon icon, const lv_area_t *area, lv_color_t color,
                     lv_opa_t opa) {
    pen p;
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    int32_t size = w < h ? w : h;
    p.layer = layer;
    p.s = (float)size / 24.0f;
    p.ox = (float)area->x1 + (float)(w - size) / 2.0f;
    p.oy = (float)area->y1 + (float)(h - size) / 2.0f;
    p.color = color;
    p.opa = opa;
    set_stroke(&p, 1.9f);
    draw_shape(&p, icon);
}

static void icon_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    nk_icon icon = (nk_icon)(intptr_t)lv_obj_get_user_data(obj);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    lv_color_t color = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    lv_opa_t opa = lv_obj_get_style_text_opa(obj, LV_PART_MAIN);
    nk_ui_icon_draw(lv_event_get_layer(e), icon, &area, color, opa);
}

lv_obj_t *nk_ui_icon(lv_obj_t *parent, nk_icon icon, int32_t size, lv_color_t color) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, size, size);
    lv_obj_set_clickable(obj, false);
    lv_obj_set_scrollable(obj, false);
    lv_obj_set_user_data(obj, (void *)(intptr_t)icon);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_add_event_cb(obj, icon_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}

void nk_ui_icon_set_color(lv_obj_t *icon_obj, lv_color_t color) {
    lv_obj_set_style_text_color(icon_obj, color, 0);
}
