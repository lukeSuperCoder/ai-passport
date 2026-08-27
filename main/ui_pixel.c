#include "ui_pixel.h"

#include <string.h>

LV_FONT_DECLARE(lv_font_cjk_12)

static lv_font_t s_font_14;
static lv_font_t s_font_20;
static lv_font_t s_cjk_title;
static bool s_fonts_ready;

/* 点阵放大共用暂存。LVGL 单线程绘制,CJK 2x 与时钟 4x 不会重入。 */
static uint8_t s_scale_src[64 * 24];

/* 12px 点阵放大一倍给 20px 标题用。必须返回 lv_draw_buf_t*,不能返回像素指针,
 * 否则 LVGL9 会把像素当 draw_buf 解引用 → 黑屏。内层 bitmap 也必须走
 * lv_font_cjk_12(fmt_txt),不能再指向 s_cjk_title,否则递归。 */
static bool cjk_title_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc,
                          uint32_t letter, uint32_t next)
{
    (void)font;
    if (!lv_font_cjk_12.get_glyph_dsc ||
        !lv_font_cjk_12.get_glyph_dsc(&lv_font_cjk_12, dsc, letter, next)) {
        return false;
    }
    dsc->adv_w = (uint16_t)(dsc->adv_w * 2);
    dsc->box_w = (uint16_t)(dsc->box_w * 2);
    dsc->box_h = (uint16_t)(dsc->box_h * 2);
    dsc->ofs_x = (int16_t)(dsc->ofs_x * 2);
    dsc->ofs_y = (int16_t)(dsc->ofs_y * 2);
    dsc->stride = 0;
    dsc->format = LV_FONT_GLYPH_FORMAT_A8;
    dsc->resolved_font = &s_cjk_title;
    return true;
}

static const void *cjk_title_bitmap(lv_font_glyph_dsc_t *g_dsc, lv_draw_buf_t *draw_buf)
{
    if (!g_dsc || !draw_buf || !draw_buf->data || !g_dsc->gid.index) return NULL;
    if (g_dsc->box_w < 2 || g_dsc->box_h < 2) return NULL;

    uint16_t sw = (uint16_t)(g_dsc->box_w / 2);
    uint16_t sh = (uint16_t)(g_dsc->box_h / 2);
    uint32_t stride_in = lv_draw_buf_width_to_stride(sw, LV_COLOR_FORMAT_A8);
    if (stride_in == 0 || (uint32_t)sh * stride_in > sizeof(s_scale_src)) return NULL;

    lv_draw_buf_t src_buf;
    if (lv_draw_buf_init(&src_buf, sw, sh, LV_COLOR_FORMAT_A8,
                         stride_in, s_scale_src, sizeof(s_scale_src)) != LV_RESULT_OK) {
        return NULL;
    }

    lv_font_glyph_dsc_t src = *g_dsc;
    src.resolved_font = &lv_font_cjk_12;
    src.req_raw_bitmap = 0;
    src.box_w = sw;
    src.box_h = sh;
    src.stride = 0;
    src.format = LV_FONT_GLYPH_FORMAT_A1;
    if (!lv_font_cjk_12.get_glyph_bitmap(&src, &src_buf)) return NULL;

    uint32_t stride_out = draw_buf->header.stride;
    if (stride_out == 0) stride_out = g_dsc->box_w;
    if (draw_buf->data_size &&
        (uint32_t)g_dsc->box_h * stride_out > draw_buf->data_size) {
        return NULL;
    }

    const uint8_t *in = src_buf.data;
    uint8_t *out = draw_buf->data;
    for (uint16_t y = 0; y < g_dsc->box_h; y++) {
        const uint8_t *row = in + (y / 2) * stride_in;
        uint8_t *dst = out + y * stride_out;
        for (uint16_t x = 0; x < g_dsc->box_w; x++) dst[x] = row[x / 2];
    }
    return draw_buf;
}

#define CLOCK_SCALE 4

static int clock4x_advance(uint32_t letter, uint32_t next)
{
    lv_font_glyph_dsc_t dsc;
    if (!lv_font_montserrat_20.get_glyph_dsc(&lv_font_montserrat_20, &dsc, letter, next)) {
        return 8 * CLOCK_SCALE;
    }
    return (int)dsc.adv_w * CLOCK_SCALE;
}

