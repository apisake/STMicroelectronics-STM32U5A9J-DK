#!/usr/bin/env python3
"""
Generate the FSM diagrams in docs/diagrams/ following the Diagram Design skill
(cathrynlavery/diagram-design) with the project's "radar-thai" profile:
dark radar paper, radar-green accent, ice-blue link, Thai labels in Prompt.

Rules applied (SKILL.md §6/§7/§9): 4px grid, orthogonal r=8 elbows, arrows drawn
before boxes, masked ALL-CAPS arrow labels with an 8px gap, <=9 nodes / <=12
transitions, <=2 accent elements, bottom legend strip, accessible <title>/<desc>.

Usage: python3 tools/fsm_diagrams.py            -> writes docs/diagrams/fsm-*.html
"""
import math
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "diagrams")

# ---------------------------------------------------------------- radar-thai tokens (dark)
PAPER, PAPER2 = "#050d0a", "#0b1a14"
INK, MUTED, SOFT = "#e6f4ee", "#8fb3a5", "#7f9e91"
ACCENT, ACCENT_TINT, LINK = "#22ff7a", "rgba(34,255,122,0.10)", "#2ed3ff"
RULE = "rgba(230,244,238,0.12)"
F_NAME = "'Geist','Prompt',system-ui,sans-serif"
F_THAI = "'Prompt','Geist',system-ui,sans-serif"
F_MONO = "'Geist Mono',ui-monospace,monospace"
F_SERIF = "'Instrument Serif','Noto Serif Thai',serif"
FONT_LINK = ("https://fonts.googleapis.com/css2?family=Instrument+Serif:ital@0;1&family=Geist:wght@400;500;600"
             "&family=Geist+Mono:wght@400;500;600&family=Prompt:wght@400;500;600&family=Noto+Serif+Thai:wght@400&display=swap")


def c4(v):
    """round up to the 4px grid"""
    return int(math.ceil(v / 4.0) * 4)


def text_w(text, size):
    """approx. width: Thai above/below marks take no horizontal space"""
    marks = set(range(0x0E31, 0x0E32)) | set(range(0x0E34, 0x0E3B)) | set(range(0x0E47, 0x0E4F))
    return sum(0 if ord(ch) in marks else 0.58 for ch in text) * size


def label_w(text):
    # 8px mono, 0.62em per char + 0.06em tracking, 4px padding each side
    return c4(len(text) * 8 * 0.68 + 8)


