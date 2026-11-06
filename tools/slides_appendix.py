#!/usr/bin/env python3
"""
Insert / refresh the FSM appendix in docs/slides.html from docs/diagrams/fsm-*.html.

The diagrams are inlined (the deck stays a single self-contained file). Marker and
title/desc IDs are already unique per slug except the shared marker ids, which are
prefixed here so several SVGs can live in one document.

Usage: python3 tools/slides_appendix.py
"""
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DECK = os.path.join(ROOT, "docs", "slides.html")
DIAG = os.path.join(ROOT, "docs", "diagrams")
BEGIN, END = "<!-- APPENDIX:BEGIN -->", "<!-- APPENDIX:END -->"

# slug, title, explanation bullets (Thai, brief)
SLIDES = [
    ("fsm-system", "ระบบ: บูต · ทำงาน · หลับ", [
        "บูตเสร็จเข้าสู่ <b>Running</b> ซึ่งเป็นลูปหลักที่วาดจอ ~30 fps",
        "กดปุ่ม USER (หรือไม่แตะจอ 60 s เมื่อ ALWAYS_ON = 0) → <b>Sleep</b> = Stop 2 จอดับ แต่ RTC ยังเดิน",
        "ตื่นได้จากแตะจอ (EXTI8), ปุ่ม (EXTI13) หรือ Alarm A/B → <b>Waking</b> เริ่มสัญญาณนาฬิกาและจอใหม่",
        "ถ้า UART หรือจอเริ่มทำงานไม่สำเร็จ → <b>Fault</b> (Error_Handler)",
    ]),
    ("fsm-alerts", "การแจ้งเตือนบนจอ", [
        "<b>Alarm B</b> ทุกต้นชั่วโมง → Chime ขอบจอกระพริบ 10 s แตะหน้าเรดาร์เพื่อหยุดได้",
        "<b>Alarm A</b> ตามเวลาปลุก → หน้าจอ \"ถึงเวลาแล้ว!\" แตะเพื่อปิด หรือปิดเองใน 60 s",
        "ถ้าปลุกระหว่างตีบอกเวลา จะซ้อนกันชั่วคราว (เส้นประ) จนตีบอกเวลาครบ 10 s",
        "ระหว่างแจ้งเตือน บอร์ดจะไม่เข้าโหมดหลับ",
    ]),
    ("fsm-theme", "สีประจำวัน", [
        "เริ่มที่ <b>Follow day</b>: ใช้สีของวันจาก RTC ทันที",
        "เที่ยงคืน: ค่อย ๆ เปลี่ยนเป็นสีของวันใหม่ (self-loop สีเขียว)",
        "แตะจอ → <b>Preview</b> ดูสีของวันถัดไป แตะต่อเพื่อเลื่อนดูวันอื่น",
        "ไม่แตะ 15 s หรือวันใน RTC เปลี่ยน → กลับเป็นสีของวันนี้",
    ]),
    ("fsm-stopwatch", "นาฬิกาจับเวลา", [
        "<b>Idle → Running</b> ด้วยปุ่มเริ่ม เวลานับจาก HAL_GetTick ละเอียด 0.01 s",
        "หยุด/เริ่มสลับ <b>Running ⇄ Paused</b> เวลาที่สะสมไว้ไม่หาย",
        "รีเซ็ตตอน Paused → กลับ Idle; รีเซ็ตตอน Running → นับใหม่จาก 0 ต่อทันที",
        "ระหว่าง Running บอร์ดจะไม่หลับ",
    ]),
    ("fsm-radar-targets", "เป้าหมายบนเรดาร์", [
        "ลำแสงกวาด = เข็มวินาที เมื่อกวาดผ่านจุดใดจะเกิด \"ping\"",
        "<b>ชม.</b>: วาบขาว + คลื่น 2 วง · <b>นาที</b>: เด้ง + คลื่นวงเล็ก แล้วกลับ Shown",
        "จุดนาทีค่อย ๆ ขยับทุก 5 วินาที",
        "เป้าหมายสุ่ม: Hidden → Fading (สว่างแล้วจาง) → Relocate ไปตำแหน่งใหม่",
    ]),
    ("fsm-touch", "ทัชสกรีน", [
        "อ่านแบบ polling: <b>Released ⇄ Pressed</b> พิกัดกลับ 180° ตามการหมุนจอ",
        "หลังตื่นจาก Stop 2 → <b>Blocked</b> นิ้วที่ใช้ปลุกจะไม่ถูกนับเป็นคลิก",
        "ยกนิ้วแล้วจึงกลับ Released ป้องกันสีธีมเปลี่ยนโดยไม่ตั้งใจ",
    ]),
    ("fsm-rtc-boot", "ตั้งเวลา RTC ตอนบูต", [
        "เปิด LSE (ถ้าไม่ติดใน 2 s ใช้ LSI) แล้ว <b>รอ RSF</b> ก่อนอ่านเวลา",
        "ไม่เคยตั้งเวลา หรือช้ากว่าเวลา build <b>เกิน 10 นาที</b> → ตั้งจากเวลา build",
        "ไม่อย่างนั้น <b>เก็บเวลาเดิม</b> (เกณฑ์นี้แก้บั๊กนาฬิกาช้า 30 s ทุกครั้งที่แฟลช)",
        "สุดท้ายคืนค่า Alarm A/B จาก backup register",
    ]),
]


