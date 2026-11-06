# STMicroelectronics STM32U5A9J-DK: นาฬิกาเรดาร์ Sci-Fi บนจอกลม (LVGL)

## ข้อมูลบอร์ด
| | |
|---|---|
| **ยี่ห้อ (ผู้ผลิต)** | STMicroelectronics (ST) |
| **รุ่นบอร์ด** | **STM32U5A9J-DK** (Discovery kit) |
| **MCU** | STM32U5A9NJH6Q: Arm Cortex-M33 + TrustZone, 160 MHz, Flash 4 MB, SRAM 2.5 MB, NeoChrom GPU2D |
| **จอ** | จอกลม TFT 2.47" 480×480 ต่อผ่าน MIPI-DSI พร้อมทัชสกรีน capacitive (Sitronix) |
| **เซนเซอร์บนบอร์ด** | อุณหภูมิ STTS22H, ToF/ท่าทาง VL53L5CX |
| **ดีบักเกอร์** | STLINK-V3E บนบอร์ด (USB Micro-B, Virtual COM, ไดรฟ์แฟลช `DIS_U5A9NJ`) |
| **หน้าข้อมูลบอร์ด** | <https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html> |
| **สถานะ** | ST ประกาศ **OBSOLETE** แล้ว รุ่นที่แนะนำให้ใช้แทนคือ [STM32U5G9J-DK1](https://www.st.com/en/evaluation-tools/stm32u5g9j-dk1.html) |

> ชื่อ repository ใส่ยี่ห้อและรุ่นบอร์ด (`STMicroelectronics-STM32U5A9J-DK`) เพื่อแยกออกจากโปรเจกต์ของบอร์ดรุ่นอื่น
>
> ประวัติการแก้ไขและเพิ่มเติมทั้งหมดดูได้ที่ [CHANGELOG.md](CHANGELOG.md)
>
> สไลด์อธิบายโปรเจกต์ (HTML, 15 หน้า): [docs/slides.html](docs/slides.html) ดาวน์โหลดแล้วเปิดในเบราว์เซอร์ ใช้ปุ่ม ← → เปลี่ยนหน้า กด F เพื่อเต็มจอ
>
> Finite State Machine ของโปรแกรม (10 FSM พร้อมแผนภาพ Mermaid): [docs/state_machine.md](docs/state_machine.md)

โปรเจกต์ STM32CubeIDE ดัดแปลงจากตัวอย่าง `GPIO_IOToggle` ของ [STM32CubeU5](https://github.com/STMicroelectronics/STM32CubeU5)
ตอนนี้โปรแกรมเป็น**นาฬิกาเรดาร์ Sci-Fi ภาษาไทยบนจอกลม 480×480** เขียนด้วย **LVGL v9.6.0** และใช้ NeoChrom GPU

## ฟีเจอร์ (ปัดซ้าย/ขวาเพื่อเปลี่ยนหน้า)
| หน้า | รายละเอียด |
|---|---|
| **เรดาร์ (นาฬิกา)** | ตัวเลขนาฬิกาดิจิทัลขนาด 130 px อยู่กลางจอ, ลำแสงกวาดที่มีหางจางลงทำหน้าที่เป็นเข็มวินาที, จุดบอก **ชม.** (ใหญ่) และ **นาที** (เล็ก ค่อย ๆ ขยับทุก 5 วินาที) ใช้สีตัดกับสีประจำวัน, เมื่อลำแสงกวาดผ่าน จุด ชม. จะสว่างวาบพร้อมคลื่น 2 วง ส่วนจุดนาทีจะเด้งพร้อมคลื่นวงเล็ก, แสดงวัน วันที่ (พ.ศ.) และอุณหภูมิเป็นภาษาไทย, ตอนเปิดเครื่องมีภาพเคลื่อนไหววงเรดาร์ขยายออก |
| **บันทึกอุณหภูมิ** | กราฟย้อนหลัง 20 นาที (เก็บค่าทุก 10 วินาที) พร้อมค่าต่ำสุด/สูงสุด |
| **จับเวลา** | ตัวเลข 130 px ละเอียด 0.01 วินาที มีปุ่ม เริ่ม/หยุด และ รีเซ็ต |
| **นาฬิกาปลุก** | ตั้งเวลาด้วย roller, สวิตช์ **ปลุก** (RTC Alarm A ทุกวัน) และสวิตช์ **ตีบอกเวลา** |

- **สีประจำวันแบบไทย:** อาทิตย์ แดง, จันทร์ เหลือง, อังคาร ชมพู, พุธ เขียว, พฤหัสบดี ส้ม, ศุกร์ ฟ้า, เสาร์ ม่วง
  - เปลี่ยนสีเองตอนเที่ยงคืน
  - แตะจอเพื่อดูสีของวันอื่น ถ้าไม่แตะต่อ 15 วินาทีจะกลับเป็นสีของวันนี้
- **ตีบอกเวลาทุกต้นชั่วโมง:** ใช้ RTC Alarm B ขอบจอจะกระพริบ 10 วินาที พร้อม LED3 และข้อความ "ตีบอกเวลา HH:00 น." แตะจอเพื่อหยุดก่อนได้ (บอร์ดไม่มีลำโพง จึงเตือนด้วยภาพอย่างเดียว)
- **หน้าจอแจ้งเตือนปลุก:** "ถึงเวลาแล้ว!" พร้อมวงแหวนเต้นและ LED3 กระพริบ แตะเพื่อปิด ถ้าไม่แตะจะปิดเองใน 60 วินาที
- **เปิดจอตลอด** (`ALWAYS_ON = 1` ใน `main.c`)
  - กดปุ่ม **USER** เพื่อปิดจอและเข้าโหมด **Stop 2**
  - ปลุกได้ด้วยการแตะจอ, กดปุ่ม USER หรือเมื่อถึงเวลาปลุก/ตีบอกเวลา
- **หมุนภาพ 180°** ให้ช่อง USB อยู่ด้านบน (`DISPLAY_ROTATE_180` ใน `lvgl_port.c`)
- **RTC:** ใช้คริสตัล LSE 32.768 kHz และจะตั้งเวลาจากเวลาที่ build ก็ต่อเมื่อ RTC ไม่มีเวลาที่ใช้ได้ (ช้าเกิน 10 นาที)
- **UART 115200:** พิมพ์วันเวลา อุณหภูมิ และ fps ทุกวินาที รับคำสั่ง:
  - `T YYYY-MM-DD HH:MM:SS` ตั้งเวลา
  - `C` ทดสอบตีบอกเวลา

### Renderer และความเร็ว (หน้าเรดาร์)
| ไฟล์ | renderer | fps |
|---|---|---|
| `build/GPIO_IOToggle_gpu.bin` (เป็นค่าเริ่มต้น = `GPIO_IOToggle.bin`) | **NeoChrom GPU2D** ผ่าน NemaGFX | ~26–30 |
| `build/GPIO_IOToggle_sw.bin` | software (CPU) | ~8 |

## ภาษาไทยและฟอนต์ Prompt
- UI ทั้งหมดเป็นภาษาไทย ใช้ฟอนต์ **Prompt** (Cadson Demak, SIL Open Font License 1.1) จากไฟล์ใน `~/Library/Fonts`
- ฟอนต์ถูกแปลงเป็นโค้ด C ไว้ใน `GPIO_IOToggle/Src/fonts/` ด้วย `tools/ttf2lvgl.py` ซึ่งเขียนเองโดยใช้ Pillow:
  - `font_prompt_clock` 130 px: ตัวเลขนาฬิกาขนาดใหญ่ ตัวเลขทุกตัวกว้างเท่ากัน (tabular)
  - `font_prompt_56` / `28` / `20` / `16`: ตัวอักษร ASCII, ไทย และสัญลักษณ์ °
- **วรรณยุกต์ไทยแบบซ้อน:** LVGL ไม่มีระบบจัดตำแหน่งสระและวรรณยุกต์ (shaping) จึงแก้ 2 ส่วน
  - `ttf2lvgl.py` สร้างวรรณยุกต์ชุดยกสูงไว้ที่รหัส U+F700–F704
  - `thai_text.c` (`thai_label_set_text` / `thai_label_set_fmt`) เปลี่ยนไปใช้ตัวยกสูงเมื่อวรรณยุกต์ตามหลังสระบน หรืออยู่หน้าสระอำ เช่น ที่, เริ่ม, เพื่อ, ต่ำ, น้ำ
  - ข้อความไทยทุกข้อความต้องตั้งผ่าน 2 ฟังก์ชันนี้
- วันที่แสดงเป็นภาษาไทย ปีเป็น **พ.ศ.** เช่น "วันพฤหัสบดี / 5 พฤศจิกายน 2569"

สร้างฟอนต์ใหม่ (ตัวอย่าง):
```bash
python3 tools/ttf2lvgl.py ~/Library/Fonts/Prompt-Medium.ttf 28 font_prompt_28 GPIO_IOToggle/Src/fonts/font_prompt_28.c 0x20-0x7E,0xB0,0xB7,0x0E01-0x0E3A,0x0E3F-0x0E5B --tabular-digits
```

## โครงสร้างโฟลเดอร์
```
ST-Micro-Board/
├── Drivers/                       # ไลบรารีจาก ST (ห้ามแก้ไข)
│   ├── CMSIS/                     # CMSIS Core + cmsis_device_u5 v1.4.3
│   ├── STM32U5xx_HAL_Driver/      # HAL/LL driver
│   └── BSP/
│       ├── STM32U5x9J-DK/         # BSP ของบอร์ด v1.3.1 (LED, COM, LCD, TS, ENV)
│       └── Components/            # Common, sitronix (ทัชสกรีน), stts22h (อุณหภูมิ)
├── Middlewares/lvgl/              # LVGL v9.6.0 (ไม่มีไฟล์ .cpp, docs, demos) + libs/nema_gfx (M33 revC)
├── Utilities/lcd, Utilities/Fonts # UTIL_LCD ของ ST (ไม่ได้ใช้แล้ว เก็บไว้อ้างอิง)
├── tools/
│   ├── build.sh                   # build ด้วย Arm GCC จาก command line (ไม่ต้องเปิด CubeIDE)
│   ├── flash.py                   # แฟลชผ่านไดรฟ์ ST-LINK + เก็บ log ตอนบูต + ซิงก์เวลา
│   ├── ttf2lvgl.py                # แปลงไฟล์ TTF เป็นฟอนต์ LVGL (รองรับภาษาไทย)
│   ├── serial_read.py             # อ่าน UART 115200 บน macOS
│   └── set_time.py                # ตั้งเวลา RTC ให้ตรงกับ Mac
├── build/                         # ไฟล์ผลลัพธ์จากการ build (.bin/.elf, ไม่เก็บใน git)
└── GPIO_IOToggle/
    ├── Inc/  lv_conf.h, lvgl_port.h, ui_pages.h, ui_theme.h, rtc_clock.h, ...
    ├── Src/  main.c, lvgl_port.c, ui_main.c, ui_radar.c, ui_pages_misc.c, ui_theme.c, rtc_clock.c, thai_text.c, fonts/ ...
    └── STM32CubeIDE/              # .project / .cproject, linker script, startup
```

## Build และแฟลช (บน Mac โดยไม่ใช้ CubeIDE)
ใช้ Arm GNU Toolchain 12.2 (`arm-none-eabi-gcc`) และ Python 3 ที่มี Pillow

build แบบ NeoChrom GPU ด้วย `-O2` (ถ้าไม่ใส่ `NEOCHROM=1` จะได้แบบ software):
```bash
NEOCHROM=1 tools/build.sh /tmp/u5build
```
แปลงไฟล์ .elf เป็น .bin:
```bash
arm-none-eabi-objcopy -O binary /tmp/u5build/GPIO_IOToggle.elf build/GPIO_IOToggle.bin
```
แฟลช + เก็บ log ตอนบูต + ซิงก์เวลา:
```bash
python3 tools/flash.py build/GPIO_IOToggle.bin
```
หรือลากไฟล์ `build/GPIO_IOToggle.bin` ไปวางในไดรฟ์ **DIS_U5A9NJ** เองก็ได้

ตั้งเวลาให้ตรงกับ Mac เพียงอย่างเดียว:
```bash
python3 tools/set_time.py
```

## เปิดใน STM32CubeIDE
1. ไปที่ **File → Import… → Existing Projects into Workspace** แล้วเลือกโฟลเดอร์ `GPIO_IOToggle/STM32CubeIDE`
2. **อย่าติ๊ก** "Copy projects into workspace" เพราะโปรเจกต์อ้างถึงไฟล์ด้วย path แบบสัมพัทธ์
3. ใน CubeIDE โฟลเดอร์ `Middlewares/lvgl` ถูกเชื่อมเข้ามาทั้งโฟลเดอร์ แล้วตั้ง define `LV_CONF_INCLUDE_SIMPLE` และ include path ไว้ให้แล้ว
4. **คอนฟิก Debug ของ CubeIDE ใช้ -O0** LVGL จะเรนเดอร์ช้ากว่าไฟล์ .bin ที่ build ด้วย `-O2` อย่างเห็นได้ชัด ถ้าต้องการความลื่น ให้ใช้คอนฟิก Release หรือตั้ง optimization เป็น `-O2`

## ข้อควรรู้ทางเทคนิค
- **ระบบไฟ:** VDD = **1.8 V** ถ้าจะต่ออุปกรณ์ภายนอกที่ใช้ 3.3 V ต้องใช้ level shifter
- **framebuffer:** BSP ใช้ GFXMMU (เก็บเฉพาะพิกเซลในวงกลม) และ LTDC แบบ ARGB8888
  - framebuffer เสมือนมีความกว้างแถวละ **3072 ไบต์** (768 พิกเซล) ฟังก์ชัน `flush_cb` ของ LVGL คัดลอกข้อมูลทีละแถวลงไปเอง
  - `BSP_LCD_FillRGBRect()` ใน BSP v1.3.1 **มีบั๊ก** คัดลอกแถวแรกของข้อมูลต้นทางซ้ำทุกบรรทัด ห้ามใช้กับภาพที่สูงเกิน 1 แถว
- **ข้อกำหนดของจอ:**
  - ต้องเปิด **HSE** เพราะ PLL3 ของ LTDC/DSI ใช้ HSE
  - ต้องมี IRQ handler ของ GFXMMU, LTDC, LTDC_ER และ DSI ใน `stm32u5xx_it.c`
- **printf:** newlib-nano ไม่รองรับ `%f` อุณหภูมิจึงพิมพ์จากจำนวนเต็มหน่วย 0.1 °C
- **macOS:** ต้องตั้ง baud ใน process เดียวกับที่อ่านพอร์ต (`stty` + `cat` ใช้ไม่ได้) จึงใช้สคริปต์ใน `tools/` แทน
- **CubeIDE:** โปรเจกต์ใน CubeIDE build เป็นแบบ software renderer
  - ถ้าจะใช้ GPU ให้เพิ่ม define `USE_NEOCHROM` แล้วลิงก์ไฟล์ `Middlewares/lvgl/libs/nema_gfx/lib/core/cortex_m33_revC/gcc/libnemagfx-float-abi-hard.a`
- **โหมด Stop 2:** หลังตื่น โปรแกรมเรียก `SystemClock_Config()` และ `BSP_LCD_Init()` ใหม่ (PLL3, DSI และ LTDC) แล้วสั่งวาดจอใหม่ทั้งหมด
  - ระหว่างหลับ UART รับคำสั่งตั้งเวลาไม่ได้