class Svg:
    def __init__(self, slug, w, h, title, desc):
        self.slug, self.w, self.h, self.title, self.desc = slug, w, h, title, desc
        self.arrows, self.labels, self.nodes, self.extra = [], [], [], []

    # ------------------------------------------------------------ connectors
    def path(self, pts, color=MUTED, dashed=False, marker="arrow", width=1.2):
        d = "M %d %d" % pts[0]
        r = 8
        for i in range(1, len(pts) - 1):
            (x0, y0), (x1, y1), (x2, y2) = pts[i - 1], pts[i], pts[i + 1]
            dx1, dy1 = (x1 - x0) and (1 if x1 > x0 else -1), (y1 - y0) and (1 if y1 > y0 else -1)
            dx2, dy2 = (x2 - x1) and (1 if x2 > x1 else -1), (y2 - y1) and (1 if y2 > y1 else -1)
            sweep = 1 if dx1 * dy2 - dy1 * dx2 > 0 else 0
            d += " L %d %d A %d %d 0 0 %d %d %d" % (x1 - dx1 * r, y1 - dy1 * r, r, r, sweep, x1 + dx2 * r, y1 + dy2 * r)
        d += " L %d %d" % pts[-1]
        dash = ' stroke-dasharray="5,4"' if dashed else ""
        self.arrows.append('<path d="%s" fill="none" stroke="%s" stroke-width="%s"%s marker-end="url(#%s)"/>'
                           % (d, color, width, dash, marker))

    def self_loop(self, cx, top, text, color=MUTED, marker="arrow"):
        x1, x2 = cx - 28, cx + 28
        self.arrows.append('<path d="M %d %d C %d %d, %d %d, %d %d" fill="none" stroke="%s" stroke-width="1.2" marker-end="url(#%s)"/>'
                           % (x1, top, x1, top - 48, x2, top - 48, x2, top, color, marker))
        self.label(cx, top - 36 - 8 - 12, text, color=SOFT if color == MUTED else color)

    def label(self, cx, mask_y, text, color=SOFT):
        w = label_w(text)
        self.labels.append('<rect x="%d" y="%d" width="%d" height="12" rx="2" fill="%s"/>' % (cx - w // 2, mask_y, w, PAPER))
        self.labels.append('<text x="%d" y="%d" fill="%s" font-size="8" font-family="%s" text-anchor="middle" letter-spacing="0.06em">%s</text>'
                           % (cx, mask_y + 9, color, F_MONO, text))

    def label_side(self, x_edge, cy, text, side="right", color=SOFT):
        w = label_w(text)
        x = x_edge + 8 if side == "right" else x_edge - 8 - w
        self.labels.append('<rect x="%d" y="%d" width="%d" height="12" rx="2" fill="%s"/>' % (x, cy - 6, w, PAPER))
        self.labels.append('<text x="%d" y="%d" fill="%s" font-size="8" font-family="%s" text-anchor="middle" letter-spacing="0.06em">%s</text>'
                           % (x + w // 2, cy + 3, color, F_MONO, text))

    # ----------------------------------------------------------------- nodes
    def state(self, x, y, w, h, name, sub, kind="state", tag="STATE"):
        if kind == "focal":
            fill, stroke, dash, tagc = ACCENT_TINT, ACCENT, "", ACCENT
        elif kind == "fault":
            fill, stroke, dash, tagc = "rgba(230,244,238,0.02)", "rgba(230,244,238,0.30)", ' stroke-dasharray="4,3"', MUTED
        else:
            fill, stroke, dash, tagc = PAPER2, INK, "", MUTED
        cx = x + w // 2
        tw = c4(len(tag) * 7 * 0.68 + 10)
        self.nodes += [
            '<rect x="%d" y="%d" width="%d" height="%d" rx="8" fill="%s"/>' % (x, y, w, h, PAPER),
            '<rect x="%d" y="%d" width="%d" height="%d" rx="8" fill="%s" stroke="%s" stroke-width="1"%s/>' % (x, y, w, h, fill, stroke, dash),
            '<rect x="%d" y="%d" width="%d" height="12" rx="2" fill="transparent" stroke="%s" stroke-opacity="0.5" stroke-width="0.8"/>' % (x + 8, y + 8, tw, tagc),
            '<text x="%d" y="%d" fill="%s" font-size="7" font-family="%s" text-anchor="middle" letter-spacing="0.08em">%s</text>' % (x + 8 + tw // 2, y + 17, tagc, F_MONO, tag),
            '<text x="%d" y="%d" fill="%s" font-size="14" font-weight="600" font-family="%s" text-anchor="middle">%s</text>' % (cx, y + h // 2 + 6, INK, F_NAME, name),
            '<text x="%d" y="%d" fill="%s" font-size="11" font-family="%s" text-anchor="middle">%s</text>' % (cx, y + h // 2 + 24, MUTED, F_THAI, sub),
        ]

    def step(self, x, y, w, h, name, sub=None, oval=False, kind="step"):
        stroke = ACCENT if kind == "focal" else INK
        fill = ACCENT_TINT if kind == "focal" else PAPER2
        rx = h // 2 if oval else 6
        cx, cy = x + w // 2, y + h // 2
        self.nodes += ['<rect x="%d" y="%d" width="%d" height="%d" rx="%d" fill="%s"/>' % (x, y, w, h, rx, PAPER),
                       '<rect x="%d" y="%d" width="%d" height="%d" rx="%d" fill="%s" stroke="%s" stroke-width="1"/>' % (x, y, w, h, rx, fill, stroke)]
        if sub:
            self.nodes.append('<text x="%d" y="%d" fill="%s" font-size="13" font-weight="600" font-family="%s" text-anchor="middle">%s</text>' % (cx, cy - 2, INK, F_NAME, name))
            self.nodes.append('<text x="%d" y="%d" fill="%s" font-size="11" font-family="%s" text-anchor="middle">%s</text>' % (cx, cy + 14, MUTED, F_THAI, sub))
        else:
            self.nodes.append('<text x="%d" y="%d" fill="%s" font-size="13" font-weight="600" font-family="%s" text-anchor="middle">%s</text>' % (cx, cy + 5, INK, F_NAME, name))

    def diamond(self, cx, cy, hw, hh, l1, l2, kind="step"):
        stroke = ACCENT if kind == "focal" else INK
        fill = ACCENT_TINT if kind == "focal" else PAPER2
        pts = "%d,%d %d,%d %d,%d %d,%d" % (cx, cy - hh, cx + hw, cy, cx, cy + hh, cx - hw, cy)
        self.nodes += ['<polygon points="%s" fill="%s"/>' % (pts, PAPER),
                       '<polygon points="%s" fill="%s" stroke="%s" stroke-width="1"/>' % (pts, fill, stroke),
                       '<text x="%d" y="%d" fill="%s" font-size="12" font-weight="600" font-family="%s" text-anchor="middle">%s</text>' % (cx, cy - 2, INK, F_NAME, l1),
                       '<text x="%d" y="%d" fill="%s" font-size="11" font-family="%s" text-anchor="middle">%s</text>' % (cx, cy + 14, MUTED, F_THAI, l2)]

    def start(self, cx, cy):
        self.nodes.append('<circle cx="%d" cy="%d" r="6" fill="%s"/>' % (cx, cy, INK))

    def dot(self, cx, cy):
        self.nodes.append('<circle cx="%d" cy="%d" r="4" fill="%s"/>' % (cx, cy, INK))

    def zone(self, x, y, text):
        self.extra.append('<text x="%d" y="%d" fill="%s" font-size="8" font-weight="500" font-family="%s" letter-spacing="0.18em">%s</text>' % (x, y, SOFT, F_MONO, text))

    def callout(self, x, y, text, anchor="start"):
        self.nodes.append('<text x="%d" y="%d" fill="%s" font-size="14" font-style="italic" font-family="%s" text-anchor="%s">%s</text>' % (x, y, MUTED, F_SERIF, anchor, text))

    def legend(self, y, items):
        out = ['<line x1="32" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="0.8"/>' % (y - 8, self.w - 32, y - 8, RULE),
               '<text x="32" y="%d" fill="%s" font-size="8" font-family="%s" letter-spacing="0.14em">LEGEND</text>' % (y + 12, MUTED, F_MONO)]
        x = 120
        for kind, text in items:
            if kind == "focal":
                out.append('<rect x="%d" y="%d" width="20" height="12" rx="3" fill="%s" stroke="%s"/>' % (x, y + 2, ACCENT_TINT, ACCENT))
            elif kind == "state":
                out.append('<rect x="%d" y="%d" width="20" height="12" rx="3" fill="%s" stroke="%s"/>' % (x, y + 2, PAPER2, INK))
            elif kind == "fault":
                out.append('<rect x="%d" y="%d" width="20" height="12" rx="3" fill="none" stroke="rgba(230,244,238,0.30)" stroke-dasharray="4,3"/>' % (x, y + 2))
            elif kind == "decision":
                out.append('<polygon points="%d,%d %d,%d %d,%d %d,%d" fill="%s" stroke="%s"/>' % (x + 10, y, x + 20, y + 8, x + 10, y + 16, x, y + 8, PAPER2, INK))
            elif kind == "oval":
                out.append('<rect x="%d" y="%d" width="20" height="12" rx="6" fill="%s" stroke="%s"/>' % (x, y + 2, PAPER2, INK))
            elif kind == "arrow":
                out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.2" marker-end="url(#arrow)"/>' % (x, y + 8, x + 20, y + 8, MUTED))
            elif kind == "dashed":
                out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.2" stroke-dasharray="5,4" marker-end="url(#arrow)"/>' % (x, y + 8, x + 20, y + 8, MUTED))
            elif kind == "start":
                out.append('<circle cx="%d" cy="%d" r="5" fill="%s"/>' % (x + 10, y + 8, INK))
            out.append('<text x="%d" y="%d" fill="%s" font-size="11" font-family="%s">%s</text>' % (x + 28, y + 12, MUTED, F_THAI, text))
            x += 28 + c4(text_w(text, 11)) + 28
        self.extra += out

    # ----------------------------------------------------------------- output
    def html(self, eyebrow, h1):
        s = self.slug
        svg = "\n".join([
            '<svg viewBox="0 0 %d %d" xmlns="http://www.w3.org/2000/svg" role="img" aria-labelledby="%s-title %s-desc">' % (self.w, self.h, s, s),
            '<title id="%s-title">%s</title>' % (s, self.title),
            '<desc id="%s-desc">%s</desc>' % (s, self.desc),
            '<defs>',
            '<marker id="arrow" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto"><polygon points="0 0, 8 3, 0 6" fill="%s"/></marker>' % MUTED,
            '<marker id="arrow-accent" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto"><polygon points="0 0, 8 3, 0 6" fill="%s"/></marker>' % ACCENT,
            '<marker id="arrow-link" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto"><polygon points="0 0, 8 3, 0 6" fill="%s"/></marker>' % LINK,
            '</defs>',
            '<rect width="100%%" height="100%%" fill="%s"/>' % PAPER,
            "<!-- transitions first, then labels, then nodes -->",
            *self.arrows, *self.labels, *self.nodes, *self.extra,
            '</svg>'])
        return """<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>%(title)s</title>
<link href="%(font)s" rel="stylesheet">
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  :root { --color-paper: %(paper)s; --color-ink: %(ink)s; --color-muted: %(muted)s; }
  body { font-family: %(fname)s; background: var(--color-paper); color: var(--color-ink);
         min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 3rem 2rem; }
  .frame { max-width: 1200px; width: 100%%; }
  .diagram-container { width: 100%%; overflow-x: auto; }
  .eyebrow { font-family: %(fmono)s; font-size: 0.66rem; font-weight: 500; letter-spacing: 0.18em;
             text-transform: uppercase; color: var(--color-muted); margin-bottom: 0.5rem; }
  h1 { font-family: %(fserif)s; font-size: clamp(1.5rem, 2.4vw + 0.75rem, 2rem); font-weight: 400;
       letter-spacing: -0.02em; line-height: 1.25; color: var(--color-ink); margin-bottom: 1.5rem; }
  svg { width: 100%%; min-width: %(w)dpx; display: block; }
  @media print { .diagram-container { overflow-x: visible; } svg { min-width: 0; } }
</style>
</head>
<body>
<div class="frame">
<p class="eyebrow">%(eyebrow)s</p>
<h1>%(h1)s</h1>
<div class="diagram-container">
%(svg)s
</div>
</div>
</body>
</html>
""" % dict(title=self.title, font=FONT_LINK, paper=PAPER, ink=INK, muted=MUTED, fname=F_NAME, fmono=F_MONO,
           fserif=F_SERIF, w=self.w, eyebrow=eyebrow, h1=h1, svg=svg)


# ============================================================== diagrams
def d_system():
    g = Svg("fsm-system", 960, 520, "System power states",
            "State machine of the firmware: boot, running, Stop 2 sleep, waking and the fault state.")
    W, H = 176, 72
    g.path([(58, 196), (96, 196)])
    g.path([(272, 196), (380, 196)], color=ACCENT, marker="arrow-accent", width=1.4)
    g.label(326, 176, "INIT OK", color=ACCENT)
    g.path([(556, 196), (676, 196)])
    g.label(616, 176, "BUTTON / IDLE")
    g.path([(764, 232), (764, 364), (704, 364)])
    g.label_side(764, 296, "EXTI / ALARM", "right")
    g.path([(528, 364), (468, 364), (468, 232)])
    g.label_side(468, 296, "RE-INIT", "left")
    g.path([(184, 232), (184, 328)], dashed=True)
    g.label_side(184, 280, "INIT FAIL", "right")
    g.start(52, 196)
    g.state(96, 160, W, H, "Boot", "เริ่มต้นฮาร์ดแวร์ + UI")
    g.state(380, 160, W, H, "Running", "จอเปิด · ลูปหลัก ~30 fps", kind="focal")
    g.state(676, 160, W, H, "Sleep", "Stop 2 · จอดับ · RTC เดิน")
    g.state(528, 328, W, H, "Waking", "คืนสัญญาณนาฬิกา + จอ")
    g.state(96, 328, W, H, "Fault", "Error_Handler() วนค้าง", kind="fault")
    g.legend(464, [("start", "เริ่ม"), ("focal", "สถานะหลัก"), ("state", "สถานะ"), ("fault", "ผิดพลาด"),
                   ("arrow", "เปลี่ยนสถานะ")])
    return g, "State machine · ระบบ", "ระบบ: บูต · ทำงาน · หลับ (Stop 2)"


def d_alerts():
    g = Svg("fsm-alerts", 960, 600, "On-screen alerts",
            "State machine of the hourly chime and daily alarm overlays and how they interact.")
    W, H = 176, 72
    g.path([(58, 280), (96, 280)])
    g.path([(152, 244), (152, 128), (420, 128)])                       # NORMAL -> CHIME
    g.label(286, 108, "ALARM B")
    g.path([(420, 152), (212, 152), (212, 244)], dashed=True)          # CHIME -> NORMAL
    g.label(316, 160, "10 S / TAP")
    g.path([(152, 316), (152, 432), (420, 432)], color=ACCENT, marker="arrow-accent", width=1.4)  # NORMAL -> ALARM
    g.label(286, 440, "ALARM A", color=ACCENT)
    g.path([(420, 408), (212, 408), (212, 316)], dashed=True)          # ALARM -> NORMAL
    g.label(316, 388, "TAP / 60 S")
    g.path([(596, 128), (776, 128), (776, 244)])                       # CHIME -> UNDER
    g.label(686, 108, "ALARM A")
    g.path([(716, 268), (536, 268), (536, 176)], dashed=True)          # UNDER -> CHIME
    g.label(626, 276, "TAP")
    g.path([(804, 316), (804, 420), (596, 420)])                       # UNDER -> ALARM
    g.label_side(804, 368, "CHIME 10 S", "right")
    g.start(52, 280)
    g.state(96, 244, W, H, "Normal", "ไม่มีการแจ้งเตือน")
    g.state(420, 104, W, H, "Chime", "ขอบจอกระพริบ 10 วินาที")
    g.state(420, 384, W, H, "Alarm", "ถึงเวลาแล้ว! · LED3", kind="focal")
    g.state(716, 244, W, H, "Chime + Alarm", "กระพริบต่อใต้หน้าจอปลุก", kind="fault", tag="OVERLAP")
    g.callout(612, 500, "Alarm B ระหว่างปลุก: ไม่ตีบอกเวลา")
    g.legend(544, [("start", "เริ่ม"), ("focal", "สำคัญที่สุด"), ("state", "สถานะ"), ("fault", "ซ้อนกันชั่วคราว"),
                   ("dashed", "กลับ")])
    return g, "State machine · การแจ้งเตือนบนจอ", "ตีบอกเวลา และนาฬิกาปลุก"


def d_theme():
    g = Svg("fsm-theme", 960, 380, "Colour of the day",
            "State machine of the Thai colour-of-the-day theme with a tap preview that times out after 15 seconds.")
    W, H = 176, 72
    g.path([(170, 196), (220, 196)])
    g.path([(396, 184), (560, 184)])
    g.label(478, 164, "TAP")
    g.path([(560, 208), (396, 208)], dashed=True)
    g.label(478, 216, "15 S / NEW DAY")
    g.self_loop(308, 160, "MIDNIGHT", color=ACCENT, marker="arrow-accent")
    g.self_loop(648, 160, "TAP")
    g.start(164, 196)
    g.state(220, 160, W, H, "Follow day", "สีประจำวันจาก RTC", kind="focal")
    g.state(560, 160, W, H, "Preview", "ดูสีของวันถัดไป")
    g.callout(480, 300, "ทุกการเปลี่ยนสี ค่อย ๆ ไล่สีใน 0.6 วินาที", anchor="middle")
    g.legend(328, [("start", "เริ่ม (ใช้สีวันนี้ทันที)"), ("focal", "สถานะหลัก"), ("state", "สถานะ"), ("dashed", "กลับเอง")])
    return g, "State machine · สีธีม", "สีประจำวัน และการดูสีของวันอื่น"


def d_stopwatch():
    g = Svg("fsm-stopwatch", 960, 400, "Stopwatch",
            "State machine of the stopwatch page: idle, running and paused with start, stop and reset.")
    W, H = 176, 72
    g.path([(58, 196), (96, 196)])
    g.path([(272, 196), (392, 196)], color=ACCENT, marker="arrow-accent", width=1.4)
    g.label(332, 176, "START", color=ACCENT)
    g.path([(568, 184), (688, 184)])
    g.label(628, 164, "STOP")
    g.path([(688, 208), (568, 208)])
    g.label(628, 216, "START")
    g.path([(776, 232), (776, 296), (184, 296), (184, 232)], dashed=True)
    g.label(480, 304, "RESET")
    g.self_loop(480, 160, "RESET")
    g.start(52, 196)
    g.state(96, 160, W, H, "Idle", "00:00.00")
    g.state(392, 160, W, H, "Running", "นับเวลา · ห้ามหลับ", kind="focal")
    g.state(688, 160, W, H, "Paused", "เก็บเวลาที่สะสมไว้")
    g.legend(344, [("start", "เริ่ม"), ("focal", "กำลังนับ"), ("state", "สถานะ"), ("dashed", "รีเซ็ตกลับ")])
    return g, "State machine · นาฬิกาจับเวลา", "จับเวลา: เริ่ม · หยุด · รีเซ็ต"


def d_targets():
    g = Svg("fsm-radar-targets", 960, 560, "Radar targets",
            "Two small state machines: the hour and minute marks pinging when the sweep passes, and the ambient contacts that fade and relocate.")
    W, H = 176, 72
    g.zone(96, 72, "HOUR / MINUTE MARKS")
    g.path([(392, 120), (272, 120)], color=ACCENT, marker="arrow-accent", width=1.4)
    g.label(332, 100, "SWEEP HR", color=ACCENT)
    g.path([(272, 144), (392, 144)], dashed=True)
    g.label(332, 152, "FLASH 0.9 S")
    g.path([(568, 120), (688, 120)])
    g.label(628, 100, "SWEEP MIN")
    g.path([(688, 144), (568, 144)], dashed=True)
    g.label(628, 152, "POP 0.4 S")
    g.zone(96, 272, "AMBIENT CONTACTS")
    g.path([(272, 332), (392, 332)])
    g.label(332, 312, "SWEEP")
    g.path([(568, 332), (688, 332)])
    g.label(628, 312, "OPACITY 0")
    g.path([(776, 368), (776, 432), (184, 432), (184, 368)], dashed=True)
    g.label(480, 440, "RANDOM POS")
    g.state(96, 96, W, H, "Ping hour", "วาบขาว + คลื่น 2 วง", kind="focal")
    g.state(392, 96, W, H, "Shown", "ขยับจุดนาทีทุก 5 วินาที")
    g.state(688, 96, W, H, "Ping minute", "เด้ง + คลื่นวงเล็ก")
    g.state(96, 296, W, H, "Hidden", "opacity 0")
    g.state(392, 296, W, H, "Fading", "สว่างแล้วจาง 3.8 วินาที")
    g.state(688, 296, W, H, "Relocate", "สุ่มมุม 60–240°")
    g.legend(504, [("focal", "เอฟเฟกต์เด่น"), ("state", "สถานะ"), ("arrow", "ลำแสงกวาดผ่าน"), ("dashed", "animation จบ")])
    return g, "State machine · เป้าหมายบนเรดาร์", "จุดบอกเวลา และเป้าหมายประกอบฉาก"


def d_touch():
    g = Svg("fsm-touch", 960, 400, "Touch input",
            "State machine of the polled touch input, including the blocked state that ignores the finger used to wake the board.")
    W, H = 176, 72
    g.path([(480, 110), (480, 160)])
    g.path([(568, 184), (688, 184)])
    g.label(628, 164, "TOUCH")
    g.path([(688, 208), (568, 208)])
    g.label(628, 216, "LIFT")
    g.path([(392, 184), (272, 184)], color=ACCENT, marker="arrow-accent", width=1.4)
    g.label(332, 164, "WAKE", color=ACCENT)
    g.path([(272, 208), (392, 208)], dashed=True)
    g.label(332, 216, "LIFT")
    g.self_loop(184, 160, "STILL HELD")
    g.start(480, 104)
    g.state(392, 160, W, H, "Released", "ไม่มีการแตะ")
    g.state(688, 160, W, H, "Pressed", "พิกัด 479−x, 479−y")
    g.state(96, 160, W, H, "Blocked", "นิ้วที่ใช้ปลุก ≠ คลิก", kind="focal")
    g.legend(344, [("start", "เริ่ม"), ("focal", "หลังตื่นจาก Stop 2"), ("state", "สถานะ"), ("dashed", "ยกนิ้ว")])
    return g, "State machine · ทัชสกรีน", "ทัชสกรีน และการกันคลิกหลังตื่น"


def d_rtc():
    g = Svg("fsm-rtc-boot", 960, 720, "RTC boot decision",
            "Flowchart of how the firmware decides at boot whether to keep the RTC time or set it from the build time.")
    sx = 360
    g.path([(sx, 72), (sx, 104)])
    g.path([(sx, 152), (sx, 184)])
    g.path([(sx, 232), (sx, 260)])
    g.path([(sx, 340), (sx, 400)])                                    # D1 no -> SET
    g.label_side(sx, 370, "NO", "right")
    g.path([(424, 300), (536, 300)])                                  # D1 yes -> D2
    g.label(480, 280, "YES")
    g.path([(664, 300), (744, 300)], color=ACCENT, marker="arrow-accent", width=1.4)  # D2 yes -> KEEP
    g.label(704, 280, "YES", color=ACCENT)
    g.path([(600, 340), (600, 424), (440, 424)])                      # D2 no -> SET
    g.label_side(600, 382, "NO", "right")
    g.path([(sx, 448), (sx, 488), (596, 488)])
    g.path([(824, 324), (824, 488), (604, 488)], color=ACCENT, marker="arrow-accent", width=1.4)
    g.path([(600, 492), (600, 512)])
    g.path([(600, 560), (600, 592)])
    g.step(300, 40, 120, 32, "rtc_clock_init", oval=True)
    g.step(280, 104, 160, 48, "Start LSE", "ไม่ติดใน 2 s → ใช้ LSI")
    g.step(280, 184, 160, 48, "Init + RSF sync", "HAL_RTC_WaitForSynchro")
    g.diamond(sx, 300, 64, 40, "Configured?", "DR0 = magic")
    g.diamond(600, 300, 64, 40, "≤ 10 min behind", "เวลาที่ build", kind="focal")
    g.step(280, 400, 160, 48, "Set from build", "__DATE__ __TIME__")
    g.step(744, 276, 160, 48, "Keep RTC time", "เวลาเดิมถูกต้อง")
    g.dot(600, 488)
    g.step(520, 512, 160, 48, "Restore alarms", "Alarm A (DR3) · B (DR4)")
    g.step(552, 592, 96, 32, "RTC ready", oval=True)
    g.callout(40, 600, "แก้บั๊ก: เดิมตั้งใหม่ทุกครั้งที่ช้ากว่า build")
    g.legend(664, [("oval", "เริ่ม / จบ"), ("state", "ขั้นตอน"), ("decision", "ตัดสินใจ"), ("focal", "เกณฑ์ 10 นาที")])
    return g, "Flowchart · RTC", "ตั้งเวลา RTC ตอนบูต: เก็บไว้หรือตั้งใหม่"


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for fn in (d_system, d_alerts, d_theme, d_stopwatch, d_targets, d_touch, d_rtc):
        g, eyebrow, h1 = fn()
        path = os.path.join(OUT, g.slug + ".html")
        with open(path, "w", encoding="utf-8") as f:
            f.write(g.html(eyebrow, h1))
        print("wrote", os.path.relpath(path))
