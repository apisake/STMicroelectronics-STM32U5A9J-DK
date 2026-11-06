# Finite State Machine: นาฬิกาเรดาร์ STM32U5A9J-DK

เอกสารนี้อธิบายสถานะ (state) และการเปลี่ยนสถานะ (transition) ของเฟิร์มแวร์
บอร์ด **STMicroelectronics STM32U5A9J-DK** (<https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html>)
อ้างอิงจากซอร์สโค้ดรุ่น 1.1.0 แผนภาพเขียนด้วย Mermaid (GitHub แสดงเป็นภาพให้อัตโนมัติ)

โปรแกรมเป็นแบบ **super-loop + interrupt** ไม่มี RTOS ระบบประกอบด้วย FSM หลายตัวที่ทำงานซ้อนกัน:

| # | FSM | อยู่ในไฟล์ | ตัวแปรสถานะ |
|---|---|---|---|
| 1 | ระบบ (บูต / ทำงาน / หลับ) | `main.c` | `in_stop`, `sleep_request` |
| 2 | ตัวจัดเวลางานใน main loop | `main.c` | `t_ui`, `t_temp`, `t_log`, `last_sec` |
| 3 | การแจ้งเตือนบนจอ (ปกติ / ตีบอกเวลา / ปลุก) | `ui_main.c` | `chime_on`, `alert_on` |
| 4 | สีธีม (ตามวัน / ดูตัวอย่าง) | `ui_theme.c` | `today`, `idx`, `preview_since` |
| 5 | หน้าจอ (ปัดซ้าย/ขวา) | `ui_main.c` | `lv_tileview` tile ที่แสดงอยู่ |
| 6 | นาฬิกาจับเวลา | `ui_pages_misc.c` | `ch_run`, `ch_acc` |
| 7 | เป้าหมายบนเรดาร์ (ping) | `ui_radar.c` | animation ของแต่ละจุด, `relocate` |
| 8 | ทัชสกรีน | `lvgl_port.c` | `touch_block` |
| 9 | ตัวรับคำสั่ง UART | `main.c` | `rx_len`, `cmd_ready` |
| 10 | การตัดสินใจตั้งเวลา RTC ตอนบูต | `rtc_clock.c` | backup register DR0 |

---

## 1. FSM ระบบ: บูต → ทำงาน ⇄ หลับ (Stop 2)

```mermaid
stateDiagram-v2
    direction LR
    [*] --> BOOT
    BOOT --> FAULT : BSP_COM_Init / lvgl_port_init ล้มเหลว
    BOOT --> RUNNING : เริ่มต้นครบทุกส่วน
    RUNNING --> SLEEP : กดปุ่ม USER (sleep_request) · หรือ ALWAYS_ON=0 และไม่แตะจอ > 60 s และ !ui_busy()
    SLEEP --> WAKING : แตะจอ (EXTI8) / ปุ่ม USER (EXTI13) · / RTC Alarm A (ปลุก) / RTC Alarm B (ตีบอกเวลา)
    WAKING --> RUNNING : คืนค่าสัญญาณนาฬิกา + เปิดจอใหม่
    FAULT --> [*] : Error_Handler() วนค้าง

    state BOOT {
        direction TB
        b1: HAL_Init + SystemClock 160 MHz (MSI→PLL, เปิด HSE)
        b2: LED + UART (USART1 115200)
        b3: UART RX interrupt
        b4: RTC (LSE→LSI) + Alarm A/B
        b5: GPU2D + LCD + Touch + LVGL
        b6: เซนเซอร์ STTS22H
        b7: ปุ่ม USER (EXTI)
        b8: สร้าง UI 4 หน้า + ภาพเปิดเครื่อง
        b1 --> b2
        b2 --> b3
        b3 --> b4
        b4 --> b5
        b5 --> b6
        b6 --> b7
        b7 --> b8
    }

    state WAKING {
        direction TB
        w1: SystemClock_Config()
        w2: HAL_ResumeTick()
        w3: BSP_TS_DisableIT + touch_block = 1
        w4: BSP_LCD_Init() ใหม่ (PLL3, DSI, LTDC)
        w5: วาดจอใหม่ทั้งหมด + ภาพเปิดเครื่อง
        w1 --> w2
        w2 --> w3
        w3 --> w4
        w4 --> w5
    }
```

