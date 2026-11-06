/**
 * RADAR page: sci-fi radar scope watch face (Thai, Prompt font)
 *
 *   - huge digital HH:MM in the centre (Prompt SemiBold 130 px)
 *   - phosphor sweep beam with fading trail  = seconds hand (1 rev / minute)
 *   - hour ("ชม.") and minute ("นาที") targets on the outer track; they light up
 *     and ripple when the beam passes ("ping")
 *   - random faint contacts appear and fade for atmosphere
 *   - Thai weekday / date (Buddhist era) / temperature
 *   - boot animation: rings expand, read-outs fade in
 *   - tap anywhere: next colour theme
 */
#include "ui_pages.h"
#include "ui_theme.h"
#include "fonts_prompt.h"
#include "thai_text.h"
#include <math.h>
#include <stdio.h>

/* ------------------------------ geometry --------------------------------- */
#define R_TRACK       186      /* hour + minute targets      */
#define R_SCOPE       205      /* sweep radius / outer ring  */
#define TRAIL_DEG      54      /* length of fading trail     */
#define TRAIL_STEPS    18      /* 3 deg per triangle         */
#define N_CONTACTS      5
#define DEG2RAD       (3.14159265f / 180.0f)

/* ------------------------------- objects --------------------------------- */
typedef struct
{
  lv_obj_t *dot;
  lv_obj_t *ripple;
  lv_obj_t *ripple2;      /* second wave (hour mark only) */
  lv_obj_t *tag;          /* may be NULL */
  uint8_t   kind;         /* 0 = contact, 1 = hour, 2 = minute */
  int32_t   grow;         /* how far the ripple expands, px */
  uint32_t  ripple_ms;
  float     ang;          /* degrees, 0 = 12 o'clock, clockwise */
  int32_t   r;
  int32_t   size;
  lv_opa_t  rest_opa;     /* opacity it fades back to after a ping */
  uint8_t   relocate;     /* contacts: move after fading out */
} blip_t;

static lv_obj_t *tile_obj;
static lv_obj_t *grid;          /* static rings + crosshair (custom draw) */
static lv_obj_t *sweep;         /* sweep beam (custom draw)               */
static lv_obj_t *bezel;         /* lv_scale: 120 ticks + hour numerals    */
static lv_obj_t *lbl_wday, *lbl_time, *lbl_date, *lbl_temp, *lbl_hud_r;

static blip_t blip_h, blip_m, contacts[N_CONTACTS];

static float     sweep_ang;     /* current beam angle, degrees */
static float     prev_ang = -1.0f;
static lv_area_t prev_inv;
static int       last_min = -1, last_hour = -1, last_day = -1, last_sec = -1;
static int32_t   intro_k = 0;   /* 0..1000 boot animation */

static const char *hour_txt[] = { "12", "1", "2", "3", "4", "5", "6",
                                  "7", "8", "9", "10", "11", "12", NULL };
static const char *wd_th[] = { "", "จันทร์", "อังคาร", "พุธ", "พฤหัสบดี", "ศุกร์", "เสาร์", "อาทิตย์" };
static const char *mon_th[] = { "", "มกราคม", "กุมภาพันธ์", "มีนาคม", "เมษายน", "พฤษภาคม", "มิถุนายน",
                                "กรกฎาคม", "สิงหาคม", "กันยายน", "ตุลาคม", "พฤศจิกายน", "ธันวาคม" };

/* ------------------------------- helpers --------------------------------- */
static inline float px(float ang, float r) { return (float)UI_CX + r * sinf(ang * DEG2RAD); }
static inline float py(float ang, float r) { return (float)UI_CY - r * cosf(ang * DEG2RAD); }

/* did the beam move across angle `a` between prev and cur (handles wrap)? */
static int swept(float prev, float cur, float a)
{
  if (prev < 0.0f) return 0;
  if (cur >= prev) return (a > prev && a <= cur);
  return (a > prev || a <= cur);
}

