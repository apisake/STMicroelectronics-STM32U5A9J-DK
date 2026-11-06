/**
 * Minimal Thai tone-mark stacking for LVGL (see thai_text.h)
 */
#include "thai_text.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

/* decode one UTF-8 sequence; returns length (0 at end of string) */
static int u8_decode(const char *s, uint32_t *cp)
{
  const uint8_t *p = (const uint8_t *)s;
  if (p[0] == 0) return 0;
  if (p[0] < 0x80) { *cp = p[0]; return 1; }
  if ((p[0] & 0xE0) == 0xC0 && p[1]) { *cp = ((p[0] & 0x1FU) << 6) | (p[1] & 0x3FU); return 2; }
  if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2])
  {
    *cp = ((p[0] & 0x0FU) << 12) | ((p[1] & 0x3FU) << 6) | (p[2] & 0x3FU);
    return 3;
  }
  *cp = p[0];                                   /* invalid: pass the byte through */
  return 1;
}

static int is_upper_mark(uint32_t c)
{
  return c == 0x0E31 || (c >= 0x0E34 && c <= 0x0E37) || c == 0x0E47 || c == 0x0E4D;
}

size_t thai_shape(const char *in, char *out, size_t out_size)
{
  size_t o = 0;
  uint32_t prev = 0, cp, next;
  int n;

  if (out_size == 0) return 0;
  while ((n = u8_decode(in, &cp)) > 0)
  {
    const char *src = in;
    int len = n;
    char tmp[3];

    if (cp >= 0x0E48 && cp <= 0x0E4C)
    {
      int nn = u8_decode(in + n, &next);
      if (is_upper_mark(prev) || (nn > 0 && next == 0x0E33))
      {
        uint32_t r = 0xF700U + (cp - 0x0E48U);          /* raised tone mark */
        tmp[0] = (char)(0xE0 | (r >> 12));
        tmp[1] = (char)(0x80 | ((r >> 6) & 0x3F));
        tmp[2] = (char)(0x80 | (r & 0x3F));
        src = tmp;
        len = 3;
      }
    }
    if (o + (size_t)len >= out_size) break;
    for (int i = 0; i < len; i++) out[o++] = src[i];
    prev = cp;
    in += n;
  }
  out[o] = '\0';
  return o;
}

void thai_label_set_text(lv_obj_t *label, const char *txt)
{
  static char buf[256];
  thai_shape(txt, buf, sizeof(buf));
  lv_label_set_text(label, buf);                /* LVGL copies the string */
}

void thai_label_set_fmt(lv_obj_t *label, const char *fmt, ...)
{
  static char tmp[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  thai_label_set_text(label, tmp);
}
