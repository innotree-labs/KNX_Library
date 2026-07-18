# PLAN — KNX Library Redesign (Adafruit-style API)

Status: **agreed design, not yet implemented.** Implementation to start in a
later session. This document is the handoff artifact — it captures the decisions
reached, the rationale, and the open forks that remain.

---

## 1. Product context (decides everything below)

We ship a **breakout board + a simple Arduino library** — the "Adafruit model"
for KNX. The value proposition is *simplicity*: it works out of the box, the user
writes a few lines, no ETS, no KNX-stack expertise required.

Decided constraints:

- **No ETS programmability.** Group addresses are **hardcoded in user sketches** —
  an accepted trade-off in exchange for a far simpler experience than
  thelsing/knx or OpenKNX.
- **The user should not need to know DPTs** for common cases. Intent, not datatype,
  is the user-facing vocabulary.
- **Physical/timing layer is an external ATTiny** that behaves like a TP-UART
  (UART peer). Bit-level timing is *not* this library's problem. → The async /
  hardware-timer / RMT discussion from the migration notes is **out of scope**;
  see §9.
- **Portability = stay on the Arduino API.** `#include <Arduino.h>` is the
  enabler for "runs on many controllers" (AVR/ESP32/RP2040/SAMD/STM32), exactly
  like Adafruit. We do **not** decouple from Arduino for portability. (Optional
  host-testability is a separate, low-priority goal — see §9.)

## 2. What we keep from the current codebase

The existing foundation is sound and survives:

- Strictly **downward layering** (physical → telegram → coordinator → device).
- **Dependency injection by pointer, no singletons.**
- **No dynamic allocation** — fixed, bounded arrays.
- The **RX registry** concept (`bind()` today) — generalized in §6.
- Doc discipline (JSDoc on public methods), project style overrides
  (camelCase methods, `#define`, `_camelCase` flags).

**What "keep" means: the architecture and principles above — NOT the old names,
file boundaries, or code style.** Do not preserve thesis-era artifacts out of
inertia. Rename freely (e.g. `KNX_Defines → KNX_Common`, `KNX_TPUART2 →
KNX_Driver`), re-split files, drop dead helpers, and restructure aggressively where
it serves the new design. This is a fresh public library, not a patched thesis;
backward-compatibility with the old internal API is a non-goal. When in doubt,
favor the cleaner structure over the familiar one.

- **Drop the `#ifdef _MSC_VER` / `#pragma region` blocks.** They are MSVC-only
  code-folding hints and add clutter to every file — especially bad in a public
  library's **headers**, which users read to learn the API in whatever editor they
  use (Arduino IDE, VS Code, CLion, vim). The new fine-grained file split (§12)
  makes them unnecessary; use plain `//---- section ----` comments where grouping
  helps. (This intentionally overrides the personal `style_cpp.md` convention for
  this project — a public artifact shouldn't carry one editor's folding markers.)

## 3. The three ways to send (core UX)

All three are front doors onto **one encode path** and **one value currency**
(§4). The DPT is progressively hidden as the tier gets more specific.

| Tier | Example | DPT | Methods / state | Address |
|---|---|---|---|---|
| **Intent object** | `KnxLight kitchen(knx, cmdGa, statusGa);` | hidden inside | rich (`.on/.off/.toggle`), value cache | fixed at construction |
| **Raw object** | `KnxObject o(knx, ga, KNX_DPT::DPT9);` | user gives once | generic (`.write/.onUpdate`), value cache | fixed at construction |
| **Stateless send** | `knx.send("0/4/2", Dpt9(21.5f));` | user gives per call (inside value) | none | **any GA at runtime** |

