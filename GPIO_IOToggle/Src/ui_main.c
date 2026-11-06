/**
 * Top-level UI: swipeable tiles, page indicator, alarm alert overlay
 */
#include "ui_pages.h"
#include "ui_theme.h"
#include "fonts_prompt.h"
#include "thai_text.h"
#include "main.h"
#include <stdio.h>

#define N_PAGES   4

static lv_obj_t *tv;
static lv_obj_t *dots[N_PAGES];
static lv_obj_t *alert, *alert_ring, *alert_title, *alert_time, *alert_hint;
static lv_obj_t *toast;
static lv_obj_t *chime_ring;
static uint32_t chime_since;
static int chime_on;

#define CHIME_MS  10000U
static uint32_t alert_since;
static int alert_on;

/* ------------------------------- helpers --------------------------------- */
void ui_plain(lv_obj_t *o)
{
  lv_obj_remove_style_all(o);
  lv_obj_set_clickable(o, false);
  lv_obj_set_scrollable(o, false);
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, int32_t x, int32_t y)
{
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_label_set_text(l, "");
  lv_obj_align(l, LV_ALIGN_CENTER, x, y);
  return l;
}

void ui_fmt_temp(char *buf, size_t n, int ok, int32_t tenths, int degree_sign)
{
  const char *unit = degree_sign ? "\xC2\xB0" "C" : "C";
  if (!ok) { snprintf(buf, n, "--.-%s", unit); return; }
  int32_t a = tenths < 0 ? -tenths : tenths;
  snprintf(buf, n, "%s%ld.%ld%s", tenths < 0 ? "-" : "", (long)(a / 10), (long)(a % 10), unit);
}

/* ---------------------------- page indicator ----------------------------- */
static void dots_update(void)
{
  lv_obj_t *act = lv_tileview_get_tile_active(tv);
  for (int i = 0; i < N_PAGES; i++)
  {
    int on = (lv_obj_get_index(act) == i);
    lv_obj_set_size(dots[i], on ? 18 : 7, 7);
    lv_obj_set_style_bg_opa(dots[i], on ? LV_OPA_COVER : 90, 0);
  }
}

static void tv_changed_cb(lv_event_t *e)
{
  (void)e;
  dots_update();
}

/* ------------------------------ alarm alert ------------------------------ */
static void alert_pulse_exec(void *var, int32_t v)
{
  lv_obj_t *r = (lv_obj_t *)var;
  int32_t s = 200 + v;                         /* 200..440 px */
  lv_obj_set_size(r, s, s);
  lv_obj_center(r);
  lv_obj_set_style_border_opa(r, (lv_opa_t)(255 - (v * 255) / 240), 0);
}

static void alert_dismiss(void)
{
  if (!alert_on) return;
  alert_on = 0;
  lv_anim_delete(alert_ring, alert_pulse_exec);
  lv_obj_set_hidden(alert, true);
  HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_PIN, GPIO_PIN_SET);
  printf("alarm dismissed\r\n");
}

static void alert_clicked_cb(lv_event_t *e)
{
  (void)e;
  alert_dismiss();
}

void ui_alarm_alert(uint8_t h, uint8_t m)
{
  alert_on = 1;
  alert_since = HAL_GetTick();
  lv_label_set_text_fmt(alert_time, "%02u:%02u", h, m);
  lv_obj_set_hidden(alert, false);
  lv_obj_move_foreground(alert);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, alert_ring);
  lv_anim_set_exec_cb(&a, alert_pulse_exec);
  lv_anim_set_values(&a, 0, 240);
  lv_anim_set_duration(&a, 1200);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
  printf("ALARM %02u:%02u\r\n", h, m);
}

