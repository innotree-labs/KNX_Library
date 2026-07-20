# PLAN — KNX Library Redesign (as-built record)

**Status: implemented, shipped, and basic-bench-tested** on branch `knx-redesign-common-value`.
This is a *completed-phase* record, not an active plan — it keeps the **decisions and their
rationale**, because *why* a choice was made outlives the commit that made it. The architecture
and file layout live in `CLAUDE.md` and the code; they are not repeated here. The full
proposal-era version of this file is in git history (pre-`a74454c`).

The next phase gets its own fresh PLAN.md.

**Verify the tree** (`pio` is not on `PATH`):

```
~/.platformio/penv/bin/pio run             # firmware — Seeed XIAO ESP32-C6
~/.platformio/penv/bin/pio test -e native  # 62/62 host Unity tests
```

---

## What this redesign delivered

An Adafruit-style KNX library: **breakout board + simple Arduino library**, no ETS, group
addresses hardcoded in the sketch, intent (not DPT) as the user vocabulary. A sketch needs one
include and one line: `#include <Konnextor.h>` / `Konnextor knx("1.1.5");`.

Three send tiers over **one encode path** and **one value currency**:

| Tier | Example | DPT visibility | Receive? |
|---|---|---|---|
| Intent object | `KnxLight k(knx, cmdGa, statusGa);` | hidden | yes (owns GA+DPT+cache) |
| Raw object | `KnxObject o(knx, ga, KnxDpt::DPT9);` | given once | yes |
| Stateless send | `knx.send("0/4/2", Dpt9(21.5f));` | given per call | no — runtime GA only |

## Decisions (settled forks — the load-bearing "why")

1. **Value objects, not a `sendDPTn(...)` family.** `Dpt1(true)`/`Dpt9(21.5f)` carry their own
   DPT + correctly-typed payload, so a payload/DPT mismatch is a **compile error** — the failure
   class that produced the original `DimmIncrement` bug. Deleted the unsafe
   `sendTelegram(ga, dpt, void*)`.
2. **`KnxValue` internals → tagged union** (`{ KnxDpt dpt; union payload; }`, ~10–12 B, DPT19 is
   the largest v1 payload). One codec does union↔wire-bytes; typed named access avoids the
   hand-marshalling that caused the `DimmIncrement` bug.
3. **Callbacks are plain function pointers** (`void(*)(...)`). **No `std::function` / capturing
   lambdas** — they heap-allocate and bloat on AVR. Non-capturing lambdas are fine (they convert
   to a function pointer). Examples teach **named free functions** so `setup()` reads as a flat
   wiring manifest, matching the `attachInterrupt(pin, handler)` idiom.
4. **Intent objects combine command + status GA**, status defaulting to command when omitted.
   `.toggle()` flips relative to the **status-fed cached value**, so it tracks real bus state —
   this killed the per-class `lastToggleState` bookkeeping.
5. **Receive: an object owns its GA + DPT, so it decodes without the user restating either.**
   Fixes the old asymmetry where `bind(GA, DPT, cb)` leaked the DPT while sending hid it.
6. **Registry → intrusive linked list of `IKnxReceiver`**, no `MAX_OBJECTS`. Memory scales with
   declared objects; the destructor unlinks so a scoped object can't dangle. Objects must be
   declared at global/member scope (raw pointers in the list).
7. **No retry logic in the library.** The ATTiny (TP-UART-like) owns bus repetition; the library
   parses the real `L_Data.con` and surfaces success/failure only — never a hard-coded `true`.
8. **Intent class set v1:** `KnxLight`/`KnxDimmLight`/`KnxBlind` (DPT1/3), `KnxTemperature`/
   `KnxHumidity` (DPT9), `KnxTime`/`KnxDate`/`KnxDateTime` (DPT10/11/19), `KnxRGB` (DPT232.600),
   `KnxPercent` (DPT5.001), `KnxChar` (DPT4), `KnxFloat` (DPT14). `KnxCodec` covers DPT1–14, 19,
   232 symmetrically, with round-trip host tests.

## Deliberate exceptions & waivers (Chesterton's-fence notes)

- **Debug mode is one library-wide `bool`** (`KnxCommon/src/KnxDebug.h`) — an **agreed exception
  to the no-globals rule**. It is a log level, not a service locator: the stateless `KnxFrame`/
  `KnxCodec` namespaces have no `this` to hang a per-instance flag on, and the alternative was
  threading a sink through every framing/codec signature. `knx.enableDebugMode(bool)`, runtime,
  no recompile. Costs ~2.4 KB flash / 0 RAM disabled. Details in `CLAUDE.md`.
- **`DimmIncrement` renumbering changes runtime behaviour** vs the thesis (e.g. `Percent_6` now
  emits stepcode 5 ≈ 6.25 %, was 2 ≈ 50 %). The new values are the real DPT3.007 stepcodes
  (`Stop`=0 … `Percent_1_5`=7); intended, spec-verified, round-trip tested.
- **Host-testability got promoted from "optional" to load-bearing.** `KnxCommon`/`KnxValue`/
  `KnxTelegram`/`KnxCoordinator` are Arduino-free; the host suite drove the whole redesign and
  keeps the Arduino-only `Konnextor` facade out of a native build. CI wiring is still not set up
  — the suite runs locally.