- Intent objects cover the common 80% (Light, DimmLight, Blind, …).
- Raw `KnxObject` covers types we didn't wrap — user specifies the DPT once,
  gets only generic methods (we don't know their intent).
- Stateless `send()` is the only tier that can address a **runtime-computed GA**
  (loop, value from WiFi, etc.). It has **no receive** — see §6.

The `knx` above is the **node object**, constructed in one line with the driver hidden:
`KNX knx("1.1.5");` (the physical address is the only argument). `KNX` is a thin facade over the
injectable `KnxCoordinator` core — see §12 "User include" for why they are split.

## 4. The value type — `Dpt` / `KnxValue` (keystone)

**Decision: value objects, NOT a `sendDPTn(...)` method family.** (Fork resolved
in favor of value types.)

Replace the unsafe `sendTelegram(ga, dpt, void* value)` — **delete it** — with a
single typed value that carries its own DPT and correctly-typed payload:

```cpp
knx.send("0/1/1", Dpt1(true));     // Dpt1 accepts only bool
knx.send("0/4/2", Dpt9(21.5f));    // Dpt9 accepts only float
knx.send("0/2/5", Dpt5(200));      // Dpt5 accepts only uint8_t
```

- A payload/DPT mismatch becomes a **compile error**, not a silent wrong
  telegram (this is the failure class that produced the `DimmIncrement` bug).
- The DPT is explicit here (correct for this advanced tier) but lives **inside
  the value**, never as a loose parallel argument.
- The **same value type is the library's one currency**:
  - `obj.write(Dpt9(21.5f))` — raw object write
  - `light.on()` — intent object constructs `Dpt1(true)` internally
  - the RX side hands a decoded value back out (§6)

**Internal representation (decided): tagged union.** `KnxValue` is
`{ KNX_DPT dpt; union{ bool; uint8_t; …; float; datetime; rgb } payload; }` — one
tag + a union sized to the largest v1 payload (DPT19 datetime, 8 bytes → whole
value ~10–12 bytes). The `Dpt1()`/`Dpt9()`/… constructors just populate it and the
codec does union↔wire-bytes in one place. Chosen over a raw byte buffer: typed
named access reads cleanly and avoids the hand-marshalling that produced the
`DimmIncrement` bug.

**Runtime-variable DPT** (rare; a generic bus tool where the DPT itself is a
variable): build the same `KnxValue` through checked factory calls and pass it to
`send()`. Encapsulated + validated inside one type — **never a bare `void*`.**
Bottom escape hatch; most users never touch it.

## 5. `KnxObject` base + intent subclasses

- `KnxObject` holds: GA(s), DPT, cached last value, callback (function pointer).
- Intent subclasses (`KnxLight`, `KnxDimmLight`, `KnxBlind`, …) wrap a
  `KnxObject`, **hide the DPT**, and expose intent methods. Their methods are
  thin sugar over the same `send()` / `write()` path — **one encode
  implementation, not a parallel one.**
- **Callbacks are plain function pointers** (`void(*)(...)`, like today's
  `KNX_Callback`). **No capturing lambdas / `std::function`** — they heap-allocate
  and bloat on AVR.

### Command GA vs status GA (decided: combine on intent objects)

Real installations command a light on one GA (`0/1/1`) and read status on another
(`0/3/0`). Intent objects take **both**, with status defaulting to the command GA
when omitted:

```cpp
KnxLight kitchen(knx, "0/1/1", "0/3/0");  // sends to cmd, listens on status
KnxLight lamp(knx, "0/1/2");              // status defaults to cmd GA
```

`.toggle()` flips relative to the **status-fed cached value**, so it tracks the
real bus state, not just what we last sent. This kills the per-class
`lastToggleState` / `toggleState` bookkeeping in the current Device layer.

## 6. Receiving model

One idea drives it: **an object knows its own GA + DPT, so it decodes on receive
without the user restating either.** This fixes the current asymmetry where
`bind(GA, DPT, cb)` leaks the DPT while sending hides it.

Flow:

1. On construction (or first `.onUpdate()`), the object links itself into the
   coordinator's RX registry — an **intrusive linked list** (coordinator holds a
   head pointer; each object carries a `next` pointer). Replaces the `bindings[]`
   array; no fixed cap, no `MAX_OBJECTS`, memory scales with declared objects.
   Keep the "walk all receivers and match" loop from `handleParsedTelegram`.
2. Incoming telegram → scan registry for objects whose **listen GA** matches.
3. Decode payload with **the object's** DPT.
4. Update the object's **cached value**.
5. Fire the object's callback.

