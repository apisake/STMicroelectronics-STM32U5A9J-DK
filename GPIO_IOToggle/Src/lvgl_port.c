/**
 * LVGL v9 port for STM32U5A9J-DK (see lvgl_port.h)
 *
 * Frame buffer layout (set up by the BSP):
 *   LTDC reads a *virtual* buffer at hlcd_ltdc.LayerCfg[0].FBStartAdress
 *   (GFXMMU virtual buffer 0). Each virtual line is 3072 bytes = 768 px of
 *   ARGB8888; only the 480x480 round visible area is backed by physical RAM.
 *
 * NOTE: BSP_LCD_FillRGBRect() from BSP v1.3.1 copies the *first* source row
 * for every line (source index ignores the row), so it cannot be used as a
 * flush routine. We copy rows ourselves.
 */
#include "lvgl_port.h"
#include "main.h"
#include "stm32u5x9j_discovery_lcd.h"
#include "stm32u5x9j_discovery_ts.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

#define DISP_W            480U
#define DISP_H            480U
#define FB_LINE_BYTES    3072U                  /* GFXMMU virtual line pitch */
#define DRAW_BUF_LINES   120U                   /* 480 x 120 x 4 B = 225 KB  */
#define DRAW_BUF_BYTES   (DISP_W * DRAW_BUF_LINES * 4U)

/* 1 = rotate the whole UI by 180 deg (board held with the USB ports at the TOP).
 * Done in the flush (pixel copy) and in the touch read, so LVGL itself is unaware. */
#ifndef DISPLAY_ROTATE_180
#define DISPLAY_ROTATE_180  1
#endif

/* two partial draw buffers in internal SRAM */
static uint8_t draw_buf1[DRAW_BUF_BYTES] __attribute__((aligned(32)));
static uint8_t draw_buf2[DRAW_BUF_BYTES] __attribute__((aligned(32)));

static int ts_ok;
static volatile uint32_t frames;
static int touch_block;          /* ignore the touch that woke us until it is released */

#ifdef USE_NEOCHROM
/* GPU2D (NeoChrom) handle: name required by LVGL's NemaGFX STM32 HAL glue */
GPU2D_HandleTypeDef hgpu2d;

