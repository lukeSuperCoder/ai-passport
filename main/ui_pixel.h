#pragma once

#include "lvgl.h"
#include <stddef.h>

#define UI_SKY        0x1689E8
#define UI_SKY_DARK   0x0872C9
#define UI_INK        0x17202A
#define UI_PAPER      0xF4F4EA
#define UI_GRASS      0x82BE2D
#define UI_GRASS_DARK 0x55951D
#define UI_YELLOW     0xFFD928
#define UI_ORANGE     0xFFB23E
#define UI_RED        0xE43B2F
#define UI_MUTED      0xD9E7EC

// 去掉 LVGL 默认 theme 的白底卡片/圆角/内边距,像素风控件必须先调这个。
void ui_pixel_strip_theme(lv_obj_t *obj);

lv_obj_t *ui_pixel_screen_create(const char *title);
lv_obj_t *ui_pixel_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t color);
lv_obj_t *ui_pixel_label(lv_obj_t *parent, const char *text,
                         const lv_font_t *font, uint32_t color);
lv_obj_t *ui_pixel_mascot_create(lv_obj_t *parent, int x, int y);
void ui_pixel_mascot_jump(lv_obj_t *mascot);
void ui_pixel_set_selected(lv_obj_t *panel, bool selected, bool enabled);

// Montserrat + CJK/假名/全角回退。显示初始化后、创建界面前调用。
void ui_pixel_fonts_init(void);
const lv_font_t *ui_pixel_font_14(void);
const lv_font_t *ui_pixel_font_20(void);

// 按完整 UTF-8 码点拷贝,避免中文 SSID 被截断成非法序列。
void ui_pixel_utf8_copy(char *dst, size_t dst_n, const char *src);