- **§ "Rebuild the Device layer" was WAIVED**, not built. `src/main.cpp` demonstrates the library
  directly; the thesis button state machines were demoted to `examples/KNX_Device/` — not in the
  build, and they **do not compile against the current API** (they call deleted methods). Kept as
  history only.
- **Dropped the `#ifdef _MSC_VER` / `#pragma region` folding blocks** — MSVC-only clutter in
  headers users read to learn the API. Intentionally overrides the personal `style_cpp.md`
  convention for this public artifact.

## Spec conformance pass (against KNX Standard v3.0.0, in `docs/`)

Reading `03_02_02 TP1` §2.2, `03_03_04 Transport Layer` §2 and the `TP-UART2` datasheet against
the framing layer produced three fixes and two deferrals.

- **Fixed — frame sync accepted 1 of 8 legal control bytes.** `KnxReassembler` exact-matched
  `0xBC`. An L_Data_Standard control field is `1 0 r 1 p1 p0 0 0` (TP1 Fig 28), so bit 5 (repeat)
  and bits 3..2 (priority) vary → eight legal values. The old code dropped **every repeated frame**
  (the retransmission after a lost original) and everything above low priority. Now masked
  `(ctrl & 0xD3) == 0x90`; `MIN_FRAME` 9 → 8 (a length-0 `T_Connect` is legal per §2.2.4.5). The
  TP-UART2 datasheet writes the same pattern `10R1cc00`, a second source confirming the mask.
- **Fixed — `KnxPriority` names were shifted by one.** Spec bits 3..2: system `00`, normal `01`,
  urgent `10`, low `11`. Renamed to `System/Normal/Urgent/Low`. **Wire output unchanged** — the
  `build` default moved from old `Normal` (0x0C) to new `Low` (0x0C), same byte, control still
  `0xBC`.
- **Fixed — individual (point-to-point) addressing was unimplemented.** `parse()` ignored octet 5
  bit 7 (AT) and unpacked every destination as a group address, so a telegram addressed to *this
  device* (descriptor-read ping, transport connect, ETS programming) was misparsed. See structure
  below.
- **Deferred — L_Data_Extended frames (frame type bit 7 = 0).** For APDUs > 15 octets and LTE-HEE
  addressing; a group-comms library never needs one. The mask rejects them cleanly. Revisit only
  for a DPT beyond 14 octets or LTE mode.
- **Deferred — transport connection state machine** (`03_03_04` §5: connect/ack/seqno/timeout).
  The framing/routing below makes point-to-point telegrams parseable, routable and sendable —
  enough to ping. *Holding* an ETS connection needs §5, a separate piece of work.
- **Closed — no link-layer ACK generation is correct.** The TP-UART2 datasheet §3.2.3.1.7 settles
  it: `U_SetAddress` (done in `begin()`) activates on-chip address evaluation and the TP-UART emits
  the IACK itself; `U_AckInformation` is only for a host that wants to filter addresses (within
  1.7 ms), which we don't. The declared `U_ACK_INFO_*` constants stay for that future. Side effect:
  the chip ACKs *every* group telegram on the bus, foreign traffic included — inherent to the mode.
- **Also verified while reading:** the `L_Data.con` byte constants (`CON_MASK 0x7F` / `CON_PATTERN
  0x0B` / `CON_POSITIVE 0x80`) match the datasheet's `x0001011`, `x=1` positive, and the bench
  `0x8B`. These were flagged "spec-derived, unverified" — now verified. Checksum is NOT-XOR
  (TP1 Fig 31); `LG = APDU octets − 1`; DPT 5.001 scaling matches the spec table; Session and
  Presentation layers are *empty* in KNX, so having no L5/L6 is conformance, not a gap.

### Point-to-point addressing — structure (built without disturbing the group path)

- **`ParsedTelegram` grew, it did not change** — group fields keep names/positions, so every
  existing consumer is untouched. Added: `addressType`, `individualTarget`, `tpci`,
  `sequenceNumber`, `apci`, `hasApci`.
- **`IKnxReceiver` left alone.** `KnxCommon` gained a separate `IKnxDeviceHandler`; the coordinator
  holds **one optional pointer** to it — not a registry, because a point-to-point telegram has
  exactly one legitimate recipient (this device), unlike a group address that fans out.
- **Routing branches in `loop()` on `addressType`.** Load-bearing: the address spaces overlap
  numerically (1.1.5 packs to the same `uint16_t` as group 2/1/5), so *type*, not value, keeps a
  point-to-point telegram out of the group registry. There is a test for exactly that.
- **Framing gained `buildIndividual()` (T_Data_Individual + raw APCI) and `buildControl()`**
  (TPCI-only Connect/Disconnect/ACK/NAK), sharing one header writer so the three builders can't
  drift. APCI codes come from `03_03_07` §3 (e.g. `A_DeviceDescriptor_Read` = 0x300); `apci` is
  exposed raw rather than enumerated — device management isn't the library's focus.

## Verification status

Everything above the driver is **host-tested** (62/62). The transmit path, receive path, and the
mask/priority/addressing changes were exercised on the bench for the basic DPT1/DPT5 flow — this
is the branch's basis for promotion to `main`. Still **not** bench-verified: repeated-frame
acceptance on a busy bus (fix 1's real payoff), a colliding-frame TX (send to a GA whose frame
byte is `0x8B`), and a real device ping. Those are the follow-ups for the next hardware session.
