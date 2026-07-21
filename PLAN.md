# PLAN — Documentation Phase

## Context

The library itself is complete and on `main` (previous phase; its as-built record is in git
history). This phase is **documentation only** — no behaviour changes.

- **Step 1 — rewrite public API docs as user-facing reference.** DONE, committed `de253bc`.
- **Step 2 — Doxygen site.** IN PROGRESS, **not committed**.

### Decisions already settled (do not re-litigate)

- **Scope:** user-facing API + `KnxCoordinator`. Out of scope, left as maintainer docs:
  `KnxFrame`, `KnxDriver`, `KnxReassembler`, `KnxCodec`, `KnxCommon` internals.
- **Style:** reference — `@brief` on every public class/method, `@param`/`@return` where they
  carry information. No usage `@code` examples inside headers (examples live on their own page).
- **Audience: users only.** No maintainer rationale, no `PLAN.md`/`§` references in public doc
  blocks. Plain vocabulary ("true if the bus confirmed the send", not "the `L_Data.con` result").
- **Docs must match the website's design** (see palette below).

---

## Step 1 — DONE (committed `de253bc`)

All 10 in-scope targets rewritten: `Konnextor.h`, `KnxObject.h`, `KnxLighting.h`, `KnxCovers.h`,
`KnxClimate.h`, `KnxDateTime.h`, `KnxScalars.h`, `KnxValue.h`, `KnxCoordinator.h`, plus the three
user-facing enums in `KnxEnums.h` (`KnxDpt`, `DimmIncrement`, `KnxTpci`).

Notable: all **25 Arduino `String` constructors** — the ones users actually write — were
previously undocumented and now are. Firmware builds, 62/62 host tests pass.

## Step 2 — IN PROGRESS (uncommitted)

### What exists and works

- **Doxygen 1.17.0** installed via Homebrew. Binary at `/opt/homebrew/bin/doxygen` — **not on the
  default PATH**, so prefix commands with `export PATH="/opt/homebrew/bin:$PATH"`.
- **`Doxyfile`** (repo root), scoped to the public headers. Key settings and *why*:
  - `PREDEFINED = ARDUINO` — **required**, otherwise the `#ifdef ARDUINO` `String` constructors
    are invisible to Doxygen and the primary user API goes undocumented.
  - `EXCLUDE_SYMBOLS` hides the internal enums sharing `KnxEnums.h` (`KnxPriority`,
    `GroupValueType`, `KnxAddressType`, `AcknowledgeInfo`, `SwitchingBehaviour`,
    `DimmingBehaviour`).
  - `HIDE_UNDOC_MEMBERS = YES` — hides protected/internal machinery (`p_knx`, `cachedValue`,
    `onValueUpdated`, the union payload) so pages show only the documented public API.
  - `WARN_NO_PARAMDOC = NO` — the trivial `as*()` accessors and framework hooks carry a
    self-describing `@brief`; per-line `@return` would be noise.
  - `HAVE_DOT = NO`, all graphs off, `SOURCE_BROWSER = NO` — usage-focused, not architecture
    (per the Eigen reference the user gave).
- **`README.md`** — the landing page (`USE_MDFILE_AS_MAINPAGE`), H1 is **"Getting Started"**.
  Covers: what it is, hardware, install, the bus node, device objects (+ table), raw sends, debug.
- **`examples.md`** — the **Examples** page, ordered *before* Classes in the nav. Three complete
  sketches: device object (KnxLight + callback), stateless send, custom `KnxObject` (DPT7).
- **`doxygen-theme/`** — vendored **doxygen-awesome-css v2.3.4** (`doxygen-awesome.css`,
  `doxygen-awesome-sidebar-only.css`, `LICENSE`) + **`custom-innotree.css`** (brand override,
  loaded last so it wins).
- **Coverage is tool-verified: `doxygen Doxyfile` produces ZERO warnings.** This is the real
  coverage check — keep it at zero.

### Commands

```
export PATH="/opt/homebrew/bin:$PATH"
doxygen Doxyfile                                     # regenerate -> doxygen/html
cat doxygen/warnings.txt                             # must stay empty
python3 -m http.server 8080 --directory doxygen/html # then open http://localhost:8080
```

Generated `doxygen/` is gitignored. Hard-refresh the browser (Cmd+Shift+R) after regenerating —
the CSS filenames don't change, so browsers cache the old look.