| จาก | ไป | เงื่อนไข / เหตุการณ์ | การทำงาน |
|---|---|---|---|
| BOOT | RUNNING | init สำเร็จ | เข้าสู่ main loop |
| BOOT | FAULT | `BSP_COM_Init` หรือ `lvgl_port_init` คืนค่าผิดพลาด | `Error_Handler()` |
| RUNNING | SLEEP | `sleep_request` (ปุ่ม USER) | `Power_Sleep()` |
| RUNNING | SLEEP | `ALWAYS_ON == 0` และ `lv_display_get_inactive_time() > 60000` และ `!ui_busy()` | `Power_Sleep()` |
| SLEEP | WAKING | interrupt: EXTI8 (touch), EXTI13 (ปุ่ม), RTC Alarm A/B | MCU ออกจาก `HAL_PWREx_EnterSTOP2Mode()` |
| WAKING | RUNNING | init ใหม่เสร็จ | พิมพ์ `wake: touch/button/alarm` |

**ก่อนเข้า SLEEP** (`Power_Sleep`):
1. รอให้ UART ส่งข้อมูลเสร็จ (`UART_FLAG_TC`)
2. `BSP_LCD_DisplayOff` + `BSP_LCD_DeInit`
3. `BSP_TS_EnableIT` ให้ขา INT ของทัชสกรีนปลุก MCU ได้
4. `HAL_SuspendTick()` แล้วเข้า Stop 2

**ระหว่าง SLEEP:** RTC เดินต่อ และ SRAM ทั้งหมดยังเก็บค่าไว้ แต่ UART รับคำสั่งไม่ได้

`ui_busy()` เป็นจริงเมื่อ: นาฬิกาจับเวลากำลังเดิน **หรือ** หน้าจอแจ้งเตือนปลุกเปิดอยู่ **หรือ** กำลังตีบอกเวลา

---

## 2. ตัวจัดเวลางานใน main loop (สถานะ RUNNING)

main loop ไม่มีการ block (ไม่ใช้ `HAL_Delay`) แต่ละรอบจะเช็กเวลาแล้วทำงานที่ถึงกำหนด:

```mermaid
flowchart TD
    L(["เริ่มรอบ: now = HAL_GetTick"]) --> A{"now - t_ui ≥ 33 ms?"}
    A -- ใช่ --> A1["อ่าน RTC + ui_tick<br/>ลำแสง, จุดบอกเวลา, สีประจำวัน, จับเวลา, LED3"]
    A1 --> A2{"วินาทีเปลี่ยน?"}
    A2 -- ใช่ --> A3["สลับ LED4 + พิมพ์ log<br/>เวลา, อุณหภูมิ, fps"]
    A2 -- ไม่ --> B
    A3 --> B
    A -- ไม่ --> B{"now - t_temp ≥ 2 s?"}
    B -- ใช่ --> B1["อ่าน STTS22H → แสดงบนเรดาร์"]
    B -- ไม่ --> C
    B1 --> C{"now - t_log ≥ 10 s?"}
    C -- ใช่ --> C1["เพิ่มจุดในกราฟอุณหภูมิ"]
    C -- ไม่ --> D
    C1 --> D{"chime_fired?<br/>Alarm B"}
    D -- ใช่ --> D1["ui_chime ชั่วโมง"]
    D -- ไม่ --> E
    D1 --> E{"alarm_fired?<br/>Alarm A"}
    E -- ใช่ --> E1["ui_alarm_alert"]
    E -- ไม่ --> F
    E1 --> F{"ต้องหลับ?"}
    F -- ใช่ --> S[["Power_Sleep → FSM 1"]]
    S --> L
    F -- ไม่ --> G["Uart_Poll_Command"]
    G --> H["lv_timer_handler<br/>วาดจอ + animation + อ่านทัช"]
    H --> L
```

| งาน | รอบ | หมายเหตุ |
|---|---|---|
| UI tick | 33 ms (~30 Hz) | ลำแสงกวาดคำนวณจากวินาทีและมิลลิวินาทีของ RTC |
| Log + LED4 | ทุกวินาที (ตามวินาทีของ RTC) | `YYYY-MM-DD HH:MM:SS temp fps` |
| อ่านอุณหภูมิ | 2 s | |
| กราฟอุณหภูมิ | 10 s | เก็บ 120 จุด = 20 นาที |
| LVGL | ทุกรอบ | refresh 33 ms → เพดาน 30 fps |

---

## 3. FSM การแจ้งเตือนบนจอ: ปกติ / ตีบอกเวลา / ปลุก

