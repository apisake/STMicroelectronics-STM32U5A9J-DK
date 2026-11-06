/**
 * Real-time clock for the watch face (see rtc_clock.h)
 */
#include "rtc_clock.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define BKP_MAGIC      0x32F5C10CUL   /* RTC_BKP_DR0: RTC has been configured   */
                                      /* RTC_BKP_DR1: hash of firmware build time */
#define BKP_ALARM      RTC_BKP_DR3    /* [31:24]=tag [16]=enabled [15:8]=h [7:0]=m */
#define ALARM_TAG      0xA1UL
#define BKP_CHIME      RTC_BKP_DR4    /* [31:24]=tag [0]=enabled */
#define CHIME_TAG      0xC1UL

static RTC_HandleTypeDef hrtc;
static int use_lse;

/* ------------------------------------------------------------------------ */
static uint32_t build_stamp(void)
{
  /* FNV-1a over "__DATE__ __TIME__": changes on every new build */
  const char *s = __DATE__ " " __TIME__;
  uint32_t h = 2166136261UL;
  while (*s) { h ^= (uint8_t)*s++; h *= 16777619UL; }
  return h;
}

/* monotonic key for comparing date-times */
static uint64_t dt_key(const rtc_datetime_t *d)
{
  return ((((((uint64_t)d->year * 13U + d->month) * 32U + d->day) * 24U + d->hour) * 60U
           + d->minute) * 60U) + d->second;
}

/* 1 = Monday .. 7 = Sunday (Sakamoto's algorithm) */
static uint8_t weekday_of(int y, int m, int d)
{
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  int w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;   /* 0 = Sunday */
  return (uint8_t)(w == 0 ? 7 : w);
}

static void build_datetime(rtc_datetime_t *dt)
{
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *date = __DATE__;            /* "Nov  5 2026" */
  const char *time = __TIME__;            /* "21:30:05"    */
  char mon[4] = { date[0], date[1], date[2], 0 };
  const char *p = strstr(months, mon);

  dt->month  = (uint8_t)(p ? (p - months) / 3 + 1 : 1);
  dt->day    = (uint8_t)((date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0'));
  dt->year   = (uint16_t)((date[7] - '0') * 1000 + (date[8] - '0') * 100 +
                          (date[9] - '0') * 10 + (date[10] - '0'));
  dt->hour   = (uint8_t)((time[0] - '0') * 10 + (time[1] - '0'));
  dt->minute = (uint8_t)((time[3] - '0') * 10 + (time[4] - '0'));
  dt->second = (uint8_t)((time[6] - '0') * 10 + (time[7] - '0'));
}

/* ------------------------------------------------------------------------ */
int rtc_clock_init(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_PeriphCLKInitTypeDef pclk = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  /* 1) try the 32.768 kHz crystal (accurate) */
  osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  osc.LSEState       = RCC_LSE_ON;
  osc.PLL.PLLState   = RCC_PLL_NONE;
  use_lse = (HAL_RCC_OscConfig(&osc) == HAL_OK);

  /* 2) fall back to the internal RC (LSI, ~32 kHz, less accurate) */
  if (!use_lse)
  {
    osc.LSEState = RCC_LSE_OFF;
    (void)HAL_RCC_OscConfig(&osc);
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    osc.LSIState       = RCC_LSI_ON;
    osc.LSIDiv         = RCC_LSI_DIV1;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
      return -1;
    }
  }

  pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  pclk.RTCClockSelection    = use_lse ? RCC_RTCCLKSOURCE_LSE : RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK)
  {
    return -2;
  }
  __HAL_RCC_RTC_ENABLE();
  __HAL_RCC_RTCAPB_CLK_ENABLE();
  __HAL_RCC_RTCAPB_CLKAM_ENABLE();

  hrtc.Instance            = RTC;
  hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv   = 127U;                      /* ck_apre = 256 Hz (LSE) */
  hrtc.Init.SynchPrediv    = use_lse ? 255U : 249U;     /* 32768 or 32000 Hz -> 1 Hz */
  hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap    = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp   = RTC_OUTPUT_PULLUP_NONE;
  hrtc.Init.BinMode        = RTC_BINARY_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    return -3;
  }

  /* After a system reset the calendar shadow registers are stale until RSF is
   * set again (RM0456). HAL_RTC_Init() skips this when the calendar is already
   * initialised, so wait explicitly before the first read. */
  __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
  (void)HAL_RTC_WaitForSynchro(&hrtc);
  __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);

  /* Use the build time only when the RTC holds no valid time: never configured,
   * or running more than 10 min behind this firmware's build time (e.g. after a power loss).
   * A correct time (e.g. synced with tools/set_time.py) survives re-flashing. */
  {
    rtc_datetime_t b, now = {0};
    build_datetime(&b);
    int configured = (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == BKP_MAGIC);
    if (configured)
    {
      rtc_clock_get(&now);
    }
    if (configured)
    {
      printf("RTC holds %04u-%02u-%02u %02u:%02u:%02u\r\n",
             now.year, now.month, now.day, now.hour, now.minute, now.second);
    }
    /* only a clearly invalid RTC (> 10 min behind the build) is overwritten:
     * flashing takes ~30 s after the build, so a small lag is normal */
    if (!configured || dt_key(&now) + 600U < dt_key(&b))
    {
      if (rtc_clock_set(&b) != 0)
      {
        return -4;
      }
      printf("RTC set from build time %s %s\r\n", __DATE__, __TIME__);
    }
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, build_stamp());
  }
  printf("RTC running on %s\r\n", rtc_clock_source());

  /* restore the daily alarm saved before reset */
  HAL_NVIC_SetPriority(RTC_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(RTC_IRQn);
  uint32_t a = HAL_RTCEx_BKUPRead(&hrtc, BKP_ALARM);
  if ((a >> 24) == ALARM_TAG)
  {
    (void)rtc_alarm_set((uint8_t)((a >> 8) & 0xFFU), (uint8_t)(a & 0xFFU), (int)((a >> 16) & 1U));
  }

  /* hourly chime: default on */
  uint32_t c = HAL_RTCEx_BKUPRead(&hrtc, BKP_CHIME);
  (void)rtc_chime_set(((c >> 24) == CHIME_TAG) ? (int)(c & 1U) : 1);
  return 0;
}