Callback shapes (decoded, no user-side DPT):

```cpp
kitchen.onUpdate(void(*)(bool on));            // intent → decoded native type
o.onUpdate(void(*)(const KnxValue& v));        // raw → generic value, v.asFloat() etc.
```

Boundaries / decisions:

- **Dynamic send is free-function; dynamic receive needs an object** (somewhere to
  hold the callback + cache). The stateless tier has no receive by definition.
  Document this plainly.
- **Lifetime:** the list holds raw pointers → objects must outlive it (globals /
  class members — already the pattern, e.g. `rocker1`). The destructor **unlinks
  the object from the list** so a scoped object can't dangle. Document "declare KNX
  objects at global/member scope."
- **Value cache read-back:** `.value()` returns last-known with no bus traffic;
  it's what makes `.toggle()` and "is it on?" work without external state.

## 7. Registry change (concrete)

- Today: `KNX_Binding bindings[MAX_BINDINGS]` of `{GA, DPT, callback}` — a fixed
  array.
- New: **intrusive linked list** of `IKnxReceiver`. The coordinator holds a head
  pointer; each `KnxObject` carries a `next` pointer, links itself in on
  construction, and unlinks in its destructor. No fixed cap, no `MAX_OBJECTS` knob —
  memory scales exactly with the objects the user declares. The incoming-telegram
  handler walks the list, matches on listen GA, and delegates decode + cache +
  callback to the object.
- `KNX::bind(...)` as a free-standing DPT-leaking call is **removed**; its role is
  served by constructing an object (raw is enough:
  `KnxObject o(knx, ga, dpt); o.onUpdate(cb);`).

## 8. Rebuild the Device layer on top

- `TwoButtonDimming` / `TwoButtonSwitching` / `SingleButtonSwitching` take
  `KnxLight` / `KnxObject` references instead of holding `String` GAs and rolling
  their own toggle state. The button state machine (`SingleButtonOperation` etc.)
  is unchanged; only what it *commands* changes to the object currency.
- Replace `String` group addresses in hot paths with a **packed GA value type**
  (`uint16_t` under the hood) constructed once, to avoid per-send re-parsing and
  heap churn. (Arduino `String` is allowed at construction/config time, not in the
  send loop.)

## 9. Correctness + housekeeping folded in

- **`L_Data.con`:** stop hard-returning `true` from the send path. The ATTiny
  (TP-UART-like) returns a transmit confirmation — parse it and surface real
  success/failure to the caller. **No retry logic in the library:** the ATTiny owns
  bus-level repetition, exactly like a TP-UART; the library only reports the final
  result.
- **DPT subsystem:** make encode/decode **symmetric** and the single source of
  truth. Concretely fixes the `DimmIncrement` enum: `Stop` and `Percent_1_5` both
  `0x00` (collision), and the percent→stepcode ordering is inverted
  (stepcode encodes number of intervals: stepcode 1 ≈ 100 %, 7 ≈ 1.5 %).
  Verify against the DPT3.007 spec during implementation.
- **Decoders currently cover only DPT1–5** and `KnxTelegramData` only holds
  dpt1–5. Extend to the DPT set the value types expose.
- **Host-testability (LOW priority, optional):** keep the *pure* protocol math
  (framing, checksum, DPT codecs, GA packing) free of `<Arduino.h>` so it can
  compile + unit-test on a PC in CI. This is a quality multiplier, not required;
  it would have caught the `DimmIncrement` bug in a 5-line test. Not for
  portability — Arduino is the portability layer.

## 10. Decisions (were open forks — now settled)

1. **`KnxValue` internals → tagged union** (`{ KNX_DPT dpt; union payload; }`).
   Typed named access, codec does union↔bytes in one place; ~10–12 bytes (DPT19 is
   the largest v1 payload). See §4.
