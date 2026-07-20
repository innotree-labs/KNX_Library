# PLAN — KNX Library Redesign (Adafruit-style API)

Status: **as-built record — implemented and shipped** on branch `knx-redesign-common-value`.
This is no longer a proposal: the sections below describe what exists. The rationale is kept
deliberately, because *why* a decision was made outlives the commit that made it — where the
build diverged from the plan as written, the section says so explicitly.

**Verifying the tree** (`pio` is not on `PATH`):

```
~/.platformio/penv/bin/pio run             # firmware — Seeed XIAO ESP32-C6
~/.platformio/penv/bin/pio test -e native  # 51/51 host Unity tests
```

**Still open — the one thing a bench can settle and a host cannot:** the `L_Data.con` byte
values in `KnxDriver` (`CON_MASK` / `CON_PATTERN` / `CON_POSITIVE`) are derived from the TP-UART2
spec and are **unverified against real hardware**. The thesis never read the con byte at all (it
hard-returned `true`), so there is no reference implementation to check them against. Everything
above the driver is host-tested; if send confirmation misbehaves on the bench, start there.

**Diverged from the plan as written:** §8 (rebuild the Device layer) was **waived**, not built —
see that section. Everything else landed substantially as described.

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
inertia. Rename freely (e.g. `KNX_Defines → KnxCommon`, `KNX_TPUART2 →
KnxDriver`), re-split files, drop dead helpers, and restructure aggressively where
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
| **Raw object** | `KnxObject o(knx, ga, KnxDpt::DPT9);` | user gives once | generic (`.write/.onUpdate`), value cache | fixed at construction |
| **Stateless send** | `knx.send("0/4/2", Dpt9(21.5f));` | user gives per call (inside value) | none | **any GA at runtime** |