void ui_pixel_draw_clock4x(lv_layer_t *layer, const char *txt, const lv_area_t *box,
                           uint32_t color)
{
    if (!layer || !txt || !txt[0] || !box) return;

    int text_w = 0;
    for (const char *p = txt; *p; p++) {
        text_w += clock4x_advance((uint8_t)*p, (uint8_t)p[1]);
    }
    int box_w = (int)lv_area_get_width(box);
    int pen = box->x1;
    if (box_w > text_w) pen += (box_w - text_w) / 2;
    const int y = box->y1;
    const int line_h = lv_font_montserrat_20.line_height;
    const int base = lv_font_montserrat_20.base_line;

    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = lv_color_hex(color);
    rd.radius = 0;
    rd.border_width = 0;
    rd.outline_width = 0;
    rd.shadow_width = 0;

    for (const char *p = txt; *p; p++) {
        uint32_t letter = (uint8_t)*p;
        uint32_t next = (uint8_t)p[1];
        lv_font_glyph_dsc_t dsc;
        if (!lv_font_montserrat_20.get_glyph_dsc(&lv_font_montserrat_20, &dsc,
                                                 letter, next) ||
            !dsc.gid.index) {
            pen += clock4x_advance(letter, next);
            continue;
        }

        uint16_t sw = dsc.box_w;
        uint16_t sh = dsc.box_h;
        uint32_t stride = lv_draw_buf_width_to_stride(sw, LV_COLOR_FORMAT_A8);
        if (stride == 0 || (uint32_t)sh * stride > sizeof(s_scale_src)) {
            pen += (int)dsc.adv_w * CLOCK_SCALE;
            continue;
        }

        lv_draw_buf_t src_buf;
        if (lv_draw_buf_init(&src_buf, sw, sh, LV_COLOR_FORMAT_A8,
                             stride, s_scale_src, sizeof(s_scale_src)) != LV_RESULT_OK) {
            pen += (int)dsc.adv_w * CLOCK_SCALE;
            continue;
        }
        dsc.resolved_font = &lv_font_montserrat_20;
        dsc.req_raw_bitmap = 0;
        dsc.stride = 0;
        dsc.format = LV_FONT_GLYPH_FORMAT_A8;
        if (!lv_font_montserrat_20.get_glyph_bitmap(&dsc, &src_buf)) {
            pen += (int)dsc.adv_w * CLOCK_SCALE;
            continue;
        }

        int gx = pen + dsc.ofs_x * CLOCK_SCALE;
        int gy = y + (line_h - base - dsc.box_h - dsc.ofs_y) * CLOCK_SCALE;
        const uint8_t *in = src_buf.data;
        for (uint16_t sy = 0; sy < sh; sy++) {
            const uint8_t *row = in + sy * stride;
            int run = -1;
            uint8_t run_a = 0;
            for (int sx = 0; sx <= (int)sw; sx++) {
                uint8_t a = (sx < (int)sw) ? row[sx] : 0;
                if (a < 24) a = 0;
                if (a && run < 0) {
                    run = sx;
                    run_a = a;
                } else if (run >= 0 &&
                           (a == 0 || a + 48 < run_a || run_a + 48 < a)) {
                    rd.bg_opa = run_a;
                    lv_area_t ar = {
                        .x1 = gx + run * CLOCK_SCALE,
                        .y1 = gy + sy * CLOCK_SCALE,
                        .x2 = gx + sx * CLOCK_SCALE - 1,
                        .y2 = gy + (sy + 1) * CLOCK_SCALE - 1,
                    };
                    lv_draw_rect(layer, &rd, &ar);
                    run = a ? sx : -1;
                    run_a = a;
                }
            }
        }
        pen += (int)dsc.adv_w * CLOCK_SCALE;
    }
}

void ui_pixel_fonts_init(void)
{
    if (s_fonts_ready) return;
    s_font_14 = lv_font_montserrat_14;
    s_font_14.fallback = &lv_font_cjk_12;

    s_cjk_title = lv_font_cjk_12;
    s_cjk_title.get_glyph_dsc = cjk_title_dsc;
    s_cjk_title.get_glyph_bitmap = cjk_title_bitmap;
    s_cjk_title.release_glyph = NULL;
    s_cjk_title.line_height = 28;
    s_cjk_title.base_line = 6;
    s_cjk_title.static_bitmap = 0;
    s_cjk_title.fallback = NULL;

    s_font_20 = lv_font_montserrat_20;
    s_font_20.line_height = 28;
    s_font_20.fallback = &s_cjk_title;
    s_fonts_ready = true;
}

const lv_font_t *ui_pixel_font_14(void)
{
    return s_fonts_ready ? &s_font_14 : &lv_font_montserrat_14;
}

const lv_font_t *ui_pixel_font_20(void)
{
    return s_fonts_ready ? &s_font_20 : &lv_font_montserrat_20;
}

void ui_pixel_utf8_copy(char *dst, size_t dst_n, const char *src)
{
    if (!dst || dst_n == 0) return;
    dst[0] = 0;
    if (!src) return;

    size_t o = 0;
    for (size_t i = 0; src[i] && o + 1 < dst_n; ) {
        unsigned char c = (unsigned char)src[i];
        int w = 1;
        if ((c & 0x80) == 0) w = 1;
        else if ((c & 0xE0) == 0xC0) w = 2;
        else if ((c & 0xF0) == 0xE0) w = 3;
        else if ((c & 0xF8) == 0xF0) w = 4;
        if (o + (size_t)w >= dst_n) break;
        memcpy(dst + o, src + i, (size_t)w);
        o += (size_t)w;
        i += (size_t)w;
    }
    dst[o] = 0;
}

