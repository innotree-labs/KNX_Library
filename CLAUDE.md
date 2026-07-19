# Software_uC — KNX Smart Home Controller

## Project overview

PlatformIO / Arduino project running on a **Seeed XIAO ESP32-C6**. The repository is now
primarily an **Adafruit-style cross-platform Arduino KNX library** (the redesign tracked in
`PLAN.md`), with `src/main.cpp` as a showcase sketch demonstrating its use. The original target
is a KNX wall controller (capacitive touch pads, OLED, RGB backlight); those sensor/display
drivers are not part of the library surface and the thesis button layer now lives under
`examples/`.

## Build system

PlatformIO, Arduino framework. Build and upload via PlatformIO CLI or the PlatformIO IDE extension.

```
pio run              # build
pio run --target upload
pio device monitor   # 115200 baud
```

All custom code lives under `lib/`. Third-party dependencies are declared in `platformio.ini` and fetched automatically.

## Architecture

Strict layered design — dependencies only flow downward, acyclic (PLAN §12):

```
src/main.cpp        ← showcase sketch: wires the stack + drives intent objects
lib/Konnextor/      ← THE public surface, and nothing else: Konnextor.h — the single user
                      include, sole root of the DAG. Defines the Konnextor node class (owns a
                      KnxDriver, built from the physical address). Header-only, Arduino-only.
lib/KnxObject/      ← KnxObject : IKnxReceiver + intent classes (KnxLight, KnxDimmLight,
                      KnxRGB, KnxBlind, KnxTemperature, …) grouped by domain header. Header-only.
lib/KnxCoordinator/ ← DI core class KnxCoordinator (KnxCoordinator.h): send(ga, KnxValue),
                      intrusive IKnxReceiver registry, loop(); holds an injected IKnxDriver*
lib/KnxDriver/      ← concrete ATTiny / TP-UART UART driver : IKnxDriver (target-only)
lib/KnxTelegram/    ← stateless L_Data framing + reassembler (KnxFrame, KnxReassembler);
                      Arduino-free, host-tested
lib/KnxValue/       ← value currency: KnxValue tagged union + symmetric KnxCodec. Pure
lib/KnxCommon/      ← shared types + contracts: KnxEnums, KnxAddress, KnxTelegramTypes,
                      KnxInterfaces (IKnxDriver / IKnxReceiver), KnxDebug (runtime
                      logging switch used by every layer). Header-only
examples/KNX_Device/← thesis button state machines (SingleButton/TwoButtonDimming, …);
                      NOT in the build, NOT part of the library surface (PLAN §12)
```

Dependency flow: `Konnextor → {KnxDriver, KnxObject, KnxCoordinator, KnxValue, KnxCommon}`,
`KnxObject → KnxCoordinator → {KnxTelegram, KnxValue, KnxCommon}`,
`KnxDriver → {KnxTelegram, KnxCommon}`, `KnxTelegram → KnxValue → KnxCommon`.
Interfaces (`IKnxDriver`, `IKnxReceiver`) live in `KnxCommon` below their consumers, so the
coordinator never includes the concrete driver or object headers — no cycle. `Konnextor` is the
only library above the driver, and nothing includes it — which is exactly why it can bundle the
whole stack, and why native tests (which include `KnxCoordinator.h` and the object headers
directly) never drag the Arduino driver into a host build.

No global singletons. Dependencies are injected by constructor pointer/reference.

**User include & construction:** a sketch needs only `#include <Konnextor.h>` and
`Konnextor knx("1.1.5");`. `Konnextor.h` (its own library, sole root of the DAG) pulls in the
driver, the coordinator core, the value currency, and every intent class, then defines the
user-facing **`Konnextor` node class** — a thin
Arduino subclass of `KnxCoordinator` that *owns* a `KnxDriver` and is built from the physical
address, so the user never instantiates or injects a driver (address typed once). The
dependency-injection **core is `KnxCoordinator`** (`KnxCoordinator.h`): Arduino-free, host-testable
with a mock driver, and the type intent objects reference (`KnxCoordinator&`). Advanced users can
inject their own `IKnxDriver` by constructing a `KnxCoordinator` directly.

**Testing:** `pio test -e native` runs the host Unity suite (codec, framing, reassembler,
coordinator, objects) against the Arduino-free layers; `pio run` builds the firmware.

## Hardware pin map (XIAO ESP32-C6)

| Signal | Pin | Direction |
|---|---|---|
| KNX UART RX | D6 | IN |
| KNX UART TX | D7 | OUT |
| ATTiny /RESET | D0 | OUT (open-drain) |
| NeoPixel data | D3 | OUT |
| I2C SDA | SDA | Shared: MPR121, SSD1306, SHTC3 |
| I2C SCL | SCL | Shared bus |

## KNX group addresses in the bench test (`src/main.cpp`)

| Address | DPT | Direction | Purpose |
|---|---|---|---|
| 1/1/1 | DPT1 | OUT | `lamp` switching — the GA being toggled |
| 0/1/1 | DPT1 | IN  | `lamp` switching status → `onLampChanged` callback |
| 0/2/1 | DPT5 | IN  | `brightness` status from the dimmer → `onBrightnessChanged` callback |