def svg_of(slug):
    html = open(os.path.join(DIAG, slug + ".html"), encoding="utf-8").read()
    svg = re.search(r"<svg .*?</svg>", html, flags=re.S).group(0)
    for mid in ("arrow-accent", "arrow-link", "arrow"):
        svg = svg.replace('id="%s"' % mid, 'id="%s-%s"' % (slug, mid)).replace("url(#%s)" % mid, "url(#%s-%s)" % (slug, mid))
    return svg


def build(first_no):
    out = [BEGIN, ""]
    n = first_no
    out.append('''<!-- appendix divider -->
<section class="slide">
  <div class="kicker">ภาคผนวก</div>
  <h1 style="margin-top:120px">Finite State Machine</h1>
  <p class="muted">แผนภาพสถานะของเฟิร์มแวร์ 7 ภาพ วาดด้วย skill Diagram Design (Cathryn Lavery) ธีมเรดาร์ <code>radar-thai</code></p>
  <div class="appx-index">%s</div>
  <div class="footer"><span>ภาคผนวก · รายละเอียดเต็มใน docs/state_machine.md</span><span>%02d</span></div>
</section>''' % ("".join('<span class="tag">A%d · %s</span>' % (i + 1, t) for i, (_, t, _) in enumerate(SLIDES)), n))
    for i, (slug, title, bullets) in enumerate(SLIDES):
        n += 1
        out.append('''
<section class="slide">
  <div class="kicker">ภาคผนวก A%d · Finite State Machine</div>
  <h2>%s</h2>
  <div class="appx">
    <div class="appx-fig">%s</div>
    <div class="card appx-note"><h3>อธิบายโดยสังเขป</h3><ul>%s</ul></div>
  </div>
  <div class="footer"><span>ภาคผนวก A%d · docs/diagrams/%s.html</span><span>%02d</span></div>
</section>''' % (i + 1, title, svg_of(slug), "".join("<li>%s</li>" % b for b in bullets), i + 1, slug, n))
    out += ["", END]
    return "\n".join(out)


CSS = '''
  /* ---------- appendix (FSM diagrams) ---------- */
  .appx { display: grid; grid-template-columns: 1fr 330px; gap: 22px; flex: 1; min-height: 0; }
  .appx-fig { min-height: 0; display: flex; align-items: center; justify-content: center;
    border: 1px solid var(--line); border-radius: 12px; background: #050d0a; padding: 8px; overflow: hidden; }
  .appx-fig svg { width: 100%; height: 100%; max-height: 520px; }
  .appx-note ul { padding-left: 1em; }
  .appx-note li { font-size: 16.5px; line-height: 1.5; margin: 7px 0; }
  .appx-index { margin-top: 26px; display: flex; flex-wrap: wrap; max-width: 900px; }
  .appx-index .tag { font-size: 16px; padding: 6px 14px; }
'''

if __name__ == "__main__":
    deck = open(DECK, encoding="utf-8").read()
    if BEGIN in deck:                                         # refresh
        deck = re.sub(re.escape(BEGIN) + r".*?" + re.escape(END), "", deck, flags=re.S)
    main_count = deck.count('<section class="slide">')
    block = build(main_count + 1)
    deck = deck.replace("</div></div>\n\n<div id=\"hint\">", block + "\n\n</div></div>\n\n<div id=\"hint\">", 1)
    if "/* ---------- appendix (FSM diagrams) ---------- */" not in deck:
        deck = deck.replace("  /* ---------- nav ---------- */", CSS + "\n  /* ---------- nav ---------- */", 1)
    fonts = "family=Prompt:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;600"
    if fonts in deck:   # diagrams use Geist / Geist Mono / Instrument Serif / Noto Serif Thai
        deck = deck.replace(fonts, fonts + "&family=Geist:wght@400;500;600&family=Geist+Mono:wght@400;500;600"
                                           "&family=Instrument+Serif:ital@0;1&family=Noto+Serif+Thai:wght@400", 1)
    open(DECK, "w", encoding="utf-8").write(deck)
    print("slides: %d main + %d appendix" % (main_count, len(SLIDES) + 1))
