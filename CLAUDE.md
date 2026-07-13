# Software_uC — KNX Smart Home Controller

## Project overview

PlatformIO / Arduino project running on a **Seeed XIAO ESP32-C6**. This is the firmware for a KNX wall controller: it reads capacitive touch pads, drives KNX bus telegrams for light switching and dimming, shows status on an OLED display, and feeds back incoming KNX events to an RGB backlight.

The codebase also serves as the prototype for a future **cross-platform Arduino KNX library** (see migration notes below).

## Build system

PlatformIO, Arduino framework. Build and upload via PlatformIO CLI or the PlatformIO IDE extension.

```
pio run              # build
pio run --target upload
pio device monitor   # 115200 baud
```

All custom code lives under `lib/`. Third-party dependencies are declared in `platformio.ini` and fetched automatically.

## Architecture

Strict layered design — dependencies only flow downward:

```
src/main.cpp              ← application: wires everything together
lib/KNX/                  ← high-level KNX coordinator (bind, send, handleUART)
lib/KNX_Telegram/         ← telegram encode / decode (DPT1–DPT19)
lib/KNX_TPUART2/          ← hardware driver: UART ↔ Siemens TP-UART2 transceiver
lib/KNX_Defines/          ← shared types, enums, structs (header-only)
lib/KNX_Device/           ← button abstractions (SingleButton, TwoButtonDimming, …)
lib/TouchSensor/          ← MPR121 capacitive touch (4-pad, I2C 0x5A)
lib/Backlight/            ← WS2812B NeoPixel strip (pin D3, 2 pixels)
lib/Display/              ← SSD1306 OLED 128×64 (I2C 0x3D)
lib/TemperatureSensor/    ← SHTC3 temperature + humidity (I2C)
```

No global singletons. Dependencies are injected via `Init()` or constructor pointers.

## Hardware pin map (XIAO ESP32-C6)

| Signal | Pin | Direction |
|---|---|---|
| KNX UART RX | D6 | IN |
| KNX UART TX | D7 | OUT |
| TPUART /RESET | D0 | OUT (open-drain) |
| TPUART SAVE | D1 | IN (FALLING interrupt → voltage failure) |
| TPUART TW | D2 | IN (RISING interrupt → temperature warning) |
| NeoPixel data | D3 | OUT |
| I2C SDA | SDA | Shared: MPR121, SSD1306, SHTC3 |
| I2C SCL | SCL | Shared bus |

## KNX group addresses in use

| Address | DPT | Direction | Purpose |
|---|---|---|---|
| 0/0/1 | DPT3 | OUT | Dimming (incremental) |
| 0/1/1 | DPT1 | OUT | Light switching |
| 0/3/0 | DPT1 | IN  | Light status feedback → backlight |

Physical address of this device: **1.1.5**

## Current wiring state

**Active in main loop:**
- `touch.update()` → `rocker1_TwoButtonDimming.update(Top_Left, Top_Right)` → KNX send
- `knx.handleUART()` → parsed telegram → `onStatus_LightA()` → backlight color

**Implemented but not yet wired into main.cpp:**
- `TemperatureSensor` — hardware ready, never instantiated
- `Display` — fully implemented (temp, humidity), never instantiated
- `TouchSensor` Bottom_Left / Bottom_Right pads — read but unused

## KNX_Device class hierarchy

```
SingleButtonOperation (abstract)
└── TwoButtonOperation (abstract)
    ├── TwoButtonDimming   ← used in main.cpp (rocker1)
    └── TwoButtonSwitching
SingleButtonSwitching
SingleTestButton / TwoTestButton  ← serial debug helpers
```

Short press → switch (DPT1). Long press / hold → dim (DPT3). Hold release → stop.

## Debug flags

Define per-module to enable `Serial.print` output:

```cpp
#define DEBUG           // KNX_TPUART2 verbose logging
```

No debug output is active in production builds.

## Planned migration: TP-UART2 → STKNX

The Siemens TP-UART2 transceiver will be replaced by the ST STKNX, which is a **pure physical-layer** chip (GPIO, not UART). This is part of the transition to a cross-platform Arduino KNX library.

Key consequences:
- `KNX_TPUART2` will be replaced by a new `KNX_STKNX` driver.
- Interface changes from `HardwareSerial` to GPIO bit-bang at 9600 baud (104 µs/bit).
- All layers above `KNX_TPUART2` (`KNX_Telegram`, `KNX`, `main.cpp`) survive unchanged.
- Bit timing driven by **hardware timer interrupts** (not `delayMicroseconds`) to allow collision detection during TX and to remain reliable when WiFi/MQTT/Matter stacks are active.
- Timer HAL will use `#ifdef` guards per platform: AVR (Timer1 CTC), ESP32 (`timerBegin`), RP2040 (`add_repeating_timer_us`), SAMD (TC3).
- Fixed tick period: **13 µs** (8 ticks per KNX bit). Dominant '0' = 3 ticks HIGH + 5 ticks LOW. Recessive '1' = 8 ticks LOW.