Note 0/2/1 is a DPT5 **brightness status**, not a DPT3 relative-dim command GA — so the sketch
uses `KnxLight` + a listen-only `KnxPercent`, not `KnxDimmLight` (whose third GA is a *send*
address for dim steps). No relative-dim command GA is configured on this device.

Group addresses are hardcoded in the sketch (no ETS) — the Adafruit-style trade-off (PLAN §1).
Physical address of this device: **1.1.5**

## Bench-test sketch (`src/main.cpp`)

`src/main.cpp` is currently a **hardware bench test**, not the API showcase: no buttons, one
`KnxLight` toggled every 5 s on a `millis()` cadence, plus a listen-only `KnxPercent` for the
dimmer's brightness status, with every status telegram printed over Serial at 115200. Its job is
to exercise the two paths host tests cannot reach — the driver's real transmit path (positive
`L_Data.con`?) and the full receive path (reassemble → parse → match → decode → callback), the
latter across two objects and two DPTs, which also proves registry dispatch selects the right
receiver. `knx.begin()`'s return value is printed at boot, and the file ends with a
symptom→cause guide for reading a bad run.

The prior three-tier API showcase (intent objects / value objects / stateless send, with
`kitchen`, `lamp` and `roomTemp`) is in git history at `e22c388` — restore it from there when
the bench work is done rather than rewriting it.

`delay()` must not be used for the cadence: it stalls `knx.loop()`, so status callbacks arrive
late or get dropped. Objects are declared at global scope — they self-register into the
coordinator's receiver registry on construction and must outlive it (PLAN §6).

**Callback style in the showcase:** handlers are **named free functions**, prototyped above
`setup()` and defined below `loop()` (the thesis layout), so `setup()` stays a flat wiring
manifest — one line per binding, no handler bodies inline. `onUpdate` takes a plain
`void(*)(native)`, so a non-capturing lambda works identically; the named form is the idiom the
example teaches because it matches `attachInterrupt(pin, handler, …)` and names the intent. The
prototypes are only needed because this is a `.cpp` — a user's `.ino` gets them generated.

## Debug mode

Verbose tracing is a **runtime** switch — no recompile, no `#define`:

```cpp
knx.enableDebugMode(true);   // call before begin() to trace driver bring-up too
```

It prints, all prefixed `[knx]`: driver bring-up and UART traffic, TX frame hexdumps, the raw
`L_Data.con` byte behind each verdict, RX frame hexdumps, frame build/parse (including checksum
mismatches), codec decode failures, per-object decode/cache updates, and **every telegram
dispatched — including telegrams addressed to other devices** (logged as "0 receiver(s)", which
is normal foreign traffic, not an error). Address-validation warnings ride the same switch.

Implementation is `KnxCommon/src/KnxDebug.h`: a single library-wide `bool` in an inline
function-local static, plus `log()` / `logBytes()`. Both the `#ifdef ARDUINO` guard and the
runtime check live **inside** those functions, so call sites elsewhere carry no preprocessor
noise and the Arduino-free layers stay Arduino-free — on a host build the bodies compile away
and the native suite is unaffected.

Three properties to know:
- **Library-wide, not per-instance.** The flag is shared; enabling it on any node enables it
  everywhere. This is the agreed, documented exception to the no-globals rule — it is a log
  level, not a service locator, and it is what lets the stateless `KnxFrame`/`KnxCodec`
  namespaces log without an object to hang a flag on.
- **Costs ~2.4 KB flash, 0 RAM** when compiled in and disabled (measured on ESP32-C6); a
  disabled call is one bool test. There is deliberately no compile-time master switch yet — add
  one only if an AVR target ever needs the code stripped entirely.
- **Arguments are still evaluated when disabled.** Guard an expensive call site with
  `if (KnxDebug::isEnabled())`.

Logging is chatty and the printing itself costs time on the receive path, so it can perturb
what it is diagnosing. Establish a clean run first, then switch it on to explain a broken one.

## Physical layer: STKNX behind an ATTiny co-processor

The Siemens TP-UART2 transceiver is replaced by the ST STKNX, a **pure
physical-layer** chip (GPIO, not UART). Rather than bit-bang the STKNX on the main
MCU, an **external ATTiny co-processor** handles all bit-level timing and presents
a **TP-UART-like UART interface** to the main MCU.

Key consequences:
- `KNX_TPUART2` is replaced by a new driver, but the driver still talks **UART**
  to the ATTiny — not GPIO bit-bang on the main MCU.
- **Bit timing, collision detection, and STKNX GPIO drive live on the ATTiny.**
  The main MCU never runs timing-critical ISRs for KNX, so the library stays
  reliable when WiFi/MQTT/Matter stacks share the main core.
- All layers above the physical driver (`KnxTelegram`, `KnxCoordinator`, application) keep a
  UART-shaped link-layer interface (send frame / poll for events / TX
  confirmation), so the swap is contained to the driver.
- The ATTiny is expected to return a **transmit confirmation** (TP-UART-style
  `L_Data.con`); the driver must surface real send success/failure instead of
  hard-returning `true` (see `PLAN.md` §9).

> The broader library redesign (Adafruit-style API, `Dpt`/`KnxValue` value types,
> `KnxObject` + intent classes, unified RX registry) is tracked in **`PLAN.md`**.