/* ------------------------------ backup regs ------------------------------ */
uint32_t rtc_clock_bkp_read(uint32_t idx)
{
  return HAL_RTCEx_BKUPRead(&hrtc, idx);
}

void rtc_clock_bkp_write(uint32_t idx, uint32_t value)
{
  HAL_RTCEx_BKUPWrite(&hrtc, idx, value);
}

/* --------------------------------- alarm --------------------------------- */
static volatile int alarm_fired;
static uint8_t alarm_h = 7, alarm_m = 0;
static int alarm_en;

int rtc_alarm_set(uint8_t hour, uint8_t minute, int enabled)
{
  if (hour > 23U || minute > 59U)
  {
    return -1;
  }
  alarm_h = hour; alarm_m = minute; alarm_en = enabled ? 1 : 0;

  (void)HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
  if (alarm_en)
  {
    RTC_AlarmTypeDef al = {0};
    al.AlarmTime.Hours          = hour;
    al.AlarmTime.Minutes        = minute;
    al.AlarmTime.Seconds        = 0;
    al.AlarmTime.SubSeconds     = 0;
    al.AlarmMask                = RTC_ALARMMASK_DATEWEEKDAY;    /* every day */
    al.AlarmSubSecondMask       = RTC_ALARMSUBSECONDMASK_ALL;
    al.AlarmDateWeekDaySel      = RTC_ALARMDATEWEEKDAYSEL_DATE;
    al.AlarmDateWeekDay         = 1;
    al.Alarm                    = RTC_ALARM_A;
    if (HAL_RTC_SetAlarm_IT(&hrtc, &al, RTC_FORMAT_BIN) != HAL_OK)
    {
      return -2;
    }
  }
  HAL_RTCEx_BKUPWrite(&hrtc, BKP_ALARM, (ALARM_TAG << 24) | ((uint32_t)alarm_en << 16) |
                                         ((uint32_t)hour << 8) | minute);
  printf("alarm %02u:%02u %s\r\n", hour, minute, alarm_en ? "ARMED" : "off");
  return 0;
}

void rtc_alarm_get(uint8_t *hour, uint8_t *minute, int *enabled)
{
  *hour = alarm_h; *minute = alarm_m; *enabled = alarm_en;
}