/* ------------------------- custom draw: grid ----------------------------- */
static void grid_draw_cb(lv_event_t *e)
{
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t c;
  lv_obj_get_coords(grid, &c);
  const int32_t ox = c.x1, oy = c.y1;
  const float k = (float)intro_k / 1000.0f;
  lv_color_t acc = theme_accent();

  /* range rings (expand during the boot animation) */
  lv_draw_arc_dsc_t arc;
  lv_draw_arc_dsc_init(&arc);
  arc.color = acc;
  arc.start_angle = 0;
  arc.end_angle = 360;
  arc.center.x = ox + UI_CX;
  arc.center.y = oy + UI_CY;
  const int32_t rings[]    = { 70, 140, R_TRACK, R_SCOPE };
  const lv_opa_t ring_opa[] = { 45, 45, 90, 170 };
  for (unsigned i = 0; i < sizeof(rings) / sizeof(rings[0]); i++)
  {
    arc.radius = (uint16_t)((float)rings[i] * k);
    if (arc.radius < 2) continue;
    arc.width  = (rings[i] == R_SCOPE) ? 2 : 1;
    arc.opa    = ring_opa[i];
    lv_draw_arc(layer, &arc);
  }

  /* spokes every 30 deg, outside the clock read-out */
  lv_draw_line_dsc_t ln;
  lv_draw_line_dsc_init(&ln);
  ln.color = acc;
  ln.width = 1;
  for (int q = 0; q < 12; q++)
  {
    float a = (float)q * 30.0f;
    int major = (q % 3) == 0;
    ln.opa = major ? 60 : 25;
    float r0 = 150.0f * k, r1 = (float)R_SCOPE * k;
    ln.p1.x = ox + px(a, r0); ln.p1.y = oy + py(a, r0);
    ln.p2.x = ox + px(a, r1); ln.p2.y = oy + py(a, r1);
    lv_draw_line(layer, &ln);
  }
}

/* ------------------------ custom draw: sweep beam ------------------------ */
static void sweep_draw_cb(lv_event_t *e)
{
  if (intro_k < 1000) return;
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t c;
  lv_obj_get_coords(sweep, &c);
  const float ox = (float)c.x1, oy = (float)c.y1;
  const float cx = ox + UI_CX, cy = oy + UI_CY;
  lv_color_t acc = theme_accent();

  lv_draw_triangle_dsc_t tri;
  lv_draw_triangle_dsc_init(&tri);
  tri.color = acc;
  const float step = (float)TRAIL_DEG / (float)TRAIL_STEPS;
  for (int i = 0; i < TRAIL_STEPS; i++)
  {
    float a0 = sweep_ang - (float)i * step;
    float a1 = a0 - step - 0.4f;
    float k  = 1.0f - (float)i / (float)TRAIL_STEPS;
    tri.opa  = (lv_opa_t)(95.0f * k * k);
    tri.p[0].x = cx;                   tri.p[0].y = cy;
    tri.p[1].x = ox + px(a0, R_SCOPE); tri.p[1].y = oy + py(a0, R_SCOPE);
    tri.p[2].x = ox + px(a1, R_SCOPE); tri.p[2].y = oy + py(a1, R_SCOPE);
    lv_draw_triangle(layer, &tri);
  }

  lv_draw_line_dsc_t ln;
  lv_draw_line_dsc_init(&ln);
  ln.color = acc;
  ln.p1.x = cx; ln.p1.y = cy;
  ln.p2.x = ox + px(sweep_ang, R_SCOPE);
  ln.p2.y = oy + py(sweep_ang, R_SCOPE);
  ln.round_end = 1;
  ln.width = 7; ln.opa = 60;
  lv_draw_line(layer, &ln);
  ln.width = 2; ln.opa = LV_OPA_COVER;
  lv_draw_line(layer, &ln);
}

/* bounding box (absolute) of the beam wedge at angle a */
static void wedge_area(float a, lv_area_t *out)
{
  lv_area_t c;
  lv_obj_get_coords(sweep, &c);
  float x0 = UI_CX, x1 = UI_CX, y0 = UI_CY, y1 = UI_CY;
  for (int i = 0; i <= 6; i++)
  {
    float t = a - (float)TRAIL_DEG * (float)i / 6.0f;
    float x = px(t, R_SCOPE), y = py(t, R_SCOPE);
    if (x < x0) x0 = x;
    if (x > x1) x1 = x;
    if (y < y0) y0 = y;
    if (y > y1) y1 = y;
  }
  for (int q = 0; q < 4; q++)
  {
    float qa = 90.0f * (float)q, d = a - qa;
    while (d < 0) d += 360.0f;
    if (d <= (float)TRAIL_DEG)
    {
      float x = px(qa, R_SCOPE), y = py(qa, R_SCOPE);
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
      if (y < y0) y0 = y;
      if (y > y1) y1 = y;
    }
  }
  out->x1 = c.x1 + (int32_t)x0 - 6; out->x2 = c.x1 + (int32_t)x1 + 6;
  out->y1 = c.y1 + (int32_t)y0 - 6; out->y2 = c.y1 + (int32_t)y1 + 6;
}