### Website design source (must stay consistent)

Site lives at **`/Applications/XAMPP/xamppfiles/htdocs/Website_`** (XAMPP, served on port 80).
Brand is **Innotree**. Palette from `style/style.css`:

| Token | Value |
|---|---|
| primary green | `#066d59` |
| accent green | `#0AC49F` |
| light green | `#EEF9E7` |
| dark accent | `#089C7E` |
| text dark | `#044A3C` |
| page background | `#FAFAFA` |
| font | `'Segoe UI', Roboto, Helvetica, Arial, sans-serif` |

Code blocks on the site use a dark IDE look: bg `#171d1b` / `#121715`, keyword `#0AC49F`,
string `#A3E635`, type `#38BDF8`, func `#FACC15`, number `#FB923C`, comment `#64748B`.
`custom-innotree.css` mirrors all of this. Design language: Bootstrap 5, 8–16px radii, soft green
shadows, hover-lift.

`documentation.php` is still a **placeholder** ("coming soon") — navbar + green `small-hero`
header + footer. The generated docs are meant to fill its `<main>`.

---

## Current problems

1. **The themed output has never been looked at by anyone.** The local server was stopped before
   any visual review. So the sidebar-only layout, the green override, and the dark code blocks are
   **unverified — they are believed correct, not seen working.** This is the first thing to do.
2. **Playwright cannot run, so the assistant cannot see the rendered page.** Root cause: the
   Playwright MCP is configured with no browser argument
   (`~/.claude/plugins/marketplaces/claude-plugins-official/external_plugins/playwright/.mcp.json`
   = `{"command":"npx","args":["@playwright/mcp@latest"]}`), so it defaults to the **`chrome`
   channel** = real Google Chrome, which is **not installed**. Playwright's own **Chromium *is*
   already downloaded** (`~/Library/Caches/ms-playwright/chromium-1228`).
   - Fix: add `--browser chromium` to that `args` array. **Requires a Claude Code restart** to
     take effect, and it edits a plugin-managed file (fragile across plugin updates).
   - `npx playwright install chrome` **fails** — it needs a sudo password, which the assistant
     cannot supply.
   - Until fixed, **the user must review the docs visually** and report back.
3. **Docs are not integrated into the website yet.** They exist only as standalone HTML under
   `doxygen/html`.

## Open decisions (waiting on the user)

- **`PROJECT_NAME` is still `"Konnextor"`**, shown in the top-left header chip. Should it be
  "Innotree", a logo, or stay?
- **Code styling split:** block code is dark (matching the site's hero windows), inline `code` in
  prose is left light for readability. Confirm or make inline dark too.
- **JS extras not added:** doxygen-awesome also offers a code copy button, dark-mode toggle, and
  interactive TOC. They need a custom `header.html` (`doxygen -w html header.html footer.html
  stylesheet.css`, then inject `<script>` tags), which is why they were skipped in the first pass.
- **`KnxEnums.h` is mixed-scope** — three documented user-facing enums live alongside untouched
  internal ones. Currently handled with `EXCLUDE_SYMBOLS`; revisit if the file grows.

## Next steps

1. **Restart the server and visually review** `http://localhost:8080` (hard-refresh). Confirm the
   sidebar renders, the green is applied, code blocks are dark. Fix whatever looks wrong.
2. Resolve the open decisions above (project name, inline code, JS extras).
3. Optionally fix Playwright (`--browser chromium` + restart) so the assistant can self-verify
   rendering instead of relying on the user.
4. **Integrate into the website:** decide how the generated HTML is published into
   `documentation.php` — embed, iframe, or copy `doxygen/html` into the site tree and link it.
   The theme already matches the palette; the remaining work is the page shell (navbar/footer).
5. **Commit step 2.** Uncommitted right now: `README.md`, `examples.md`, `Doxyfile`,
   `doxygen-theme/`, and the `.gitignore` entry for `doxygen/`.

## Files touched this phase

- Committed (`de253bc`): the 10 in-scope headers + previous PLAN.md.
- Uncommitted: `README.md` (new), `examples.md` (new), `Doxyfile` (new), `doxygen-theme/` (new,
  vendored theme + brand override), `.gitignore` (+`doxygen/`), `lib/KnxValue/src/KnxValue.h`
  (struct fields given `///<` docs so Doxygen documents them).