```mermaid
stateDiagram-v2
    [*] --> NORMAL
    NORMAL --> CHIME : RTC Alarm B (ต้นชั่วโมง) · หรือคำสั่ง UART "C"
    CHIME --> NORMAL : ครบ 10 s · หรือแตะจอในหน้าเรดาร์
    NORMAL --> ALARM : RTC Alarm A
    CHIME --> CHIME_UNDER_ALARM : RTC Alarm A
    CHIME_UNDER_ALARM --> ALARM : ตีบอกเวลาครบ 10 s
    CHIME_UNDER_ALARM --> CHIME : แตะจอ (ปิดหน้าจอปลุก)
    ALARM --> NORMAL : แตะจอ หรือครบ 60 s
    ALARM --> ALARM : Alarm B มาถึง · (ไม่ตีบอกเวลา เพราะหน้าจอปลุกสำคัญกว่า)

    state CHIME {
        c1: ขอบจอกระพริบติด/ดับทุก 250 ms (lv_timer)
        c2: ข้อความ ตีบอกเวลา HH.00 น. นาน 10 s
        c3: LED3 กระพริบ
    }
    state ALARM {
        a1: หน้าจอสีแดง "ถึงเวลาแล้ว!" (lv_layer_top)
        a2: วงแหวนเต้นขยาย 1.2 s วนซ้ำ
        a3: LED3 กระพริบ 2 Hz
    }
```

- **CHIME:** วงแหวนขอบจออยู่บน `lv_layer_top` และไม่รับการแตะ การแตะจึงไปถึงหน้าที่อยู่ข้างล่าง ใน**หน้าเรดาร์** การแตะครั้งแรก**ใช้หยุดการกระพริบอย่างเดียว** ไม่เปลี่ยนสี ในหน้าอื่นการแตะไม่หยุดการกระพริบ ต้องรอให้ครบ 10 วินาที
- **ALARM:** หน้าจอแจ้งเตือนรับการแตะทั้งจอ แตะที่ไหนก็ปิดได้
- `ui_chime()` จะไม่ทำงานถ้า `alert_on` เป็นจริงอยู่แล้ว แต่ถ้ากำลังตีบอกเวลาอยู่แล้วนาฬิกาปลุกดัง การตีบอกเวลาจะไม่หยุด แต่กระพริบต่อจนครบ 10 วินาทีอยู่ใต้หน้าจอปลุก (สถานะ `CHIME_UNDER_ALARM`)
- ทั้ง CHIME และ ALARM ทำให้ `ui_busy()` เป็นจริง บอร์ดจะไม่เข้าโหมดหลับอัตโนมัติระหว่างนั้น

---

## 4. FSM สีธีม: สีประจำวัน / ดูสีของวันอื่น

```mermaid
stateDiagram-v2
    [*] --> FOLLOW_DAY : ui_tick ครั้งแรก · (ใช้สีของวันนี้ทันที ไม่ค่อย ๆ เปลี่ยน + toast "วันนี้ ...")
    FOLLOW_DAY --> FOLLOW_DAY : เที่ยงคืน (วันใน RTC เปลี่ยน) · ค่อย ๆ เปลี่ยนเป็นสีวันใหม่ 0.6 s + toast
    FOLLOW_DAY --> PREVIEW : แตะหน้าเรดาร์ · idx = idx + 1, preview_since = now
    PREVIEW --> PREVIEW : แตะอีกครั้ง → สีของวันถัดไป · เริ่มนับ 15 s ใหม่
    PREVIEW --> FOLLOW_DAY : ไม่แตะ 15 s · กลับเป็นสีของวันนี้ + toast "กลับเป็น ..."
    PREVIEW --> FOLLOW_DAY : วันใน RTC เปลี่ยนระหว่างดูตัวอย่าง
```

- การเปลี่ยนสีทุกครั้งค่อย ๆ ไล่สีด้วย `lv_anim` ใน 0.6 วินาที ทั้งสีหลัก (accent) และสีตัดกัน (contrast) ของจุดบอกเวลา
- ในแต่ละขั้นของ animation จะเรียก callback ที่ลงทะเบียนไว้ด้วย `theme_register()` ทุกตัว ได้แก่ เรดาร์, บันทึกอุณหภูมิ, จับเวลา, นาฬิกาปลุก และ ui_main
- ค่าที่ `theme_follow_day()` คืน: `0` ไม่มีอะไรเปลี่ยน, `1` วันเปลี่ยน, `2` หมดเวลาดูตัวอย่าง

---

## 5. FSM หน้าจอ (ปัดซ้าย/ขวา)

