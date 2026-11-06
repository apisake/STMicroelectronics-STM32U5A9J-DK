/**
 * Accent colour follows the Thai colour of the day (สีประจำวัน), with an
 * animated cross-fade (see ui_theme.h).
 *
 *   อาทิตย์ แดง | จันทร์ เหลือง | อังคาร ชมพู | พุธ เขียว |
 *   พฤหัสบดี ส้ม | ศุกร์ ฟ้า | เสาร์ ม่วง
 *
 * Tapping previews the other days' colours; after PREVIEW_MS without a tap the
 * watch fades back to today's colour. At midnight it fades to the new day.
 */
#include "ui_theme.h"
#include <stdio.h>

#define MAX_CBS        12
#define FADE_MS        600
#define PREVIEW_MS     15000U

/* contrast: colour for the hour / minute marks, chosen to stand out from the
 * day colour (roughly complementary, but bright enough on a black background) */
typedef struct { const char *name; uint32_t accent; uint32_t contrast; } palette_t;

/* index 0 = Sunday ... 6 = Saturday */
static const palette_t palettes[7] = {
  { "สีแดง · วันอาทิตย์",     0xFF3B3B, 0x00E5FF },   /* red    -> cyan    */
  { "สีเหลือง · วันจันทร์",    0xFFD60A, 0xD25CFF },   /* yellow -> violet  */
  { "สีชมพู · วันอังคาร",      0xFF6EC7, 0x9DFF2E },   /* pink   -> lime    */
  { "สีเขียว · วันพุธ",        0x22FF7A, 0xFF4FD8 },   /* green  -> magenta */
  { "สีส้ม · วันพฤหัสบดี",     0xFF8A00, 0x2EC8FF },   /* orange -> azure   */
  { "สีฟ้า · วันศุกร์",         0x2ED3FF, 0xFF9A1F },   /* blue   -> orange  */
  { "สีม่วง · วันเสาร์",       0xB27BFF, 0xFFE14D },   /* purple -> yellow  */
};

static theme_apply_cb_t cbs[MAX_CBS];
static uint32_t n_cbs;
static uint32_t idx;               /* palette shown now        */
static int      today = -1;        /* palette of the RTC day   */
static uint32_t preview_since;     /* 0 = following the day    */
static lv_color_t cur_accent, cur_bg, from_accent, to_accent;
static lv_color_t cur_contrast, from_contrast, to_contrast;

/* background = accent mixed ~6% into black: a faint CRT-phosphor tint */
static lv_color_t bg_for(lv_color_t accent)
{
  return lv_color_mix(accent, lv_color_black(), 16);
}

static void apply_all(lv_color_t a, lv_color_t c)
{
  cur_accent = a;
  cur_contrast = c;
  cur_bg     = bg_for(a);
  for (uint32_t i = 0; i < n_cbs; i++)
  {
    cbs[i](cur_accent, cur_bg);
  }
}

static void fade_cb(void *var, int32_t v)
{
  (void)var;
  apply_all(lv_color_mix(to_accent, from_accent, (uint8_t)v),
            lv_color_mix(to_contrast, from_contrast, (uint8_t)v));
}

static void go_to(uint32_t new_idx, int animate)
{
  idx = new_idx;
  lv_anim_delete(&idx, fade_cb);
  if (!animate)
  {
    apply_all(lv_color_hex(palettes[idx].accent), lv_color_hex(palettes[idx].contrast));
    return;
  }
  from_accent   = cur_accent;
  to_accent     = lv_color_hex(palettes[idx].accent);
  from_contrast = cur_contrast;
  to_contrast   = lv_color_hex(palettes[idx].contrast);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, &idx);
  lv_anim_set_values(&a, 0, 255);
  lv_anim_set_duration(&a, FADE_MS);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_set_exec_cb(&a, fade_cb);
  lv_anim_start(&a);
}

void theme_init(void)
{
  idx = 0;
  cur_accent   = lv_color_hex(palettes[idx].accent);
  cur_contrast = lv_color_hex(palettes[idx].contrast);
  cur_bg       = bg_for(cur_accent);
}

void theme_register(theme_apply_cb_t cb)
{
  if (n_cbs < MAX_CBS)
  {
    cbs[n_cbs++] = cb;
    cb(cur_accent, cur_bg);
  }
}

int theme_follow_day(uint8_t weekday)
{
  int day_idx = (weekday >= 7U) ? 0 : (int)weekday;     /* RTC: 1 = Mon ... 7 = Sun */
  int changed = 0;

  if (day_idx != today)
  {
    int first = (today < 0);
    today = day_idx;
    preview_since = 0;
    go_to((uint32_t)today, !first);                     /* first call: no fade */
    printf("colour of the day -> %s\r\n", palettes[today].name);
    changed = !first;
  }
  else if (preview_since && lv_tick_elaps(preview_since) > PREVIEW_MS)
  {
    preview_since = 0;                                   /* preview over: back to today */
    go_to((uint32_t)today, 1);
    changed = 2;
  }
  return changed;
}

void theme_next(void)
{
  preview_since = lv_tick_get();
  if (preview_since == 0) preview_since = 1;
  go_to((idx + 1U) % 7U, 1);
  printf("theme preview -> %s\r\n", palettes[idx].name);
}

lv_color_t theme_accent(void)  { return cur_accent; }
lv_color_t theme_contrast(void) { return cur_contrast; }
lv_color_t theme_bg(void)      { return cur_bg; }
const char *theme_name(void)   { return palettes[idx].name; }
int        theme_is_today(void) { return (int)idx == today; }