2. **Intent class set v1 → actuators + values:**
   - Actuators: `KnxLight` (DPT1), `KnxDimmLight` (DPT1+DPT3), `KnxBlind` (DPT1).
   - Values: `KnxTemperature` / `KnxHumidity` (DPT9), `KnxTime` (DPT10),
     `KnxDate` (DPT11), `KnxDateTime` (DPT19), `KnxRGB` (DPT232.600 — **enum + codec
     must be extended, see §12**), `KnxPercent` (DPT5.001), `KnxChar` (DPT4),
     `KnxFloat` (DPT14).
3. **Retry policy → none in the library.** The ATTiny handles repetition like a
   TP-UART; the library surfaces the `L_Data.con` result only. See §9.
4. **Registry → intrusive linked list**, no `MAX_OBJECTS`, no config knob. See §7.

## 11. Note: CLAUDE.md physical-layer section updated

DONE (this session, commit `4b37b30`): `CLAUDE.md`'s old "Planned migration:
TP-UART2 → STKNX" section was rewritten to "Physical layer: STKNX behind an ATTiny
co-processor" — the ATTiny handles timing and presents a TP-UART-like UART
interface (superseding the earlier bit-bang-on-main-MCU plan).

## 12. File layout (agreed module structure)

Renamed/new modules under `lib/`. Dependency flow is strictly downward, acyclic.

```
lib/
  KNX_Common/    (was KNX_Defines — renamed; "Defines" meant #defines, but it holds
                  types + contracts. "Core" avoided → clashes with the KNX coordinator.)
    KNX_Common.h        umbrella — #includes the rest (old include path aliases here)
    KnxEnums.h          KNX_DPT, priorities, Switching/Dimming behaviours, DimmIncrement
    KnxAddress.h        packed GroupAddress / PhysicalAddress + parse/format inlines
    KnxTelegramTypes.h  ParsedTelegram, KnxEvent
    KnxInterfaces.h     IKnxDriver, IKnxReceiver  (own file, separate from passive types)
  KNX_Value/     NEW — value currency + symmetric codec. PURE (Arduino-free), host-testable.
    KnxValue.h          KnxValue tagged type + Dpt1(..)..Dpt19(..) typed ctors
    KnxCodec.h/.cpp     symmetric native<->raw encode/decode by DPT (single source of truth)
  KNX_Telegram/  framing, checksum, APDU, addressing; delegates value coding to KNX_Value
  KNX_Driver/    concrete ATTiny/TP-UART UART driver : IKnxDriver  (replaces KNX_TPUART2)
  KNX/           DI core (was a class named KNX; renamed to avoid clashing with the facade)
    KnxCoordinator.h/.cpp  class KnxCoordinator: IKnxReceiver linked-list head, send(ga, KnxValue),
                           dispatch; holds the injected IKnxDriver*. Arduino-free, host-testable.
  KNX_Object/    KnxObject : IKnxReceiver (base, carries `next` ptr) + intent classes BY DOMAIN
    KNX.h               public facade: #includes driver + coordinator + values + all objects, then
                        defines class KNX : public KnxCoordinator (owns a KNX_Driver, built from a
                        physical-address string). The single user include. See §12 "User include".
    KnxObject.h
    KnxLighting.h       KnxLight (DPT1), KnxDimmLight (DPT1+DPT3), KnxRGB (DPT232)
    KnxCovers.h         KnxBlind (DPT1)
    KnxClimate.h        KnxTemperature (DPT9), KnxHumidity (DPT9)
    KnxDateTime.h       KnxTime (DPT10), KnxDate (DPT11), KnxDateTime (DPT19)
    KnxScalars.h        KnxPercent (DPT5.001), KnxChar (DPT4), KnxFloat (DPT14)
  examples/      KNX_Device (button state machines) demoted here — optional helper from the
                 thesis, NOT part of the core surface, NOT included by KNX.h
```

Dependency arrows (downward, acyclic; lib `KNX` = the `KnxCoordinator` core):
- `KNX_Object → {KNX (KnxCoordinator), KNX_Value, KNX_Telegram}`; its `KNX.h` facade also
  → `KNX_Driver` (the facade is the only thing above the driver)