- Intent objects cover the common 80% (Light, DimmLight, Blind, …).
- Raw `KnxObject` covers types we didn't wrap — user specifies the DPT once,
  gets only generic methods (we don't know their intent).
- Stateless `send()` is the only tier that can address a **runtime-computed GA**
  (loop, value from WiFi, etc.). It has **no receive** — see §6.

The `knx` above is the **node object**, constructed in one line with the driver hidden:
`Konnextor knx("1.1.5");` (the physical address is the only argument). `Konnextor` is a thin facade over the
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
`{ KnxDpt dpt; union{ bool; uint8_t; …; float; datetime; rgb } payload; }` — one
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
  and bloat on AVR. A **non-capturing** lambda is fine: it implicitly converts to a
  function pointer, so it compiles into the same storage with no heap. Both a named
  free function and a non-capturing lambda are therefore valid at every `onUpdate`
  — which style the *examples* teach is a separate UX decision, see §6.

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

**Sketch-level callback style (as-built, decided on UX grounds).** The signature above accepts
either form, so this is purely what the *examples* teach — and examples get copied, so the
choice matters. Decision: **named free functions, not inline lambdas.**

```cpp
void onKitchenChanged(bool on);              // prototype, above setup()

void setup() {
    kitchen.onUpdate(onKitchenChanged);      // one line per binding
    roomTemp.onUpdate(onTemperatureChanged);
}

void loop() { knx.loop(); }

void onKitchenChanged(bool on) { … }         // body, below loop()
```

Rationale, strongest first:
- **`setup()` becomes a flat wiring manifest.** Inline lambda bodies interleave configuration
  and behaviour, so "what is this device bound to?" can't be answered without reading handler
  internals. One line per binding answers *what*, not *how*.
- **It is the Arduino idiom** (`attachInterrupt(pin, handler, …)`) — native to the audience an
  Adafruit-style library targets.
- A named handler documents intent, is reusable across several objects, and appears in a stack
  trace; a lambda does none of these.

Cost, and it is real: the bodies sit below `loop()` (the thesis layout), so in a **`.cpp`** each
handler needs a **forward declaration** — C++ requires declaration before use. A user's **`.ino`
never pays this**: the Arduino builder generates prototypes. Omitting one yields a confusing
error for a beginner, so the showcase comments explain why the prototypes are there. Handlers
defined *above* `setup()` would avoid the prototypes entirely and was the considered
alternative; the thesis order won on keeping the file top focused on configuration.

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

- Was (thesis): `KNX_Binding bindings[MAX_BINDINGS]` of `{GA, DPT, callback}` — a fixed
  array. Deleted.
- Now: **intrusive linked list** of `IKnxReceiver`. The coordinator holds a head
  pointer; each `KnxObject` carries a `next` pointer, links itself in on
  construction, and unlinks in its destructor. No fixed cap, no `MAX_OBJECTS` knob —
  memory scales exactly with the objects the user declares. The incoming-telegram
  handler walks the list, matches on listen GA, and delegates decode + cache +
  callback to the object.
- `KNX::bind(...)` as a free-standing DPT-leaking call is **removed**; its role is
  served by constructing an object (raw is enough:
  `KnxObject o(knx, ga, dpt); o.onUpdate(cb);`).

## 8. Rebuild the Device layer on top — WAIVED, not implemented

> **As-built: this section was deliberately dropped.** The user chose a clean-API showcase over
> rewiring the thesis button layer ("I don't need the main program anymore"), so `src/main.cpp`
> demonstrates the library directly and the button state machines were demoted to
> `examples/KNX_Device/` — **not in the build, and they do not compile against the current API**
> (they still call deleted methods). Kept as history only; the porting steps below remain valid
> if the layer is ever revived. The packed-GA point in the second bullet *did* ship — it landed
> in `KnxAddress` and the object layer, independently of the Device rebuild.

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
  **As-built:** fixed and spec-verified — `DimmIncrement` is renumbered to the real DPT3.007
  stepcodes (`Stop`=0 … `Percent_1_5`=7), with round-trip tests. Note this **changes runtime
  behaviour** versus the thesis (e.g. `Percent_6` now emits stepcode 5 ≈ 6.25 %, was 2 ≈ 50 %);
  intended, but only ever confirmed on hardware.
- **Decoders currently cover only DPT1–5** and `KnxTelegramData` only holds
  dpt1–5. Extend to the DPT set the value types expose.
  **As-built:** done — `KnxCodec` covers DPT1–14, 19 and 232, symmetric in both directions.
  `KnxTelegramData` no longer exists: decoding moved to the receiving object, which owns its DPT.
- **Host-testability (LOW priority, optional):** keep the *pure* protocol math
  (framing, checksum, DPT codecs, GA packing) free of `<Arduino.h>` so it can
  compile + unit-test on a PC in CI. This is a quality multiplier, not required;
  it would have caught the `DimmIncrement` bug in a 5-line test. Not for
  portability — Arduino is the portability layer.
  **As-built: this got promoted from "optional" to load-bearing.** `KnxCommon` / `KnxValue` /
  `KnxTelegram` / `KnxCoordinator` are Arduino-free, and the 51-test host suite (`pio test -e
  native`) drove the whole redesign — it is what makes the coordinator testable with a mock
  driver, and what keeps the Arduino-only facade out of a host build. CI wiring is still not set
  up; the suite runs locally.

## 10. Decisions (were open forks — now settled)

1. **`KnxValue` internals → tagged union** (`{ KnxDpt dpt; union payload; }`).
   Typed named access, codec does union↔bytes in one place; ~10–12 bytes (DPT19 is
   the largest v1 payload). See §4.
2. **Intent class set v1 → actuators + values:**
   - Actuators: `KnxLight` (DPT1), `KnxDimmLight` (DPT1+DPT3), `KnxBlind` (DPT1).
   - Values: `KnxTemperature` / `KnxHumidity` (DPT9), `KnxTime` (DPT10),
     `KnxDate` (DPT11), `KnxDateTime` (DPT19), `KnxRGB` (DPT232.600 — enum + codec
     were extended for it, as-built), `KnxPercent` (DPT5.001), `KnxChar` (DPT4),
     `KnxFloat` (DPT14).
3. **Retry policy → none in the library.** The ATTiny handles repetition like a
   TP-UART; the library surfaces the `L_Data.con` result only. See §9.
4. **Registry → intrusive linked list**, no `MAX_OBJECTS`, no config knob. See §7.

## 10b. Debug mode (as-built — added after the redesign shipped)

**Requirement:** one runtime switch, `knx.enableDebugMode(bool)`, that traces everything the
library does — for diagnosing a user's misbehaving installation without asking them to
recompile. It replaces the thesis's nine `#ifdef DEBUG` sites (German-language address
warnings in `KnxAddress.h`, the only ones that ever existed).

