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
lib/KNX_Object/     ← KnxObject : IKnxReceiver + intent classes (KnxLight, KnxDimmLight,
                      KnxRGB, KnxBlind, KnxTemperature, …) grouped by domain header;
                      KNX.h is the public one-line facade (top of the DAG). Header-only.
lib/KNX/            ← DI core class KnxCoordinator (KnxCoordinator.h): send(ga, KnxValue),
                      intrusive IKnxReceiver registry, loop(); holds an injected IKnxDriver*
                      (the user-facing KNX node subclass that owns the driver is in KNX.h)
lib/KNX_Driver/     ← concrete ATTiny / TP-UART UART driver : IKnxDriver (target-only)
lib/KNX_Telegram/   ← stateless L_Data framing + reassembler (KnxFrame, KnxReassembler);
                      Arduino-free, host-tested
lib/KNX_Value/      ← value currency: KnxValue tagged union + symmetric KnxCodec. Pure
lib/KNX_Common/     ← shared types + contracts: KnxEnums, KnxAddress, KnxTelegramTypes,
                      KnxInterfaces (IKnxDriver / IKnxReceiver). Header-only
examples/KNX_Device/← thesis button state machines (SingleButton/TwoButtonDimming, …);
                      NOT in the build, NOT part of the library surface (PLAN §12)
```

Dependency flow: `KNX_Object → KNX → {KNX_Telegram, KNX_Value, KNX_Common}`,
`KNX_Driver → {KNX_Telegram, KNX_Common}`, `KNX_Telegram → KNX_Value → KNX_Common`.
Interfaces (`IKnxDriver`, `IKnxReceiver`) live in `KNX_Common` below their consumers, so the
coordinator never includes the concrete driver or object headers — no cycle.

No global singletons. Dependencies are injected by constructor pointer/reference.

**User include & construction:** a sketch needs only `#include <KNX.h>` and `KNX knx("1.1.5");`.
`KNX.h` (in `KNX_Object`, atop the DAG) pulls in the driver, the coordinator core, the value
currency, and every intent class, then defines the user-facing **`KNX` node class** — a thin
Arduino subclass of `KnxCoordinator` that *owns* a `KNX_Driver` and is built from the physical
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

## KNX group addresses in the showcase (`src/main.cpp`)

| Address | DPT | Direction | Purpose |
|---|---|---|---|
| 0/1/1 | DPT1 | OUT | `kitchen` light switching |
| 0/3/0 | DPT1 | IN  | `kitchen` status feedback → onUpdate callback |
| 0/1/2 | DPT1 | OUT | `lamp` switching |
| 0/0/1 | DPT3 | OUT | `lamp` incremental dimming |
| 0/4/2 | DPT9 | IN/OUT | `roomTemp` temperature |

Group addresses are hardcoded in the sketch (no ETS) — the Adafruit-style trade-off (PLAN §1).
Physical address of this device: **1.1.5**

## Showcase sketch (`src/main.cpp`)

Demonstrates the three usage tiers (PLAN §3):
- **Intent objects** — `KnxLight kitchen(knx, "0/1/1", "0/3/0")`; `.on()/.off()/.toggle()`
  flips relative to the cached status-fed state; `.onUpdate([](bool){…})` for feedback.
- **Value objects** — `KnxTemperature roomTemp(knx, "0/4/2")`; `.set(float)` / `.onUpdate`.
- **Stateless send** — `knx.send("0/4/2", Dpt9(21.5f))` for a one-off to any address (no RX).

Objects are declared at global scope: they self-register into the coordinator's receiver
registry on construction and must outlive it (PLAN §6). The Arduino `loop()` calls `knx.loop()`
to service the stack (parse incoming telegrams, fire callbacks) and drives the light from buttons.

## Debug flags

Define per-module to enable `Serial.print` output:

```cpp
#define DEBUG           // KNX_Driver verbose logging
```

No debug output is active in production builds.

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
- All layers above the physical driver (`KNX_Telegram`, `KNX`, application) keep a
  UART-shaped link-layer interface (send frame / poll for events / TX
  confirmation), so the swap is contained to the driver.
- The ATTiny is expected to return a **transmit confirmation** (TP-UART-style
  `L_Data.con`); the driver must surface real send success/failure instead of
  hard-returning `true` (see `PLAN.md` §9).

