/**
 * Real-time clock for the watch face (STM32 RTC, LSE with LSI fallback)
 *
 *  - First boot after a new firmware build: time is set from __DATE__/__TIME__
 *  - After a plain reset: RTC keeps running (backup domain not reset)
 *  - Exact time can be pushed over UART:  "T YYYY-MM-DD HH:MM:SS\n"
 *    (see tools/set_time.py)
 */
#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <stdint.h>

typedef struct
{
  uint16_t year;     /* 2000..2099 */
  uint8_t  month;    /* 1..12 */
  uint8_t  day;      /* 1..31 */
  uint8_t  weekday;  /* 1 = Monday .. 7 = Sunday */
  uint8_t  hour;     /* 0..23 */
  uint8_t  minute;
  uint8_t  second;
  uint16_t ms;       /* 0..999 from RTC sub-seconds */
} rtc_datetime_t;

int         rtc_clock_init(void);                 /* 0 = OK */
void        rtc_clock_get(rtc_datetime_t *dt);
int         rtc_clock_set(const rtc_datetime_t *dt);  /* weekday is computed */
const char *rtc_clock_source(void);               /* "LSE" or "LSI" */

/* Parse "T YYYY-MM-DD HH:MM:SS" and set the RTC. Returns 0 on success. */
int         rtc_clock_set_from_cmd(const char *line);

/* Backup registers (survive reset & Stop mode). Index 2..31 are free for the app. */
uint32_t    rtc_clock_bkp_read(uint32_t idx);
void        rtc_clock_bkp_write(uint32_t idx, uint32_t value);

/* Daily alarm on RTC Alarm A (also wakes the MCU from Stop mode).
 * Setting is stored in a backup register and restored at boot. */
int         rtc_alarm_set(uint8_t hour, uint8_t minute, int enabled);
void        rtc_alarm_get(uint8_t *hour, uint8_t *minute, int *enabled);
int         rtc_alarm_take_fired(void);      /* 1 once after the alarm fired */
void        rtc_clock_irq_handler(void);     /* call from RTC_IRQHandler     */

/* Hourly chime on RTC Alarm B (fires at every HH:00:00, wakes from Stop).
 * On/off is stored in a backup register (default: on). */
int         rtc_chime_set(int enabled);
int         rtc_chime_enabled(void);
int         rtc_chime_take_fired(void);      /* 1 once after the hour struck */

#endif /* RTC_CLOCK_H */