**The constraint that shaped it.** The interesting events happen in `KnxCoordinator`,
`KnxFrame` and `KnxCodec` — all Arduino-free, and the latter two are *stateless namespaces*
with no `this` to reach a per-instance flag through. `#ifdef ARDUINO` alone solves the compile
problem (host builds simply drop the `Serial` calls) but not the flag-reachability problem: a
namespace free function cannot see state set on a `knx` object.

**Decision: one library-wide `bool`** in `KnxCommon/src/KnxDebug.h`, held in an inline
function-local static (keeps `KnxCommon` header-only under C++14 — `inline` variables would
need C++17, and the native env is `-std=c++14`). `log()` / `logBytes()` carry *both* guards
internally, so call sites elsewhere stay free of preprocessor noise.

This is a **deliberate, agreed exception to the no-global-singletons rule.** The reasoning: it
is a log level, not a service locator; the alternative was threading a sink parameter through
every framing and codec signature. Consequence, documented rather than hidden: the switch is
library-wide, so enabling it on one node enables it everywhere.

Rejected alternatives: an injected `IKnxLogSink` interface (DI-consistent and testable, but
cannot reach the stateless namespaces — the exact gap that mattered); a compile-time
`KNX_DEBUG` master switch (defeats the "user hits a problem, flips a switch" purpose, since it
requires a recompile).

**Measured cost:** +2.4 KB flash, +0 B RAM on ESP32-C6, compiled in and disabled. A disabled
call is one bool test. No compile-time strip switch exists yet — add one only if an AVR target
ever needs it, on the strength of a measurement rather than speculation. Note arguments are
still evaluated when logging is off; expensive call sites use `if (KnxDebug::isEnabled())`.

## 10c. Spec conformance pass (as-built — after a read of the KNX v3.0.0 specs)

The KNX Standard v3.0.0 PDFs now live in `docs/`. Reading `03_02_02 Communication Medium TP1`
§2.2 and `03_03_04 Transport Layer` §2 against the framing layer turned up three deviations,
all since fixed, plus one deferral recorded here.

**Fixed — frame sync accepted 1 of 8 legal control bytes.** `KnxReassembler` exact-matched
`0xBC`. An L_Data_Standard control field is `1 0 r 1 p1 p0 0 0` (TP1 Figure 28): bit 5 is the
repeat flag and bits 3..2 the priority, so eight values are legal — `0xB0/B4/B8/BC` original and
`0x90/94/98/9C` repeated. The old code therefore dropped **every repeated frame** — i.e. exactly
the retransmission a sender emits after a lost or NAKed original — and everything above low
priority. Now masked: `(ctrl & 0xD3) == 0x90`, which still rejects extended, acknowledge and
poll frames. `MIN_FRAME` also dropped 9 → 8, because TP1 §2.2.4.5 allows length 0 (a frame
ending after octet 6) and that is exactly the shape of a `T_Connect`.

**Fixed — `KnxPriority` names were shifted by one.** Spec values (bits 3..2) are system `00`,
normal `01`, urgent `10`, low `11`. The enum had `High = 0x04` (really *normal*) and
`Normal = 0x0C` (really *low*), with no name for low at all. Renamed to
`System/Normal/Urgent/Low`. **Wire behaviour is deliberately unchanged:** `KnxFrame::build`'s
default moved from the old `Normal` (0x0C) to the new `Low` (0x0C), the same byte, so group
communication still goes out at low priority and the control byte is still `0xBC`.