```mermaid
stateDiagram-v2
    direction LR
    [*] --> RADAR
    RADAR --> THERMAL : ปัดซ้าย
    THERMAL --> RADAR : ปัดขวา
    THERMAL --> CHRONO : ปัดซ้าย
    CHRONO --> THERMAL : ปัดขวา
    CHRONO --> ALARM_PAGE : ปัดซ้าย
    ALARM_PAGE --> CHRONO : ปัดขวา
```

- `lv_tileview` 1 แถว 4 หน้า หน้าแรกปัดได้แค่ไปทางขวา หน้าสุดท้ายปัดได้แค่ไปทางซ้าย
- จุดบอกหน้าปัจจุบันด้านล่างอัปเดตจาก `LV_EVENT_VALUE_CHANGED`
- ทุกหน้ายังทำงานอยู่เบื้องหลัง เช่น นาฬิกาจับเวลาเดินต่อแม้ไม่ได้อยู่ในหน้านั้น

---

## 6. FSM นาฬิกาจับเวลา

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> RUNNING : เริ่ม · ch_start = now, ch_run = 1
    RUNNING --> PAUSED : หยุด · ch_acc += now - ch_start, ch_run = 0
    PAUSED --> RUNNING : เริ่ม · ch_start = now
    PAUSED --> IDLE : รีเซ็ต · ch_acc = 0
    RUNNING --> RUNNING : รีเซ็ต · ch_acc = 0, ch_start = now (เดินต่อจาก 0)
    IDLE --> IDLE : รีเซ็ต
```

- เวลาที่แสดง = `ch_acc + (ch_run ? now - ch_start : 0)` นับจาก `HAL_GetTick()` จึงละเอียด 1 ms และแสดง 0.01 วินาที
- **RUNNING** ทำให้ `ui_busy()` เป็นจริง บอร์ดจะไม่หลับเองระหว่างจับเวลา

---

## 7. FSM เป้าหมายบนเรดาร์ (ping)

ลำแสงกวาด (วินาที) ถือว่า "ผ่าน" เป้าหมาย เมื่อมุมของเป้าหมายอยู่ระหว่างมุมของลำแสงในเฟรมก่อนกับเฟรมนี้ (`swept()` รองรับกรณีมุมวนผ่าน 360°)

### 7.1 จุด ชม. และ นาที
```mermaid
stateDiagram-v2
    direction LR
    [*] --> SHOWN
    SHOWN --> PING_HOUR : ลำแสงผ่านจุด ชม.
    PING_HOUR --> SHOWN : จุดสว่างวาบเป็นสีขาว 0.9 s · + คลื่น 2 วง (110 px, 1.6 s, วงที่ 2 ตามหลัง 0.38 s)
    SHOWN --> PING_MIN : ลำแสงผ่านจุดนาที
    PING_MIN --> SHOWN : จุดเด้ง +10 px (0.14 s ขยาย / 0.26 s หด) · + คลื่นวงเดียว (40 px, 0.65 s)
    SHOWN --> SHOWN : ทุก 5 s ขยับจุดนาที 0.5° · นาทีเปลี่ยน → ย้ายจุด ชม.
```

### 7.2 เป้าหมายสุ่ม 5 จุด (ประกอบฉาก)
```mermaid
stateDiagram-v2
    direction LR
    [*] --> HIDDEN : opacity 0, ตำแหน่งสุ่ม
    HIDDEN --> FADING : ลำแสงผ่าน → opacity 255 แล้วค่อย ๆ จาง 3.8 s · + คลื่น 60 px
    FADING --> FADING : ลำแสงผ่านอีกครั้งก่อนจางหมด → สว่างใหม่
    FADING --> RELOCATE : จางจนหมด (opacity = 0)
    RELOCATE --> HIDDEN : สุ่มตำแหน่งใหม่ ห่างจากลำแสง 60–240° · รัศมี 40–190 px
```

### 7.3 ภาพเปิดเครื่อง (intro)
`INTRO` (intro_k 0 → 1000 ใน 1.4 s: วงแหวนขยาย, ข้อความค่อย ๆ ปรากฏ, ยังไม่วาดลำแสงและยังไม่ ping) → `ACTIVE`
เล่นตอนบูต และทุกครั้งที่ตื่นจาก Stop 2 (`ui_redraw_all`)

---

## 8. FSM ทัชสกรีน (อ่านแบบ polling ใน `touch_read_cb`)

```mermaid
stateDiagram-v2
    direction LR
    [*] --> RELEASED
    RELEASED --> PRESSED : TouchDetected · (พิกัดกลับเป็น 479−x, 479−y เพราะหมุนจอ 180°)
    PRESSED --> RELEASED : ไม่มีการแตะ
    RELEASED --> BLOCKED : ตื่นจาก Stop 2 · (touch_block = 1)
    BLOCKED --> BLOCKED : นิ้วที่ใช้ปลุกยังแตะอยู่ · (ส่ง RELEASED ให้ LVGL)
    BLOCKED --> RELEASED : ยกนิ้วออก