int rtc_alarm_take_fired(void)
{
  if (!alarm_fired) return 0;
  alarm_fired = 0;
  return 1;
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *h)
{
  (void)h;
  alarm_fired = 1;
}

/* --------------------------------- chime --------------------------------- */
static volatile int chime_fired;
static int chime_en;

int rtc_chime_set(int enabled)
{
  chime_en = enabled ? 1 : 0;
  (void)HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_B);
  if (chime_en)
  {
    RTC_AlarmTypeDef al = {0};
    al.AlarmTime.Minutes    = 0;
    al.AlarmTime.Seconds    = 0;
    al.AlarmMask            = RTC_ALARMMASK_DATEWEEKDAY | RTC_ALARMMASK_HOURS;   /* every hour */
    al.AlarmSubSecondMask   = RTC_ALARMSUBSECONDMASK_ALL;
    al.AlarmDateWeekDaySel  = RTC_ALARMDATEWEEKDAYSEL_DATE;
    al.AlarmDateWeekDay     = 1;
    al.Alarm                = RTC_ALARM_B;
    if (HAL_RTC_SetAlarm_IT(&hrtc, &al, RTC_FORMAT_BIN) != HAL_OK)
    {
      return -1;
    }
  }
  HAL_RTCEx_BKUPWrite(&hrtc, BKP_CHIME, (CHIME_TAG << 24) | (uint32_t)chime_en);
  printf("hourly chime %s\r\n", chime_en ? "on" : "off");
  return 0;
}

int rtc_chime_enabled(void)
{
  return chime_en;
}

int rtc_chime_take_fired(void)
{
  if (!chime_fired) return 0;
  chime_fired = 0;
  return 1;
}

void HAL_RTCEx_AlarmBEventCallback(RTC_HandleTypeDef *h)
{
  (void)h;
  chime_fired = 1;
}

void rtc_clock_irq_handler(void)
{
  HAL_RTC_AlarmIRQHandler(&hrtc);
}

void rtc_clock_get(rtc_datetime_t *dt)
{
  RTC_TimeTypeDef t = {0};
  RTC_DateTypeDef d = {0};

  /* GetTime must be followed by GetDate (unlocks the shadow registers) */
  HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

  dt->year    = (uint16_t)(2000U + d.Year);
  dt->month   = d.Month;
  dt->day     = d.Date;
  dt->weekday = d.WeekDay;
  dt->hour    = t.Hours;
  dt->minute  = t.Minutes;
  dt->second  = t.Seconds;
  dt->ms      = (uint16_t)(((t.SecondFraction - t.SubSeconds) * 1000U) / (t.SecondFraction + 1U));
}

int rtc_clock_set(const rtc_datetime_t *dt)
{
  RTC_TimeTypeDef t = {0};
  RTC_DateTypeDef d = {0};

  if (dt->year < 2000U || dt->year > 2099U || dt->month < 1U || dt->month > 12U ||
      dt->day < 1U || dt->day > 31U || dt->hour > 23U || dt->minute > 59U || dt->second > 59U)
  {
    return -1;
  }

  t.Hours          = dt->hour;
  t.Minutes        = dt->minute;
  t.Seconds        = dt->second;
  t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  t.StoreOperation = RTC_STOREOPERATION_RESET;
  d.Year           = (uint8_t)(dt->year - 2000U);
  d.Month          = dt->month;
  d.Date           = dt->day;
  d.WeekDay        = weekday_of(dt->year, dt->month, dt->day);

  if (HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK ||
      HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) != HAL_OK)
  {
    return -2;
  }
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, BKP_MAGIC);
  return 0;
}

const char *rtc_clock_source(void)
{
  return use_lse ? "LSE (32.768 kHz crystal)" : "LSI (internal RC, less accurate)";
}

int rtc_clock_set_from_cmd(const char *line)
{
  unsigned y, mo, d, h, mi, s;
  rtc_datetime_t dt = {0};

  if (sscanf(line, "T %u-%u-%u %u:%u:%u", &y, &mo, &d, &h, &mi, &s) != 6)
  {
    return -1;
  }
  dt.year = (uint16_t)y; dt.month = (uint8_t)mo; dt.day = (uint8_t)d;
  dt.hour = (uint8_t)h;  dt.minute = (uint8_t)mi; dt.second = (uint8_t)s;
  return rtc_clock_set(&dt);
}