**Fixed — individual (point-to-point) addressing was unimplemented.** `parse()` ignored octet 5
bit 7 (AT) and unpacked every destination as a group address, so a telegram addressed to *this
device* — a descriptor-read ping, a transport connect, ETS programming traffic — was misparsed
as group traffic. See §10d for the structure.

**Deferred — L_Data_Extended frames (frame type bit 7 = 0).** Not reassembled, not parsed, by
choice. TP1 §2.2.5 uses them for APDUs > 15 octets and for LTE-HEE extended addressing; a
group-communication library for switching, dimming, blinds and scalars never needs one, and
supporting them means a second length rule (8-bit LG with an `0xFF` escape), a CTRLE octet and
a larger RX buffer. The mask above rejects them cleanly rather than misparsing them. Revisit if
a DPT beyond 14 octets or LTE mode is ever wanted.

**Open, not a defect — no link-layer ACK generation.** `U_ACK_INFO_ACK/BUSY/NACK` are declared
in `KnxDriver.h` and never sent, so the stack never emits the acknowledge frame of TP1 §2.2.7.
Whether that is a bug depends on the TP-UART auto-ACKing addressed frames once `U_SetAddress`
is applied — that is Siemens datasheet behaviour, not settled by these specs. Bench evidence so
far is consistent with auto-ACK (status telegrams arrive with no repeat storm). Verify against
the datasheet before acting.

Confirmed correct while reading, worth not re-litigating: the checksum is NOT-XOR (odd parity)
as in TP1 Figure 31; `LG = APDU octets - 1` matches real bus traffic; hop count 6 is the
conventional Network Layer default; `KnxPercent`'s DPT 5.001 scaling matches the spec table
(50 % → `0x80`, 100 % → `0xFF`); and Session and Presentation layers are *empty* in KNX
(`03_03_05`, `03_03_06`), so having no L5/L6 is conformance, not a gap.

## 10d. Point-to-point addressing (as-built — structure)

The constraint was to add device-management addressing **without disturbing the group path**,
which is the whole existing library surface. What that ruled out and what it produced:

- **`ParsedTelegram` grew, it did not change.** `target`, `type`, `payload` and the inline-6
  fields keep their names, positions and meaning, so every existing consumer compiles and
  behaves identically. Added alongside them: `addressType`, `individualTarget`, `tpci`,
  `sequenceNumber`, `apci`, `hasApci`. `addressType` discriminates which destination field is
  live.
- **`IKnxReceiver` was left alone.** Widening `matches(uint16_t)` to carry an address type would
  have touched every intent class for a case none of them care about. Instead `KnxCommon` gained
  a separate `IKnxDeviceHandler`, and the coordinator holds **one optional pointer** to it — not
  a registry, because a point-to-point telegram has exactly one legitimate recipient (this
  device), unlike a group address which legitimately fans out.
- **Routing happens in `KnxCoordinator::loop()`**, which branches on `addressType` to either the
  existing group `dispatch()` or the new `dispatchIndividual()`. The latter compares the target
  against this device's physical address; anything else is foreign traffic and is logged, not
  delivered. This separation is load-bearing: the two address spaces overlap numerically — 1.1.5
  packs to the same `uint16_t` as group address 2/1/5 — so type, not value, is what keeps a
  point-to-point telegram out of the group registry. There is a test for exactly that.
- **Framing gained `buildIndividual()` (T_Data_Individual + raw APCI) and `buildControl()`**
  (TPCI-only T_Connect/T_Disconnect/T_ACK/T_NAK), with the octet 0..5 header writer factored out
  so the three builders cannot drift apart. TPCI decoding follows Transport Layer Figure 3.
- **No transport state machine.** `03_03_04` §5 specifies a full connection-oriented state
  machine (connect/ack/seqno/timeout/repeat). Deliberately not implemented: the plumbing above
  makes point-to-point telegrams parseable, routable and sendable, which is what a ping needs.
  A device that wants to *hold* an ETS connection needs §5, and that is a separate piece of work.