/* --------------------------------- blips --------------------------------- */
static void ripple_exec(void *var, int32_t v)
{
  lv_obj_t *rp = (lv_obj_t *)var;
  blip_t *b = (blip_t *)lv_obj_get_user_data(rp);
  int32_t s = b->size + (b->grow * v) / 1000;
  lv_obj_set_size(rp, s, s);
  lv_obj_set_pos(rp, (int32_t)px(b->ang, (float)b->r) - s / 2, (int32_t)py(b->ang, (float)b->r) - s / 2);
  lv_obj_set_style_border_opa(rp, (lv_opa_t)(220 - (220 * v) / 1000), 0);
}

static void dot_fade_exec(void *var, int32_t v)
{
  lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

/* hour mark: dot flashes white and fades back to its colour */
static void dot_flash_exec(void *var, int32_t v)
{
  lv_obj_set_style_bg_color((lv_obj_t *)var, lv_color_mix(lv_color_white(), theme_contrast(), (uint8_t)v), 0);
}

/* minute mark: dot pops bigger and back */
static void dot_pop_exec(void *var, int32_t v)
{
  blip_t *b = (blip_t *)var;
  int32_t s = b->size + v;
  int32_t x = (int32_t)px(b->ang, (float)b->r), y = (int32_t)py(b->ang, (float)b->r);
  lv_obj_set_size(b->dot, s, s);
  lv_obj_set_pos(b->dot, x - s / 2, y - s / 2);
}

static void ripple_start(blip_t *b, lv_obj_t *rp, uint32_t delay)
{
  lv_anim_t a;
  lv_anim_delete(rp, ripple_exec);
  lv_anim_init(&a);
  lv_anim_set_var(&a, rp);
  lv_anim_set_exec_cb(&a, ripple_exec);
  lv_anim_set_values(&a, 0, 1000);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_duration(&a, b->ripple_ms);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

static void blip_place(blip_t *b)
{
  int32_t x = (int32_t)px(b->ang, (float)b->r), y = (int32_t)py(b->ang, (float)b->r);
  lv_obj_set_pos(b->dot, x - b->size / 2, y - b->size / 2);
  if (b->tag)
  {
    /* tag sits just inside the track, centred on the target's angle */
    int32_t tx = (int32_t)px(b->ang, (float)b->r - 30.0f);
    int32_t ty = (int32_t)py(b->ang, (float)b->r - 30.0f);
    lv_obj_update_layout(b->tag);
    lv_obj_set_pos(b->tag, tx - lv_obj_get_width(b->tag) / 2, ty - lv_obj_get_height(b->tag) / 2);
  }
}

static void blip_ping(blip_t *b)
{
  lv_anim_t a;

  if (b->kind == 1)                       /* HOUR: white flash + double slow wave */
  {
    lv_anim_delete(b->dot, dot_flash_exec);
    lv_anim_init(&a);
    lv_anim_set_var(&a, b->dot);
    lv_anim_set_exec_cb(&a, dot_flash_exec);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_duration(&a, 900);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
    ripple_start(b, b->ripple, 0);
    ripple_start(b, b->ripple2, 380);
    return;
  }
  if (b->kind == 2)                       /* MINUTE: quick pop + small fast wave */
  {
    lv_anim_delete(b, dot_pop_exec);
    lv_anim_init(&a);
    lv_anim_set_var(&a, b);
    lv_anim_set_exec_cb(&a, dot_pop_exec);
    lv_anim_set_values(&a, 0, 10);
    lv_anim_set_duration(&a, 140);
    lv_anim_set_reverse_duration(&a, 260);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    ripple_start(b, b->ripple, 0);
    return;
  }

  /* contact: flare up, then fade out */
  lv_anim_delete(b->dot, dot_fade_exec);
  lv_anim_init(&a);
  lv_anim_set_var(&a, b->dot);
  lv_anim_set_exec_cb(&a, dot_fade_exec);
  lv_anim_set_values(&a, LV_OPA_COVER, b->rest_opa);
  lv_anim_set_duration(&a, b->rest_opa ? 2600 : 3800);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
  lv_anim_start(&a);

  ripple_start(b, b->ripple, 0);

  if (b->relocate == 0 && b->rest_opa == 0) b->relocate = 1;
}

static void blip_init(blip_t *b, lv_obj_t *parent, int32_t size, lv_opa_t rest, const char *tag)
{
  b->size = size;
  b->rest_opa = rest;
  b->dot = lv_obj_create(parent);
  ui_plain(b->dot);
  lv_obj_set_size(b->dot, size, size);
  lv_obj_set_style_radius(b->dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(b->dot, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(b->dot, size, 0);
  lv_obj_set_style_shadow_spread(b->dot, 1, 0);
  lv_obj_set_style_opa(b->dot, rest, 0);

  b->ripple = lv_obj_create(parent);
  ui_plain(b->ripple);
  lv_obj_set_size(b->ripple, size, size);
  lv_obj_set_style_radius(b->ripple, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(b->ripple, 2, 0);
  lv_obj_set_style_border_opa(b->ripple, 0, 0);
  lv_obj_set_user_data(b->ripple, b);

  b->ripple2 = lv_obj_create(parent);
  ui_plain(b->ripple2);
  lv_obj_set_size(b->ripple2, size, size);
  lv_obj_set_style_radius(b->ripple2, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(b->ripple2, 2, 0);
  lv_obj_set_style_border_opa(b->ripple2, 0, 0);
  lv_obj_set_user_data(b->ripple2, b);

  b->kind = 0;
  b->grow = 60;
  b->ripple_ms = 900;
  b->tag = NULL;
  if (tag)
  {
    b->tag = ui_label(parent, &font_prompt_16, 0, 0);
    lv_obj_set_align(b->tag, LV_ALIGN_TOP_LEFT);
    thai_label_set_text(b->tag, tag);
  }
}

static void contact_relocate(blip_t *b)
{
  b->ang = fmodf(sweep_ang + (float)lv_rand(60, 240), 360.0f);
  b->r   = (int32_t)lv_rand(40, R_SCOPE - 15);
  b->relocate = 0;
  blip_place(b);
}

/* ------------------------------- theming --------------------------------- */
static void radar_apply_theme(lv_color_t acc, lv_color_t bg)
{
  lv_obj_set_style_bg_color(tile_obj, bg, 0);
  lv_obj_invalidate(grid);
  lv_obj_invalidate(sweep);

  lv_obj_set_style_line_color(bezel, acc, LV_PART_INDICATOR);
  lv_obj_set_style_line_color(bezel, acc, LV_PART_ITEMS);
  lv_obj_set_style_text_color(bezel, acc, LV_PART_INDICATOR);

  /* big clock: accent blended towards white so it reads well on any theme */
  lv_obj_set_style_text_color(lbl_time, lv_color_mix(lv_color_white(), acc, 90), 0);
  lv_obj_t *labels[] = { lbl_wday, lbl_date, lbl_temp, lbl_hud_r };
  for (unsigned i = 0; i < sizeof(labels) / sizeof(labels[0]); i++)
  {
    lv_obj_set_style_text_color(labels[i], acc, 0);
  }

  blip_t *all[2 + N_CONTACTS] = { &blip_h, &blip_m };
  for (int i = 0; i < N_CONTACTS; i++) all[2 + i] = &contacts[i];
  for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); i++)
  {
    lv_obj_set_style_bg_color(all[i]->dot, acc, 0);
    lv_obj_set_style_shadow_color(all[i]->dot, acc, 0);
    lv_obj_set_style_border_color(all[i]->ripple, acc, 0);
    lv_obj_set_style_border_color(all[i]->ripple2, acc, 0);
    if (all[i]->tag) lv_obj_set_style_text_color(all[i]->tag, acc, 0);
  }

  /* hour / minute marks: contrasting colour so they read as markers next to
   * the big digital time; a thin background-coloured outline separates them
   * from the rings they sit on */
  lv_color_t con = theme_contrast();
  blip_t *marks[] = { &blip_h, &blip_m };
  for (int i = 0; i < 2; i++)
  {
    lv_obj_set_style_bg_color(marks[i]->dot, con, 0);
    lv_obj_set_style_shadow_color(marks[i]->dot, con, 0);
    lv_obj_set_style_border_color(marks[i]->dot, bg, 0);
    lv_obj_set_style_border_color(marks[i]->ripple, con, 0);
    lv_obj_set_style_border_color(marks[i]->ripple2, con, 0);
    lv_obj_set_style_text_color(marks[i]->tag, con, 0);
    lv_obj_set_style_bg_color(marks[i]->tag, bg, 0);
  }
}

static void tile_clicked_cb(lv_event_t *e)
{
  (void)e;
  if (ui_chime_stop()) return;               /* first tap just silences the chime */
  theme_next();
  ui_toast(theme_name());
}

/* --------------------------- boot animation ------------------------------ */
static void intro_exec(void *var, int32_t v)
{
  (void)var;
  intro_k = v;
  lv_obj_invalidate(grid);
  lv_opa_t o = (lv_opa_t)((v * 255) / 1000);
  lv_obj_t *fade[] = { lbl_wday, lbl_time, lbl_date, lbl_temp, lbl_hud_r, bezel };
  for (unsigned i = 0; i < sizeof(fade) / sizeof(fade[0]); i++) lv_obj_set_style_opa(fade[i], o, 0);
}

void radar_intro(void)
{
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, &intro_k);
  lv_anim_set_exec_cb(&a, intro_exec);
  lv_anim_set_values(&a, 0, 1000);
  lv_anim_set_duration(&a, 1400);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

/* --------------------------------- API ----------------------------------- */
void radar_create(lv_obj_t *tile)
{
  tile_obj = tile;
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(tile, tile_clicked_cb, LV_EVENT_SHORT_CLICKED, NULL);

  grid = lv_obj_create(tile);
  ui_plain(grid);
  lv_obj_set_size(grid, UI_W, UI_H);
  lv_obj_set_pos(grid, 0, 0);
  lv_obj_add_event_cb(grid, grid_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

  sweep = lv_obj_create(tile);
  ui_plain(sweep);
  lv_obj_set_size(sweep, UI_W, UI_H);
  lv_obj_set_pos(sweep, 0, 0);
  lv_obj_add_event_cb(sweep, sweep_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

  /* bezel: 120 ticks (every 3 deg), numerals every 30 deg */
  bezel = lv_scale_create(tile);
  lv_obj_set_size(bezel, 468, 468);
  lv_obj_center(bezel);
  lv_scale_set_mode(bezel, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_range(bezel, 0, 120);
  lv_scale_set_angle_range(bezel, 360);
  lv_scale_set_rotation(bezel, 270);
  lv_scale_set_total_tick_count(bezel, 121);
  lv_scale_set_major_tick_every(bezel, 10);
  lv_scale_set_label_show(bezel, true);
  lv_scale_set_text_src(bezel, hour_txt);
  lv_obj_set_style_bg_opa(bezel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_arc_opa(bezel, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_length(bezel, 12, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(bezel, 3, LV_PART_INDICATOR);
  lv_obj_set_style_text_font(bezel, &font_prompt_20, LV_PART_INDICATOR);
  lv_obj_set_style_pad_radial(bezel, 0, LV_PART_INDICATOR);
  lv_obj_set_style_length(bezel, 5, LV_PART_ITEMS);
  lv_obj_set_style_line_width(bezel, 1, LV_PART_ITEMS);
  lv_obj_set_style_line_opa(bezel, 140, LV_PART_ITEMS);
  lv_obj_set_clickable(bezel, false);

  /* targets */
  for (int i = 0; i < N_CONTACTS; i++)
  {
    blip_init(&contacts[i], tile, 6, 0, NULL);
    contacts[i].ang = (float)lv_rand(0, 359);
    contacts[i].r   = (int32_t)lv_rand(40, R_SCOPE - 15);
    blip_place(&contacts[i]);
  }
  /* minute: small, quick;  hour: big, slow and heavy */
  blip_init(&blip_m, tile, 14, LV_OPA_COVER, "นาที");
  blip_init(&blip_h, tile, 28, LV_OPA_COVER, "ชม.");
  blip_m.kind = 2; blip_m.grow = 40;  blip_m.ripple_ms = 650;
  blip_h.kind = 1; blip_h.grow = 110; blip_h.ripple_ms = 1600;
  lv_obj_set_style_border_width(blip_m.dot, 2, 0);
  lv_obj_set_style_border_width(blip_h.dot, 3, 0);
  lv_obj_set_style_border_width(blip_h.ripple, 3, 0);
  lv_obj_set_style_border_width(blip_h.ripple2, 2, 0);
  lv_obj_set_style_text_font(blip_m.tag, &font_prompt_20, 0);
  lv_obj_set_style_text_font(blip_h.tag, &font_prompt_20, 0);
  blip_h.r = R_TRACK;
  blip_m.r = R_TRACK;

  /* read-outs */
  lbl_wday = ui_label(tile, &font_prompt_28, 0, -88);
  lbl_time = ui_label(tile, &font_prompt_clock, 0, -2);
  lbl_date = ui_label(tile, &font_prompt_20, 0, 70);
  lbl_temp = ui_label(tile, &font_prompt_20, 0, 100);
  lbl_hud_r = ui_label(tile, &font_prompt_16, 0, 146);
  lv_obj_set_style_text_opa(lbl_hud_r, 150, 0);

  /* hour / minute marks and their tags are drawn ON TOP of the big digits;
   * the tags get a small background pill so they stay readable over them */
  blip_t *marks[] = { &blip_m, &blip_h };
  for (int i = 0; i < 2; i++)
  {
    lv_obj_set_style_bg_opa(marks[i]->tag, 200, 0);
    lv_obj_set_style_radius(marks[i]->tag, 8, 0);
    lv_obj_set_style_pad_hor(marks[i]->tag, 5, 0);
    lv_obj_move_foreground(marks[i]->ripple);
    lv_obj_move_foreground(marks[i]->ripple2);
    lv_obj_move_foreground(marks[i]->dot);
    lv_obj_move_foreground(marks[i]->tag);
  }

  theme_register(radar_apply_theme);
  intro_exec(NULL, 0);
  radar_intro();
}

void radar_tick(const rtc_datetime_t *dt)
{
  /* beam = seconds hand */
  sweep_ang = ((float)dt->second + (float)dt->ms / 1000.0f) * 6.0f;

  if (dt->minute != last_min || dt->hour != last_hour)
  {
    last_min  = dt->minute;
    last_hour = dt->hour;
    blip_m.ang = (float)dt->minute * 6.0f + (float)dt->second * 0.1f;
    blip_h.ang = (float)(dt->hour % 12U) * 30.0f + (float)dt->minute * 0.5f;
    blip_place(&blip_m);
    blip_place(&blip_h);
    lv_label_set_text_fmt(lbl_time, "%02u:%02u", dt->hour, dt->minute);
  }
  if (dt->day != last_day)
  {
    last_day = dt->day;
    thai_label_set_fmt(lbl_wday, "วัน%s", wd_th[dt->weekday <= 7 ? dt->weekday : 0]);
    thai_label_set_fmt(lbl_date, "%u %s %u", dt->day, mon_th[dt->month <= 12 ? dt->month : 0],
                       dt->year + 543U);                        /* Buddhist era */
  }

  if (intro_k >= 1000)
  {
    if (swept(prev_ang, sweep_ang, blip_h.ang)) blip_ping(&blip_h);
    if (swept(prev_ang, sweep_ang, blip_m.ang)) blip_ping(&blip_m);
    for (int i = 0; i < N_CONTACTS; i++)
    {
      blip_t *b = &contacts[i];
      if (swept(prev_ang, sweep_ang, b->ang)) blip_ping(b);
      else if (b->relocate && lv_obj_get_style_opa(b->dot, 0) == 0) contact_relocate(b);
    }

    /* redraw only the region covered by the old + new beam wedge */
    if (sweep_ang != prev_ang)
    {
      lv_area_t now;
      wedge_area(sweep_ang, &now);
      if (prev_ang >= 0.0f)
      {
        lv_area_t u;
        u.x1 = LV_MIN(now.x1, prev_inv.x1); u.y1 = LV_MIN(now.y1, prev_inv.y1);
        u.x2 = LV_MAX(now.x2, prev_inv.x2); u.y2 = LV_MAX(now.y2, prev_inv.y2);
        lv_obj_invalidate_area(sweep, &u);
      }
      else
      {
        lv_obj_invalidate(sweep);
      }
      prev_inv = now;
    }
    prev_ang = sweep_ang;
  }

  if (dt->second != last_sec)
  {
    last_sec = dt->second;
    thai_label_set_fmt(lbl_hud_r, "วินาที %02u", dt->second);

    /* minute mark creeps forward every 5 s (0.5 deg) instead of jumping 6 deg
     * once a minute - smooth, but far calmer than the seconds sweep */
    if (dt->second % 5U == 0U)
    {
      blip_m.ang = (float)dt->minute * 6.0f + (float)dt->second * 0.1f;
      blip_place(&blip_m);
    }
  }
}

void radar_set_temperature(int ok, int32_t tenths)
{
  char buf[24];
  ui_fmt_temp(buf, sizeof(buf), ok, tenths, 1);
  thai_label_set_fmt(lbl_temp, "อุณหภูมิ %s", buf);
}