static void alert_create(void)
{
  alert = lv_obj_create(lv_layer_top());
  ui_plain(alert);
  lv_obj_set_size(alert, UI_W, UI_H);
  lv_obj_set_style_bg_color(alert, lv_color_hex(0x140000), 0);
  lv_obj_set_style_bg_opa(alert, 235, 0);
  lv_obj_set_clickable(alert, true);
  lv_obj_add_event_cb(alert, alert_clicked_cb, LV_EVENT_CLICKED, NULL);

  alert_ring = lv_obj_create(alert);
  ui_plain(alert_ring);
  lv_obj_set_style_radius(alert_ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(alert_ring, 6, 0);
  lv_obj_set_style_border_color(alert_ring, lv_color_hex(0xFF3B3B), 0);

  alert_title = ui_label(alert, &font_prompt_28, 0, -90);
  thai_label_set_text(alert_title, "ถึงเวลาแล้ว!");
  alert_time = ui_label(alert, &font_prompt_clock, 0, 0);
  alert_hint = ui_label(alert, &font_prompt_20, 0, 88);
  thai_label_set_text(alert_hint, "แตะหน้าจอเพื่อปิดการปลุก");
  lv_obj_t *ls[] = { alert_title, alert_time, alert_hint };
  for (int i = 0; i < 3; i++) lv_obj_set_style_text_color(ls[i], lv_color_hex(0xFF5050), 0);

  lv_obj_set_hidden(alert, true);
}

/* -------------------------------- theming -------------------------------- */
static void main_theme(lv_color_t acc, lv_color_t bg)
{
  lv_obj_set_style_bg_color(lv_screen_active(), bg, 0);
  lv_obj_set_style_bg_color(tv, bg, 0);
  for (int i = 0; i < N_PAGES; i++) lv_obj_set_style_bg_color(dots[i], acc, 0);
  if (chime_ring) lv_obj_set_style_border_color(chime_ring, acc, 0);
  if (toast)
  {
    lv_obj_set_style_bg_color(toast, bg, 0);
    lv_obj_set_style_text_color(toast, acc, 0);
    lv_obj_set_style_border_color(toast, acc, 0);
    lv_obj_set_style_border_width(toast, 1, 0);
  }
}

static void toast_opa_exec(void *var, int32_t v)
{
  lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

void ui_toast(const char *thai_text)
{
  ui_toast_ms(thai_text, 1200);
}

void ui_toast_ms(const char *thai_text, uint32_t hold_ms)
{
  thai_label_set_text(toast, thai_text);
  lv_anim_delete(toast, toast_opa_exec);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, toast);
  lv_anim_set_exec_cb(&a, toast_opa_exec);
  lv_anim_set_values(&a, LV_OPA_COVER, 0);
  lv_anim_set_delay(&a, hold_ms);
  lv_anim_set_duration(&a, 500);
  lv_anim_start(&a);
  lv_obj_set_style_opa(toast, LV_OPA_COVER, 0);
}

/* --------------------------------- API ----------------------------------- */
void ui_create(void)
{
  theme_init();

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  tv = lv_tileview_create(scr);
  lv_obj_set_size(tv, UI_W, UI_H);
  lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(tv, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(tv, tv_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *t0 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
  lv_obj_t *t1 = lv_tileview_add_tile(tv, 1, 0, LV_DIR_HOR);
  lv_obj_t *t2 = lv_tileview_add_tile(tv, 2, 0, LV_DIR_HOR);
  lv_obj_t *t3 = lv_tileview_add_tile(tv, 3, 0, LV_DIR_LEFT);

  radar_create(t0);
  templog_create(t1);
  chrono_create(t2);
  alarm_page_create(t3);

  /* page indicator dots */
  lv_obj_t *bar = lv_obj_create(lv_layer_top());
  ui_plain(bar);
  lv_obj_set_size(bar, 90, 12);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bar, 6, 0);
  for (int i = 0; i < N_PAGES; i++)
  {
    dots[i] = lv_obj_create(bar);
    ui_plain(dots[i]);
    lv_obj_set_style_radius(dots[i], 4, 0);
  }

  alert_create();

  /* toast: short message that fades out (e.g. theme name) */
  toast = lv_label_create(lv_layer_top());
  lv_obj_set_style_text_font(toast, &font_prompt_20, 0);
  lv_obj_set_style_bg_opa(toast, 200, 0);
  lv_obj_set_style_radius(toast, 16, 0);
  lv_obj_set_style_pad_hor(toast, 16, 0);
  lv_obj_set_style_pad_ver(toast, 4, 0);
  lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, 62);
  lv_obj_set_style_opa(toast, 0, 0);

  /* hourly chime: thick rim that pulses for 10 s (touches pass through) */
  chime_ring = lv_obj_create(lv_layer_top());
  ui_plain(chime_ring);
  lv_obj_set_size(chime_ring, UI_W, UI_H);
  lv_obj_set_style_radius(chime_ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(chime_ring, 18, 0);
  lv_obj_set_style_border_opa(chime_ring, LV_OPA_COVER, 0);
  lv_obj_set_hidden(chime_ring, true);

  theme_register(main_theme);
  dots_update();
}

void ui_tick(const rtc_datetime_t *dt)
{
  /* Thai colour of the day (สีประจำวัน) */
  static int shown_boot_toast;
  int r = theme_follow_day(dt->weekday);
  if (r != 0 || !shown_boot_toast)
  {
    shown_boot_toast = 1;
    char buf[80];
    snprintf(buf, sizeof(buf), "%s%s", (r == 2) ? "กลับเป็น" : "วันนี้ ", theme_name());
    ui_toast(buf);
  }

  radar_tick(dt);
  chrono_tick();

  if (chime_on)
  {
    HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_PIN,
                      ((HAL_GetTick() / 250U) & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if (HAL_GetTick() - chime_since > CHIME_MS) (void)ui_chime_stop();
  }

  if (alert_on)
  {
    /* blink LED3 and auto-dismiss after 60 s */
    HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_PIN,
                      ((HAL_GetTick() / 250U) & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if (HAL_GetTick() - alert_since > 60000U) alert_dismiss();
  }
}

void ui_set_temperature(int ok, int32_t tenths)
{
  radar_set_temperature(ok, tenths);
}

int ui_busy(void)
{
  return chrono_running() || alert_on || chime_on;
}

/* on/off blink (no fading): a fading full-screen ring forces LVGL to render the
 * whole 480x480 area through an intermediate layer every frame (fps 24 -> 8). */
static lv_timer_t *chime_timer;

static void chime_blink_cb(lv_timer_t *t)
{
  (void)t;
  lv_obj_set_hidden(chime_ring, !lv_obj_is_hidden(chime_ring));
}

void ui_chime(uint8_t hour)
{
  if (alert_on) return;                      /* the alarm overlay has priority */
  chime_on = 1;
  chime_since = HAL_GetTick();
  lv_obj_set_hidden(chime_ring, false);
  if (chime_timer == NULL) chime_timer = lv_timer_create(chime_blink_cb, 250, NULL);  /* 2 Hz */
  lv_timer_resume(chime_timer);

  char buf[48];
  snprintf(buf, sizeof(buf), "ตีบอกเวลา %02u:00 น.", hour);
  ui_toast_ms(buf, CHIME_MS);
  lv_display_trigger_activity(NULL);
  printf("chime %02u:00\r\n", hour);
}

int ui_chime_stop(void)
{
  if (!chime_on) return 0;
  chime_on = 0;
  if (chime_timer) lv_timer_pause(chime_timer);
  lv_obj_set_hidden(chime_ring, true);
  lv_obj_set_style_opa(toast, 0, 0);
  HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_PIN, GPIO_PIN_SET);
  return 1;
}

void ui_redraw_all(void)
{
  radar_intro();
  lv_obj_invalidate(lv_screen_active());
  lv_obj_invalidate(lv_layer_top());
}