Application-service codes come from `03_03_07` Application Layer §3: `A_DeviceDescriptor_Read`
is APCI `0x300`, `A_Restart` `0x380`. `apci` is exposed raw rather than enumerated, because the
table is long and device management is not the library's focus — the caller names what it needs.

## 11. Note: CLAUDE.md physical-layer section updated

DONE (this session, commit `4b37b30`): `CLAUDE.md`'s old "Planned migration:
TP-UART2 → STKNX" section was rewritten to "Physical layer: STKNX behind an ATTiny
co-processor" — the ATTiny handles timing and presents a TP-UART-like UART
interface (superseding the earlier bit-bang-on-main-MCU plan).

## 12. File layout (agreed module structure)

Renamed/new modules under `lib/`. Dependency flow is strictly downward, acyclic.

```
lib/
  KnxCommon/     (was KNX_Defines — renamed; "Defines" meant #defines, but it holds
                  types + contracts. "Core" avoided → clashes with the KNX coordinator.)
    KnxCommon.h         umbrella — #includes the rest
    KnxEnums.h          KnxDpt, KnxPriority, Switching/Dimming behaviours, DimmIncrement
    KnxAddress.h        packed GroupAddress / PhysicalAddress + parse/format inlines
    KnxTelegramTypes.h  ParsedTelegram
    KnxInterfaces.h     IKnxDriver, IKnxReceiver  (own file, separate from passive types)
  KnxValue/      NEW — value currency + symmetric codec. PURE (Arduino-free), host-testable.
    KnxValue.h          KnxValue tagged type + Dpt1(..)..Dpt19(..) typed ctors
    KnxCodec.h/.cpp     symmetric native<->raw encode/decode by DPT (single source of truth)
  KnxTelegram/   framing, checksum, APDU, addressing; delegates value coding to KnxValue
  KnxDriver/     concrete ATTiny/TP-UART UART driver : IKnxDriver  (replaces KNX_TPUART2)
  KnxCoordinator/ DI core (was a class named KNX; renamed to avoid clashing with the facade)
    KnxCoordinator.h/.cpp  class KnxCoordinator: IKnxReceiver linked-list head, send(ga, KnxValue),
                           dispatch; holds the injected IKnxDriver*. Arduino-free, host-testable.
  KnxObject/     KnxObject : IKnxReceiver (base, carries `next` ptr) + intent classes BY DOMAIN
    KnxObject.h
    KnxLighting.h       KnxLight (DPT1), KnxDimmLight (DPT1+DPT3), KnxRGB (DPT232)
    KnxCovers.h         KnxBlind (DPT1)
    KnxClimate.h        KnxTemperature (DPT9), KnxHumidity (DPT9)
    KnxDateTime.h       KnxTime (DPT10), KnxDate (DPT11), KnxDateTime (DPT19)
    KnxScalars.h        KnxPercent (DPT5.001), KnxChar (DPT4), KnxFloat (DPT14)
  Konnextor/     the public surface, alone in its own library at the top of the DAG
    Konnextor.h         public facade: #includes driver + coordinator + values + all objects, then
                        defines class Konnextor : public KnxCoordinator (owns a KnxDriver, built
                        from a physical-address string). The single user include. See "User include".
  examples/      KNX_Device (button state machines) demoted here — optional helper from the
                 thesis, NOT part of the core surface, NOT included by Konnextor.h
```

Dependency arrows (downward, acyclic):
- `Konnextor → {KnxDriver, KnxObject, KnxCoordinator, KnxValue, KnxCommon}` — the facade lib is
  the sole root and the only thing above the driver; nothing includes it
- `KnxObject → {KnxCoordinator, KnxValue, KnxTelegram}`
- `KnxCoordinator → {KnxTelegram, KnxValue, KnxInterfaces}`
- `KnxTelegram → KnxValue → KnxCommon`
- `KnxDriver → KnxInterfaces` (implements `IKnxDriver`)
- everything → `KnxCommon`