static int gpu2d_init(void)
{
  __HAL_RCC_GPU2D_CLK_ENABLE();
  hgpu2d.Instance = GPU2D;
  if (HAL_GPU2D_Init(&hgpu2d) != HAL_OK)
  {
    return -1;
  }
  HAL_NVIC_SetPriority(GPU2D_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(GPU2D_IRQn);
  HAL_NVIC_SetPriority(GPU2D_ER_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(GPU2D_ER_IRQn);
  return 0;
}
#endif

static void render_ready_cb(lv_event_t *e)
{
  (void)e;
  frames++;
}

/* ------------------------------------------------------------------------ */
static uint32_t tick_cb(void)
{
  return HAL_GetTick();
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  const uint32_t w      = (uint32_t)lv_area_get_width(area);
  const uint32_t h      = (uint32_t)lv_area_get_height(area);
  const uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_ARGB8888);
  uint8_t *fb = (uint8_t *)hlcd_ltdc.LayerCfg[0].FBStartAdress;

#if DISPLAY_ROTATE_180
  /* (x, y) -> (W-1-x, H-1-y): rows bottom-up, pixels right-to-left */
  uint8_t *dst_row = fb + (DISP_H - 1U - (uint32_t)area->y1) * FB_LINE_BYTES
                        + (DISP_W - 1U - (uint32_t)area->x1) * 4U;
  for (uint32_t y = 0; y < h; y++)
  {
    const uint32_t *s = (const uint32_t *)px_map;
    uint32_t *d = (uint32_t *)dst_row;
    for (uint32_t x = 0; x < w; x++)
    {
      *(d - x) = s[x];
    }
    dst_row -= FB_LINE_BYTES;
    px_map  += stride;
  }
#else
  uint8_t *dst = fb + (uint32_t)area->y1 * FB_LINE_BYTES + (uint32_t)area->x1 * 4U;
  for (uint32_t y = 0; y < h; y++)
  {
    memcpy(dst, px_map, w * 4U);
    dst    += FB_LINE_BYTES;
    px_map += stride;
  }
#endif
  lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  (void)indev;
  static int32_t last_x, last_y;
  TS_State_t ts = {0};

  if (ts_ok && BSP_TS_GetState(0, &ts) == BSP_ERROR_NONE && ts.TouchDetected)
  {
    if (touch_block)                  /* still the finger that woke the device */
    {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
#if DISPLAY_ROTATE_180
    last_x = (int32_t)(DISP_W - 1U) - (int32_t)ts.TouchX;
    last_y = (int32_t)(DISP_H - 1U) - (int32_t)ts.TouchY;
#else
    last_x = (int32_t)ts.TouchX;
    last_y = (int32_t)ts.TouchY;
#endif
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else
  {
    touch_block = 0;
    data->state = LV_INDEV_STATE_RELEASED;
  }
  data->point.x = last_x;
  data->point.y = last_y;
}

/* ------------------------------------------------------------------------ */
int lvgl_port_init(void)
{
  /* LCD hardware */
  int32_t err = BSP_LCD_Init(0, LCD_ORIENTATION_PORTRAIT);
  if (err != BSP_ERROR_NONE)
  {
    printf("BSP_LCD_Init failed: %ld\r\n", (long)err);
    return -1;
  }
  BSP_LCD_SetActiveLayer(0, 0);
  BSP_LCD_FillRect(0, 0, 0, DISP_W, DISP_H, 0xFF000000UL);   /* black before first frame */
  BSP_LCD_DisplayOn(0);

  /* touch panel (polling, no EXTI) */
  TS_Init_t ts_init = { .Width = DISP_W, .Height = DISP_H,
                        .Orientation = TS_ORIENTATION_PORTRAIT, .Accuracy = 0 };
  err = BSP_TS_Init(0, &ts_init);
  ts_ok = (err == BSP_ERROR_NONE);
  printf("Touch init %s (%ld)\r\n", ts_ok ? "OK" : "FAILED", (long)err);

#ifdef USE_NEOCHROM
  if (gpu2d_init() != 0)
  {
    printf("GPU2D init failed\r\n");
    return -2;
  }
#endif

  /* LVGL core */
  lv_init();
  lv_tick_set_cb(tick_cb);

  lv_display_t *disp = lv_display_create((int32_t)DISP_W, (int32_t)DISP_H);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp, draw_buf1, draw_buf2, DRAW_BUF_BYTES,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_add_event_cb(disp, render_ready_cb, LV_EVENT_RENDER_READY, NULL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read_cb);

  printf("LVGL %d.%d.%d ready, renderer: %s\r\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
         LVGL_VERSION_PATCH, lvgl_port_renderer());
  return 0;
}

int lvgl_port_touch_ok(void)
{
  return ts_ok;
}

uint32_t lvgl_port_take_frames(void)
{
  uint32_t f = frames;
  frames = 0;
  return f;
}

const char *lvgl_port_renderer(void)
{
#ifdef USE_NEOCHROM
  return "NeoChrom GPU2D (NemaGFX)";
#else
  return "software";
#endif
}

void lvgl_port_display_sleep(void)
{
  BSP_LCD_DisplayOff(0);
  BSP_LCD_DeInit(0);                 /* DSI, LTDC, GFXMMU, PLL3 off */
  if (ts_ok)
  {
    BSP_TS_EnableIT(0);              /* PE8 falling edge -> EXTI8 wakes the MCU */
  }
}

void lvgl_port_display_wake(void)
{
  if (ts_ok)
  {
    BSP_TS_DisableIT(0);             /* back to polling */
    touch_block = 1;
  }
  if (BSP_LCD_Init(0, LCD_ORIENTATION_PORTRAIT) == BSP_ERROR_NONE)
  {
    BSP_LCD_SetActiveLayer(0, 0);
    BSP_LCD_DisplayOn(0);
  }
  lv_obj_invalidate(lv_screen_active());
  lv_obj_invalidate(lv_layer_top());
  lv_display_trigger_activity(NULL);
}