```

สถานะ **BLOCKED** ป้องกันไม่ให้การแตะที่ใช้ปลุกบอร์ดถูกนับเป็นคลิก ซึ่งจะทำให้สีธีมเปลี่ยนโดยไม่ตั้งใจ

---

## 9. FSM ตัวรับคำสั่ง UART

```mermaid
stateDiagram-v2
    direction LR
    [*] --> COLLECT
    COLLECT --> COLLECT : ได้ตัวอักษร (RX interrupt) · rx_line[rx_len++] (สูงสุด 39 ตัว)
    COLLECT --> READY : ได้ '\r' หรือ ' · ' และ rx_len > 0 · คัดลอกไป cmd_line, cmd_ready = 1
    COLLECT --> COLLECT : UART error · ทิ้งบรรทัดนี้ (rx_len = 0)
    READY --> COLLECT : Uart_Poll_Command() ใน main loop
```

| คำสั่ง | ผลลัพธ์ |
|---|---|
| `T YYYY-MM-DD HH:MM:SS` | ตั้ง RTC → `OK time set: ...` |
| `C` | ทดสอบตีบอกเวลาทันที → `OK chime test` |
| อื่น ๆ | `ERR bad command: ...` |

ระหว่าง `cmd_ready = 1` ถ้ามีบรรทัดใหม่เข้ามา จะถูกทิ้ง (ไม่เขียนทับคำสั่งที่ยังไม่ได้ประมวลผล)

---

## 10. การตัดสินใจตั้งเวลา RTC ตอนบูต

```mermaid
flowchart TD
    S(["rtc_clock_init"]) --> C1["เปิด LSE<br/>ถ้าไม่สำเร็จใน 2 s → ใช้ LSI"]
    C1 --> C2["HAL_RTC_Init + WaitForSynchro RSF"]
    C2 --> Q1{"DR0 = magic?<br/>เคยตั้งเวลาแล้ว"}
    Q1 -- ไม่ --> SET["ตั้งเวลาจาก __DATE__ __TIME__"]
    Q1 -- ใช่ --> Q2{"RTC ช้ากว่าเวลา build<br/>เกิน 10 นาที?"}
    Q2 -- ใช่ --> SET
    Q2 -- ไม่ --> KEEP["ใช้เวลาเดิมของ RTC"]
    SET --> R["คืนค่า Alarm A จาก DR3<br/>และ Alarm B (ตีบอกเวลา) จาก DR4"]
    KEEP --> R
    R --> E(["RTC พร้อม"])
```

| Backup register | ใช้เก็บ |
|---|---|
| DR0 | magic: RTC เคยตั้งเวลาแล้ว |
| DR1 | hash ของเวลาที่ build (ไว้อ้างอิง) |
| DR3 | นาฬิกาปลุก: tag, เปิด/ปิด, ชั่วโมง, นาที |
| DR4 | ตีบอกเวลา: tag, เปิด/ปิด |

---

## สรุปแหล่งเหตุการณ์ (event sources)

| เหตุการณ์ | ที่มา | FSM ที่ได้รับผล |
|---|---|---|
| แตะจอ / ปัดจอ | Sitronix touch (I2C5, polling) | 3, 4, 5, 6, 8 |
| แตะจอขณะหลับ | ขา INT ของทัช → EXTI8 | 1 |
| ปุ่ม USER | PC13 → EXTI13 | 1 |
| ถึงเวลาปลุก | RTC Alarm A → `RTC_IRQHandler` | 1, 3 |
| ต้นชั่วโมง | RTC Alarm B → `RTC_IRQHandler` | 1, 3 |
| วันใน RTC เปลี่ยน | `ui_tick` อ่าน RTC | 4 |
| ตัวอักษรเข้า UART | USART1 RX interrupt | 9 |
| เวลาผ่านไป | `HAL_GetTick`, `lv_timer`, `lv_anim` | 2, 3, 4, 6, 7 |
