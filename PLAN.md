# PLAN — Documentation Phase

## Context

The library is functionally complete and merged to `main` (previous phase; that as-built record
is in git history). This phase is **documentation, not features** — no behaviour changes.

Today the public headers carry Doxygen-flavoured JSDoc (`@brief`/`@param`/`@return`), but the
content is mixed: it explains the design to a *maintainer* (Chesterton's-fence rationale,
layering internals, "PLAN §N" cross-references, LDF notes) as much as it guides a *user*. Several
of the methods a real sketch calls — notably the Arduino `String` constructors like
`KnxLight lamp(knx, "0/1/1", "0/3/0")` — have **no documentation at all**, while lower-level
overloads users never touch are annotated.

Goal: clean, user-facing **reference** documentation on the public API, Doxygen-ready so a future
website (step 2) renders directly from the headers.

### Decisions (settled with the user this session)

- **Scope: user-facing API + `KnxCoordinator`.** In scope: `Konnextor`, the intent classes, the
  raw `KnxObject` tier, the `KnxValue`/`Dpt*` factories, and `KnxCoordinator` (the advanced
  bring-your-own-driver path). **Out of scope, left as maintainer docs:** `KnxFrame`,
  `KnxDriver`, `KnxReassembler`, `KnxCodec` internals, and the `KnxCommon` internals.
- **Style: reference.** A one-line `@brief` on every public class and every public method, with
  `@param`/`@return` where they carry information. **No usage `@code` examples** (those belong on
  the website, step 2).
- **Audience: users only.** Strip maintainer rationale out of the public doc blocks. **No
  `PLAN.md` or internal-design references anywhere in the docs.** User-facing vocabulary — say
  "true if the bus confirmed the send," not "the `L_Data.con` result (PLAN §9)."

## Steps

### Step 1 — Rewrite public API docs (this step)

Files in scope (rewrite the file-level `@details` to a user summary, then every public member):

1. `Konnextor.h` — the entry point
2. `KnxObject.h` — raw tier + the inherited `write/value/onUpdate` surface
3. `KnxLighting.h` — `KnxLight`, `KnxDimmLight`, `KnxRGB`
4. `KnxCovers.h` — `KnxBlind`
5. `KnxClimate.h` — `KnxTemperature`, `KnxHumidity`
6. `KnxDateTime.h` — `KnxTime`, `KnxDate`, `KnxDateTime`
7. `KnxScalars.h` — `KnxPercent`, `KnxChar`, `KnxFloat`
8. `KnxValue.h` — the `Dpt1(..)..Dpt232(..)` factories users pass to `send()`/`write()`
9. `KnxCoordinator.h` — `send`/`loop`/`begin`/`reset`/`enableDebugMode` + the advanced ctor
10. The user-facing enums in `KnxEnums.h` that appear in the above signatures
    (`KnxDpt`, `KnxPriority`, `DimmIncrement`), documented where a user selects a value.

Per file:
- **Document the constructors a user calls** — including the `#ifdef ARDUINO` `String` ctors,
  which are currently blank and are the primary user entry.
- Rewrite each `@brief` to say what the caller *gets*; `@param` says what to pass; `@return` says
  what comes back and what a failure looks like, in plain terms.
- Framework hooks that are public but user-never-calls (`matches`/`receive` from `IKnxReceiver`)
  get a one-line `@brief` stating they are called by the coordinator, not the sketch.
- Keep the `@brief`/`@param`/`@return` tag structure intact so step 2 reads it directly.
- **Do not touch:** the out-of-scope headers, any `.cpp` implementation comments, or `examples/`.

Order: pilot `Konnextor.h` + `KnxLighting.h` first → confirm tone → roll out the remaining files.

### Step 2 — Doxygen (later, separate session)

- Add a `Doxyfile`; generate HTML from the reference docs above.
- Configure it to document only the in-scope public surface (exclude / `@cond` the lower layers).
- Evaluate embedding the generated site into the project website.
- Not started this session.

## Verification

- **Compiles unchanged:** `~/.platformio/penv/bin/pio run` and `pio test -e native` stay green —
  docs are comments, so this only guards against an accidental code edit.
- **Coverage read:** every in-scope public class and method has a `@brief`; every parameter and
  return is described; the Arduino `String` ctors are documented.
- **Cleanliness read:** no maintainer rationale, no `PLAN.md`/`§` references, no layering
  internals in any in-scope doc block.
- **(Step 2)** `doxygen` runs with no "undocumented public member" warning for the in-scope set.