**Cycle-breaking (dependency inversion).** Interfaces are abstract base classes
(pure virtual, no data) that sit *below* their consumers so both sides can see them:
- `IKnxReceiver` in `KnxCommon`. `KnxCoordinator` holds the intrusive linked-list head of
  `IKnxReceiver` and **never includes `KnxObject.h`**. `KnxObject` includes `KnxCoordinator.h`
  (to `send()` + self-register) and implements `IKnxReceiver`. Arrow is one-way → no cycle. The
  public `Konnextor.h` facade sits *above* both (top of the DAG, nothing includes it), so it can
  bundle the coordinator + objects + driver without reintroducing a cycle.
- `IKnxDriver` in `KnxCommon`. `KnxCoordinator` holds an injected `IKnxDriver*`; `KnxDriver`
  implements it. Swapping ATTiny/TP-UART/mock = change the injection only. The `Konnextor` facade
  subclass wires a concrete `KnxDriver` in for the common case (see "User include" above).
- Promote interfaces to a dedicated `KnxInterfaces` module only if the contract set
  grows past these two.

**User include + construction (as-built — the `Konnextor` facade decision).** A sketch writes
**one include and one object**:

```cpp
#include <Konnextor.h>
Konnextor knx("1.1.5");                    // driver hidden; physical address typed once
KnxLight kitchen(knx, "0/1/1", "0/3/0");
```

This split the original single `KNX` class into two, to satisfy *both* the Adafruit-style
one-liner and the host-testable DI core (they conflicted — see cycle-breaking below):

- **`KnxCoordinator`** (`KnxCoordinator.h`, lib `KnxCoordinator`) is the DI core: Arduino-free,
  holds an injected `IKnxDriver*`, `send(ga, KnxValue)` + the intrusive `IKnxReceiver` registry. It
  is the type intent objects reference (`KnxCoordinator&`) and the type host tests drive with a mock.
- **`Konnextor`** (defined in the `Konnextor.h` facade) is a thin **Arduino subclass** —
  `class Konnextor : public KnxCoordinator` — that *owns* a concrete `KnxDriver` member and is built
  from just the physical-address string. It feeds that address to both the frame source
  (`KnxCoordinator`) and the transceiver `U_SetAddress` (`KnxDriver`) internally, so the user never
  instantiates or injects a driver and never types the address twice. Because `Konnextor` **is-a**
  `KnxCoordinator`, intent objects bind to it directly.
- **`Konnextor.h`** is the public facade, and the sole content of its own `Konnextor` library at
  the top of the DAG: it `#include`s the driver + `KnxCoordinator` + `KnxValue` + every intent
  domain header, then defines the `Konnextor`
  class. It is Arduino-only (pulls the driver) — the host tests include `KnxCoordinator.h` and the
  object headers directly, never the facade, so the Arduino driver never enters a native build.
- **Advanced escape hatch:** a user who wants a different transport (custom driver, on-hardware
  mock) constructs `KnxCoordinator knx(&myDriver, "1.1.5")` directly — the injectable path is
  preserved, just not the default one has to type.

**Host-testable core** (refines §9): `KnxCommon` + `KnxValue` (+ most of
`KnxTelegram`) are Arduino-free by construction. Approach — **write testable code
now, wire CI later**: build the codec Arduino-free from day one, add the native test
env (`pio test -e native` + Unity) and a GH Actions job once the codec exists and has
earned it. Build the **test-case catalog from the spec, not the implementation**
(every DPT round-trip, boundary values, the DPT3 stepcode table that bit us, GA edge
cases like `0/0/0`, DPT16 truncation, checksum corruption) so tests catch a wrong
implementation instead of ratifying it.

**DPT coverage (as-built):** `KnxDpt` covers DPT1–14, 19 and **232.600** (3-byte R/G/B, added
for `KnxRGB`), each with a symmetric `KnxCodec` entry and round-trip host tests. Every v1 intent
class in §10 maps to a DPT in the enum.