> The broader library redesign (Adafruit-style API, `Dpt`/`KnxValue` value types,
> `KnxObject` + intent classes, unified RX registry) is tracked in **`PLAN.md`**.

## IN-PROGRESS REFACTOR: unified `Knx` naming + `Konnextor` facade

**Status: PLANNED — not started.** Delete this whole section once step 10 is done and committed.

**Goal.** Adopt ONE naming convention across the library: PascalCase **`Knx…`** for every
library folder, header file, class, and enum *type*. Drop the `KNX_` SCREAMING prefix from all
*identifiers*. The single user-facing facade class is renamed to the product name
**`Konnextor`**, so a sketch reads:

```cpp
#include <Konnextor.h>
Konnextor knx("1.1.5");
```

**Scope boundaries (do NOT over-reach):**
- The protocol word "KNX" stays as-is in **prose / doc-comments** ("the KNX bus", "a KNX
  telegram"). Only the `KNX` *class*, the `KNX.h` header, and the `<KNX.h>` include change.
- `#define` macros stay SCREAMING_SNAKE (correct per the C++ style guide) — leave
  `KNX_DRIVER_RX/TX/RESN/BAUD` untouched.
- Enum *type names* change (`KNX_DPT`→`KnxDpt`, `KNX_Priority`→`KnxPriority`); their
  *enumerators* (`DPT1`, `DPT232`, `System`, `Normal`, …) do NOT.
- Already-correct `Knx…` names (`KnxValue`, `KnxObject`, `KnxCoordinator`, the `KnxFrame`/
  `KnxCodec` namespaces, …) are unchanged.
- **File placement / library merge (point 4) is DEFERRED** — this pass only renames; it does
  not move the facade out of the objects lib or merge libraries.

**Verify after the bulk edits (and before commit):** `~/.platformio/penv/bin/pio run`
(firmware) AND `~/.platformio/penv/bin/pio test -e native` must be green (**51/51** tests).

**Steps (checklist):**
- [ ] 1. Delete dead `lib/KNX_Common/src/KNX_Defines.h` (unused back-compat shim; old includers gone).
- [ ] 2. Rename headers (`git mv`): `KNX_Common.h`→`KnxCommon.h`, `KNX_Driver.h`→`KnxDriver.h`,
      `KNX_Object/src/KNX.h`→`Konnextor.h`.
- [ ] 3. Rename lib folders (`git mv`): `KNX_Common`→`KnxCommon`, `KNX_Value`→`KnxValue`,
      `KNX_Telegram`→`KnxTelegram`, `KNX_Driver`→`KnxDriver`, `KNX_Object`→`KnxObject`,
      `KNX`→`KnxCoordinator`.
- [ ] 4. Every `library.json`: update `"name"` + `"dependencies"` to the new folder names
      (`KNX_Common`→`KnxCommon`, etc.).
- [ ] 5. Enum type names in `KnxEnums.h` and ALL usages (~16 files for `KNX_DPT`, ~4 for
      `KNX_Priority`): `KNX_DPT`→`KnxDpt`, `KNX_Priority`→`KnxPriority`.
- [ ] 6. Class renames: `KNX_Driver`→`KnxDriver` (in `KnxDriver.h/.cpp`), `KNX`→`Konnextor`
      (facade in `Konnextor.h`).
- [ ] 7. Update includes: `"KNX_Driver.h"`→`"KnxDriver.h"` (in `KnxDriver.cpp` + `Konnextor.h`);
      `src/main.cpp` `<KNX.h>`→`<Konnextor.h>`. (`KNX_Common.h`/`KNX_Defines.h` have no includers.)
- [ ] 8. `src/main.cpp`: `KNX knx(...)`→`Konnextor knx(...)`.
- [ ] 9. Docs: refresh this file's Architecture lib-tree + "User include" wording, and `PLAN.md`
      §12 (tree / dependency arrows / facade text) to the new names.
- [ ] 10. Verify firmware + 51/51 native green → commit → delete this section.

**Tip:** the `KNX_DPT`/`KNX_Priority` and folder/name swaps are safe with `perl -pi -e` over the
file list from `grep -rl`; do the class `KNX`→`Konnextor` rename carefully (word-boundary,
identifier only — never the protocol word in comments).
