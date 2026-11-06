/**
 * Sci-fi radar watch UI: pages and shared helpers
 *
 *   tile 0  RADAR      sweep = seconds, blips = hour / minute, ambient contacts
 *   tile 1  THERMAL    temperature log (lv_chart, 20 min history)
 *   tile 2  CHRONO     stopwatch
 *   tile 3  ALARM      daily alarm (RTC Alarm A)
 *   overlay            alarm alert (lv_layer_top)
 */
#ifndef UI_PAGES_H
#define UI_PAGES_H

#include "lvgl.h"
#include "rtc_clock.h"

#define UI_W   480
#define UI_H   480
#define UI_CX  240
#define UI_CY  240

/* ---- top level (ui_main.c) ---- */
void ui_create(void);
void ui_tick(const rtc_datetime_t *dt);            /* call ~30 Hz            */
void ui_set_temperature(int ok, int32_t tenths);   /* call when re-sampled   */
int  ui_busy(void);                                /* 1 = don't go to sleep  */
void ui_alarm_alert(uint8_t h, uint8_t m);         /* show alarm overlay     */
void ui_redraw_all(void);                          /* after wake-up          */
void ui_toast(const char *thai_text);              /* short message, fades   */
void ui_toast_ms(const char *thai_text, uint32_t hold_ms);
void ui_chime(uint8_t hour);                       /* hourly chime: 10 s flash */
int  ui_chime_stop(void);                          /* 1 if a chime was stopped */

/* ---- pages ---- */
void radar_create(lv_obj_t *tile);
void radar_tick(const rtc_datetime_t *dt);
void radar_set_temperature(int ok, int32_t tenths);
void radar_intro(void);                            /* boot / wake animation  */

void templog_create(lv_obj_t *tile);
void templog_add(int ok, int32_t tenths);          /* every 10 s */

void chrono_create(lv_obj_t *tile);
void chrono_tick(void);
int  chrono_running(void);

void alarm_page_create(lv_obj_t *tile);

/* ---- shared helpers (ui_main.c) ---- */
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, int32_t x, int32_t y);
void      ui_plain(lv_obj_t *o);                   /* strip theme styles     */
void      ui_fmt_temp(char *buf, size_t n, int ok, int32_t tenths, int degree_sign);

#endif /* UI_PAGES_H */