static void start_blink(lv_obj_t *eye);

void ui_pixel_strip_theme(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    ui_pixel_strip_theme(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

lv_obj_t *ui_pixel_label(lv_obj_t *parent, const char *text,
                         const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static void add_cloud(lv_obj_t *parent, int x, int y)
{
    block(parent, x + 1, y + 7, 43, 10, UI_INK);
    block(parent, x + 5, y + 4, 35, 10, 0xFFFFFF);
    block(parent, x + 12, y, 10, 9, 0xFFFFFF);
    block(parent, x + 27, y + 1, 9, 8, 0xFFFFFF);
}

lv_obj_t *ui_pixel_screen_create(const char *title)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    ui_pixel_strip_theme(scr);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_SKY), 0);
    lv_obj_set_style_text_font(scr, ui_pixel_font_14(), 0);

    add_cloud(scr, 188, 8);
    block(scr, 0, 286, 240, 34, UI_GRASS);
    block(scr, 0, 286, 240, 4, 0xA7D93E);
    for (int x = 0; x < 240; x += 30) {
        block(scr, x, 312, 18, 8, UI_GRASS_DARK);
        block(scr, x + 18, 316, 12, 4, 0x75452E);
    }

    block(scr, 9, 12, 151, 33, UI_INK);
    lv_obj_t *plate = block(scr, 5, 8, 151, 33, UI_PAPER);
    lv_obj_set_style_border_color(plate, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_border_width(plate, 3, 0);
    lv_obj_t *heading = ui_pixel_label(plate, title, ui_pixel_font_20(), UI_INK);
    lv_obj_center(heading);
    return scr;
}

lv_obj_t *ui_pixel_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t color)
{
    block(parent, x + 5, y + 6, w, h, UI_INK);
    lv_obj_t *panel = block(parent, x, y, w, h, color);
    lv_obj_set_style_border_color(panel, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_border_width(panel, 4, 0);
    lv_obj_set_style_pad_all(panel, 7, 0);
    return panel;
}

lv_obj_t *ui_pixel_mascot_create(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *m = lv_obj_create(parent);
    ui_pixel_strip_theme(m);
    lv_obj_set_pos(m, x, y);
    lv_obj_set_size(m, 38, 48);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);

    /* 原创“小电视机器人”：天线、发光屏幕脸、橙色围巾与履带脚。 */
    block(m, 18, 0, 3, 6, UI_INK);
    block(m, 16, 0, 7, 3, UI_ORANGE);
    block(m, 3, 6, 32, 24, UI_INK);
    block(m, 0, 12, 5, 10, 0x7557D9);
    block(m, 33, 12, 5, 10, 0x7557D9);
    block(m, 7, 10, 24, 16, 0xB9F3FF);
    lv_obj_t *left_eye = block(m, 11, 14, 4, 6, 0x294B7A);
    lv_obj_t *right_eye = block(m, 23, 14, 4, 6, 0x294B7A);
    block(m, 16, 22, 7, 2, 0x7557D9);
    block(m, 10, 29, 18, 4, UI_ORANGE);
    block(m, 8, 33, 22, 11, 0x7557D9);
    block(m, 3, 35, 5, 7, 0xB9F3FF);
    block(m, 30, 35, 5, 7, 0xB9F3FF);
    block(m, 8, 44, 9, 4, UI_INK);
    block(m, 21, 44, 9, 4, UI_INK);
    start_blink(left_eye);
    start_blink(right_eye);
    return m;
}

static void jump_y(void *obj, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)obj, value);
}

static void blink_eye(void *obj, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void start_blink(lv_obj_t *eye)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, eye);
    lv_anim_set_exec_cb(&anim, blink_eye);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_20);
    lv_anim_set_duration(&anim, 70);
    lv_anim_set_playback_duration(&anim, 70);
    lv_anim_set_repeat_delay(&anim, 1700);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&anim, lv_anim_path_step);
    lv_anim_start(&anim);
}

void ui_pixel_mascot_jump(lv_obj_t *mascot)
{
    if (!mascot) return;
    int y = lv_obj_get_y(mascot);
    lv_anim_delete(mascot, jump_y);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, mascot);
    lv_anim_set_exec_cb(&anim, jump_y);
    lv_anim_set_values(&anim, y, y - 5);
    lv_anim_set_duration(&anim, 110);
    lv_anim_set_playback_duration(&anim, 140);
    lv_anim_set_path_cb(&anim, lv_anim_path_step);
    lv_anim_start(&anim);
}

void ui_pixel_set_selected(lv_obj_t *panel, bool selected, bool enabled)
{
    uint32_t color = !enabled ? 0x78909C : (selected ? UI_YELLOW : UI_PAPER);
    lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
    lv_obj_set_style_border_color(panel,
        lv_color_hex(selected ? 0xFFFFFF : UI_INK), 0);
}
