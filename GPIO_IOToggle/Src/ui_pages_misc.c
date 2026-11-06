/**
 * THERMAL (temperature log), CHRONO (stopwatch) and ALARM pages
 */
#include "ui_pages.h"
#include "ui_theme.h"
#include "fonts_prompt.h"
#include "thai_text.h"
#include "main.h"
#include <stdio.h>

/* ========================================================================= */
/*                               THERMAL LOG                                 */
/* ========================================================================= */
#define LOG_POINTS   120          /* 120 x 10 s = 20 minutes */

static lv_obj_t *tl_tile, *tl_title, *tl_now, *tl_chart, *tl_stats, *tl_axis;
static lv_chart_series_t *tl_ser;
static int32_t tl_min = INT32_MAX, tl_max = INT32_MIN;
static uint32_t tl_count;

static void templog_theme(lv_color_t acc, lv_color_t bg)
{
  lv_obj_set_style_bg_color(tl_tile, bg, 0);
  lv_obj_set_style_text_color(tl_title, acc, 0);
  lv_obj_set_style_text_color(tl_now, acc, 0);
  lv_obj_set_style_text_color(tl_stats, acc, 0);
  lv_obj_set_style_text_color(tl_axis, acc, 0);
  lv_obj_set_style_line_color(tl_chart, acc, LV_PART_MAIN);         /* grid */
  lv_obj_set_style_border_color(tl_chart, acc, 0);
  lv_chart_set_series_color(tl_chart, tl_ser, acc);
}

