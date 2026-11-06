/**
 * Accent-colour theme with animated cross-fade between palettes.
 * Every UI module registers an "apply" callback; during a transition the
 * callbacks are called on each animation step with the blended colour.
 */
#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

typedef void (*theme_apply_cb_t)(lv_color_t accent, lv_color_t bg);

void        theme_init(void);
void        theme_register(theme_apply_cb_t cb);
/* Thai colour of the day; call every UI tick with the RTC weekday (1 = Mon .. 7 = Sun).
 * Returns 1 when the day (and so the colour) changed, 2 when a tap preview ended. */
int         theme_follow_day(uint8_t weekday);
void        theme_next(void);                 /* tap: preview next day's colour */
int         theme_is_today(void);
lv_color_t  theme_accent(void);
lv_color_t  theme_contrast(void);             /* hour / minute marks */
lv_color_t  theme_bg(void);
const char *theme_name(void);

#endif /* UI_THEME_H */