- `KNX (KnxCoordinator) → {KNX_Telegram, KNX_Value, KnxInterfaces}`
- `KNX_Telegram → KNX_Value → KNX_Common`
- `KNX_Driver → KnxInterfaces` (implements `IKnxDriver`)
- everything → `KNX_Common`

**Cycle-breaking (dependency inversion).** Interfaces are abstract base classes
(pure virtual, no data) that sit *below* their consumers so both sides can see them:
- `IKnxReceiver` in `KNX_Common`. `KnxCoordinator` holds the intrusive linked-list head of
  `IKnxReceiver` and **never includes `KnxObject.h`**. `KnxObject` includes `KnxCoordinator.h`
  (to `send()` + self-register) and implements `IKnxReceiver`. Arrow is one-way → no cycle. The
  public `KNX.h` facade sits *above* both (top of the DAG, nothing includes it), so it can bundle
  the coordinator + objects + driver without reintroducing a cycle.
- `IKnxDriver` in `KNX_Common`. `KnxCoordinator` holds an injected `IKnxDriver*`; `KNX_Driver`
  implements it. Swapping ATTiny/TP-UART/mock = change the injection only. The `KNX` facade
  subclass wires a concrete `KNX_Driver` in for the common case (see "User include" above).
- Promote interfaces to a dedicated `KNX_Interfaces` module only if the contract set
  grows past these two.

**User include + construction (as-built — the `KNX` facade decision).** A sketch writes
**one include and one object**:

```cpp
#include <KNX.h>
KNX knx("1.1.5");                          // driver hidden; physical address typed once
KnxLight kitchen(knx, "0/1/1", "0/3/0");
```

This split the original single `KNX` class into two, to satisfy *both* the Adafruit-style
one-liner and the host-testable DI core (they conflicted — see cycle-breaking below):

- **`KnxCoordinator`** (`KnxCoordinator.h`, lib `KNX`) is the DI core: Arduino-free, holds an
  injected `IKnxDriver*`, `send(ga, KnxValue)` + the intrusive `IKnxReceiver` registry. It is the
  type intent objects reference (`KnxCoordinator&`) and the type the host tests drive with a mock.
- **`KNX`** (defined in the `KNX.h` facade) is a thin **Arduino subclass** —
  `class KNX : public KnxCoordinator` — that *owns* a concrete `KNX_Driver` member and is built
  from just the physical-address string. It feeds that address to both the frame source
  (`KnxCoordinator`) and the transceiver `U_SetAddress` (`KNX_Driver`) internally, so the user
  never instantiates or injects a driver and never types the address twice. Because `KNX` **is-a**
  `KnxCoordinator`, intent objects bind to it directly.
- **`KNX.h`** is the public facade (lives in `KNX_Object`, atop the DAG): it `#include`s the
  driver + `KnxCoordinator` + `KnxValue` + every intent domain header, then defines the `KNX`
  class. It is Arduino-only (pulls the driver) — the host tests include `KnxCoordinator.h` and the
  object headers directly, never the facade, so the Arduino driver never enters a native build.
- **Advanced escape hatch:** a user who wants a different transport (custom driver, on-hardware
  mock) constructs `KnxCoordinator knx(&myDriver, "1.1.5")` directly — the injectable path is
  preserved, just not the default one has to type.

**Host-testable core** (refines §9): `KNX_Common` + `KNX_Value` (+ most of
`KNX_Telegram`) are Arduino-free by construction. Approach — **write testable code
now, wire CI later**: build the codec Arduino-free from day one, add the native test
env (`pio test -e native` + Unity) and a GH Actions job once the codec exists and has
earned it. Build the **test-case catalog from the spec, not the implementation**
(every DPT round-trip, boundary values, the DPT3 stepcode table that bit us, GA edge
cases like `0/0/0`, DPT16 truncation, checksum corruption) so tests catch a wrong
implementation instead of ratifying it.

**DPT coverage note:** the current `KNX_DPT` enum stops at DPT19. `KnxRGB` needs
**DPT232.600** (3-byte R/G/B) — extend the enum (`KnxEnums.h`) and add a codec
entry (`KnxCodec`) for it. All other v1 intent classes map to DPTs already in the
enum (see §10).
