/**
 * LVGL v9 port for STM32U5A9J-DK
 *  - display : BSP LCD (MIPI-DSI + LTDC + GFXMMU), ARGB8888, partial rendering
 *  - input   : Sitronix capacitive touch panel (BSP_TS, polled)
 *  - tick    : HAL_GetTick()
 */
#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include <stdint.h>

/* Initialise LCD + touch + LVGL. Returns 0 on success. */
int  lvgl_port_init(void);

/* 1 if the touch panel was initialised successfully */
int  lvgl_port_touch_ok(void);

/* frames rendered since the last call (for an FPS read-out) */
uint32_t    lvgl_port_take_frames(void);
const char *lvgl_port_renderer(void);   /* "NeoChrom GPU2D" or "software" */

/* power saving: panel + DSI/LTDC off, touch INT armed as wake-up source */
void lvgl_port_display_sleep(void);
/* restore the panel after Stop mode (clocks must already be restored) */
void lvgl_port_display_wake(void);

#endif /* LVGL_PORT_H */