void templog_create(lv_obj_t *tile)
{
  tl_tile = tile;
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);

  tl_title = ui_label(tile, &font_prompt_28, 0, -152);
  thai_label_set_text(tl_title, "บันทึกอุณหภูมิ");
  tl_now = ui_label(tile, &font_prompt_56, 0, -96);

  tl_chart = lv_chart_create(tile);
  lv_obj_set_size(tl_chart, 320, 160);
  lv_obj_align(tl_chart, LV_ALIGN_CENTER, 0, 20);
  lv_chart_set_type(tl_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(tl_chart, LOG_POINTS);
  lv_chart_set_div_line_count(tl_chart, 5, 7);
  lv_chart_set_axis_range(tl_chart, LV_CHART_AXIS_PRIMARY_Y, 250, 350);
  lv_obj_set_style_bg_opa(tl_chart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tl_chart, 1, 0);
  lv_obj_set_style_border_opa(tl_chart, 120, 0);
  lv_obj_set_style_radius(tl_chart, 0, 0);
  lv_obj_set_style_line_opa(tl_chart, 45, LV_PART_MAIN);
  lv_obj_set_style_line_width(tl_chart, 2, LV_PART_ITEMS);
  lv_obj_set_style_width(tl_chart, 0, LV_PART_INDICATOR);           /* no point markers */
  lv_obj_set_style_height(tl_chart, 0, LV_PART_INDICATOR);
  lv_obj_set_clickable(tl_chart, false);
  tl_ser = lv_chart_add_series(tl_chart, lv_color_white(), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_values(tl_chart, tl_ser, LV_CHART_POINT_NONE);

  tl_axis = ui_label(tile, &font_prompt_16, 0, 114);
  thai_label_set_text(tl_axis, "20 นาทีก่อน                         ล่าสุด");
  lv_obj_set_style_text_opa(tl_axis, 160, 0);
  tl_stats = ui_label(tile, &font_prompt_20, 0, 146);
  thai_label_set_text(tl_stats, "ต่ำสุด --.-  สูงสุด --.-");

  theme_register(templog_theme);
}

void templog_add(int ok, int32_t tenths)
{
  char buf[16];
  ui_fmt_temp(buf, sizeof(buf), ok, tenths, 1);
  lv_label_set_text(tl_now, buf);
  if (!ok) return;

  lv_chart_set_next_value(tl_chart, tl_ser, tenths);
  tl_count++;
  if (tenths < tl_min) tl_min = tenths;
  if (tenths > tl_max) tl_max = tenths;

  /* auto-scale Y with 0.5 C margin, at least 2 C span */
  int32_t lo = tl_min - 5, hi = tl_max + 5;
  if (hi - lo < 20) { int32_t mid = (lo + hi) / 2; lo = mid - 10; hi = mid + 10; }
  lv_chart_set_axis_range(tl_chart, LV_CHART_AXIS_PRIMARY_Y, lo, hi);

  char a[12], b[12];
  ui_fmt_temp(a, sizeof(a), 1, tl_min, 0);
  ui_fmt_temp(b, sizeof(b), 1, tl_max, 0);
  thai_label_set_fmt(tl_stats, "ต่ำสุด %s   สูงสุด %s", a, b);
}

/* ========================================================================= */
/*                                 CHRONO                                    */
/* ========================================================================= */
static lv_obj_t *ch_tile, *ch_title, *ch_ring, *ch_time, *ch_cs, *ch_btn_go, *ch_btn_rst;
static lv_obj_t *ch_lbl_go, *ch_lbl_rst;
static uint32_t ch_start, ch_acc;          /* ms */
static int ch_run;
static uint32_t ch_last_shown = UINT32_MAX;

static uint32_t chrono_ms(void)
{
  return ch_acc + (ch_run ? HAL_GetTick() - ch_start : 0U);
}

static void chrono_show(void)
{
  uint32_t ms = chrono_ms();
  uint32_t cs = ms / 10U;
  if (cs == ch_last_shown) return;
  ch_last_shown = cs;
  uint32_t s = ms / 1000U;
  /* big digits: MM:SS (font has digits and ':' only); hours go to the sub line */
  lv_label_set_text_fmt(ch_time, "%02lu:%02lu", (unsigned long)((s / 60U) % 60U), (unsigned long)(s % 60U));
  if (s >= 3600U)
    thai_label_set_fmt(ch_cs, "%lu ชม.  .%02lu", (unsigned long)(s / 3600U), (unsigned long)(cs % 100U));
  else
    lv_label_set_text_fmt(ch_cs, ".%02lu", (unsigned long)(cs % 100U));
  lv_arc_set_value(ch_ring, (int32_t)(ms % 60000U) / 100);
}

static void chrono_theme(lv_color_t acc, lv_color_t bg)
{
  lv_obj_set_style_bg_color(ch_tile, bg, 0);
  lv_obj_set_style_text_color(ch_title, acc, 0);
  lv_obj_set_style_text_color(ch_time, lv_color_mix(lv_color_white(), acc, 90), 0);
  lv_obj_set_style_text_color(ch_cs, acc, 0);
  lv_obj_set_style_arc_color(ch_ring, acc, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(ch_ring, lv_color_mix(acc, bg, 50), LV_PART_MAIN);
  lv_obj_set_style_bg_color(ch_btn_go, acc, 0);
  lv_obj_set_style_text_color(ch_lbl_go, bg, 0);
  lv_obj_set_style_border_color(ch_btn_rst, acc, 0);
  lv_obj_set_style_text_color(ch_lbl_rst, acc, 0);
}

static void chrono_go_cb(lv_event_t *e)
{
  (void)e;
  if (ch_run) { ch_acc += HAL_GetTick() - ch_start; ch_run = 0; }
  else        { ch_start = HAL_GetTick(); ch_run = 1; }
  thai_label_set_text(ch_lbl_go, ch_run ? "หยุด" : "เริ่ม");
  ch_last_shown = UINT32_MAX;
  chrono_show();
}

static void chrono_rst_cb(lv_event_t *e)
{
  (void)e;
  ch_acc = 0; ch_start = HAL_GetTick();
  ch_last_shown = UINT32_MAX;
  chrono_show();
}

static lv_obj_t *make_btn(lv_obj_t *parent, int32_t x, int32_t y, const char *txt, lv_obj_t **lbl,
                          lv_event_cb_t cb)
{
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_size(b, 120, 46);
  lv_obj_align(b, LV_ALIGN_CENTER, x, y);
  lv_obj_set_style_radius(b, 23, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
  *lbl = lv_label_create(b);
  lv_obj_set_style_text_font(*lbl, &font_prompt_20, 0);
  thai_label_set_text(*lbl, txt);
  lv_obj_center(*lbl);
  return b;
}

void chrono_create(lv_obj_t *tile)
{
  ch_tile = tile;
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);

  ch_ring = lv_arc_create(tile);
  lv_obj_set_size(ch_ring, 440, 440);
  lv_obj_center(ch_ring);
  lv_arc_set_rotation(ch_ring, 270);
  lv_arc_set_bg_angles(ch_ring, 0, 360);
  lv_arc_set_range(ch_ring, 0, 600);
  lv_arc_set_value(ch_ring, 0);
  lv_obj_set_style_arc_width(ch_ring, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ch_ring, 6, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(ch_ring, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_pad_all(ch_ring, 0, LV_PART_KNOB);
  lv_obj_set_clickable(ch_ring, false);

  ch_title = ui_label(tile, &font_prompt_28, 0, -128);
  thai_label_set_text(ch_title, "จับเวลา");
  ch_time = ui_label(tile, &font_prompt_clock, 0, -30);
  ch_cs = ui_label(tile, &font_prompt_28, 0, 36);

  ch_btn_go  = make_btn(tile, -68, 104, "เริ่ม", &ch_lbl_go, chrono_go_cb);
  ch_btn_rst = make_btn(tile, 68, 104, "รีเซ็ต", &ch_lbl_rst, chrono_rst_cb);
  lv_obj_set_style_bg_opa(ch_btn_rst, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ch_btn_rst, 2, 0);

  theme_register(chrono_theme);
  chrono_show();
}

void chrono_tick(void)
{
  if (ch_run) chrono_show();
}

int chrono_running(void)
{
  return ch_run;
}

/* ========================================================================= */
/*                                  ALARM                                    */
/* ========================================================================= */
static lv_obj_t *al_tile, *al_title, *al_roll_h, *al_roll_m, *al_colon, *al_sw, *al_state;
static lv_obj_t *al_chime_sw, *al_lbl_alarm, *al_lbl_chime;

static void alarm_apply(void)
{
  uint8_t h = (uint8_t)lv_roller_get_selected(al_roll_h);
  uint8_t m = (uint8_t)lv_roller_get_selected(al_roll_m);
  int en = lv_obj_has_state(al_sw, LV_STATE_CHECKED);
  rtc_alarm_set(h, m, en);
  if (en) thai_label_set_fmt(al_state, "ปลุกทุกวัน เวลา %02u:%02u น.", h, m);
  else    thai_label_set_text(al_state, "ปิดการปลุกอยู่");
}

static void alarm_changed_cb(lv_event_t *e)
{
  (void)e;
  alarm_apply();
}

static void chime_changed_cb(lv_event_t *e)
{
  (void)e;
  int on = lv_obj_has_state(al_chime_sw, LV_STATE_CHECKED);
  rtc_chime_set(on);
  ui_toast(on ? "เปิดตีบอกเวลาทุกชั่วโมง" : "ปิดตีบอกเวลา");
}

static void alarm_theme(lv_color_t acc, lv_color_t bg)
{
  lv_obj_set_style_bg_color(al_tile, bg, 0);
  lv_obj_set_style_text_color(al_title, acc, 0);
  lv_obj_set_style_text_color(al_colon, acc, 0);
  lv_obj_set_style_text_color(al_state, acc, 0);
  lv_obj_set_style_text_color(al_lbl_alarm, acc, 0);
  lv_obj_set_style_text_color(al_lbl_chime, acc, 0);
  lv_obj_t *rolls[] = { al_roll_h, al_roll_m };
  for (int i = 0; i < 2; i++)
  {
    lv_obj_set_style_bg_color(rolls[i], bg, 0);
    lv_obj_set_style_border_color(rolls[i], acc, 0);
    lv_obj_set_style_text_color(rolls[i], lv_color_mix(acc, bg, 110), 0);
    lv_obj_set_style_bg_color(rolls[i], acc, LV_PART_SELECTED);
    lv_obj_set_style_text_color(rolls[i], bg, LV_PART_SELECTED);
  }
  lv_obj_t *sws[] = { al_sw, al_chime_sw };
  for (int i = 0; i < 2; i++)
  {
    lv_obj_set_style_bg_color(sws[i], lv_color_mix(acc, bg, 60), 0);
    lv_obj_set_style_bg_color(sws[i], acc, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sws[i], lv_color_white(), LV_PART_KNOB);
  }
}

static lv_obj_t *make_roller(lv_obj_t *parent, const char *opts, int32_t x)
{
  lv_obj_t *r = lv_roller_create(parent);
  lv_roller_set_options(r, opts, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(r, 3);
  lv_obj_set_width(r, 96);
  lv_obj_align(r, LV_ALIGN_CENTER, x, -34);
  lv_obj_set_style_text_font(r, &font_prompt_28, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_opa(r, 120, 0);
  lv_obj_set_style_radius(r, 8, 0);
  lv_obj_add_event_cb(r, alarm_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
  return r;
}

void alarm_page_create(lv_obj_t *tile)
{
  static char opts_h[24 * 3 + 1], opts_m[60 * 3 + 1];
  char *p = opts_h;
  for (int i = 0; i < 24; i++) p += sprintf(p, i ? "\n%02d" : "%02d", i);
  p = opts_m;
  for (int i = 0; i < 60; i++) p += sprintf(p, i ? "\n%02d" : "%02d", i);

  al_tile = tile;
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  al_title = ui_label(tile, &font_prompt_28, 0, -150);
  thai_label_set_text(al_title, "นาฬิกาปลุก");

  al_roll_h = make_roller(tile, opts_h, -62);
  al_roll_m = make_roller(tile, opts_m, 62);
  al_colon = ui_label(tile, &font_prompt_28, 0, -36);
  lv_label_set_text(al_colon, ":");

  /* row 1: daily alarm on/off */
  al_lbl_alarm = ui_label(tile, &font_prompt_20, -56, 72);
  thai_label_set_text(al_lbl_alarm, "ปลุก");
  al_sw = lv_switch_create(tile);
  lv_obj_set_size(al_sw, 64, 34);
  lv_obj_align(al_sw, LV_ALIGN_CENTER, 46, 72);
  lv_obj_add_event_cb(al_sw, alarm_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

  /* row 2: hourly chime on/off */
  al_lbl_chime = ui_label(tile, &font_prompt_20, -56, 116);
  thai_label_set_text(al_lbl_chime, "ตีบอกเวลา");
  al_chime_sw = lv_switch_create(tile);
  lv_obj_set_size(al_chime_sw, 64, 34);
  lv_obj_align(al_chime_sw, LV_ALIGN_CENTER, 46, 116);
  if (rtc_chime_enabled()) lv_obj_add_state(al_chime_sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(al_chime_sw, chime_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

  al_state = ui_label(tile, &font_prompt_16, 0, 160);

  /* restore current alarm */
  uint8_t h, m; int en;
  rtc_alarm_get(&h, &m, &en);
  lv_roller_set_selected(al_roll_h, h, LV_ANIM_OFF);
  lv_roller_set_selected(al_roll_m, m, LV_ANIM_OFF);
  if (en) lv_obj_add_state(al_sw, LV_STATE_CHECKED);
  if (en) thai_label_set_fmt(al_state, "ปลุกทุกวัน เวลา %02u:%02u น.", h, m);
  else    thai_label_set_text(al_state, "ปิดการปลุกอยู่");

  theme_register(alarm_theme);
}
