# PLAN — Documentation Phase

## Context

The library itself is complete and on `main` (previous phase; its as-built record is in git
history). This phase is **documentation only** — no behaviour changes.

- **Step 1 — rewrite public API docs as user-facing reference.** DONE, committed `de253bc`.
- **Step 2 — Doxygen site.** DONE, committed `23215ad`.
- **Step 3 — wrap the site in the real Innotree navbar/footer chrome and publish it.** DONE,
  committed (see the site-integration commits following `23215ad`) — **but see Step 4: this
  mechanism (static chrome duplicated into the generated pages, manually copied into `Website_`)
  is planned to be superseded, not just extended.** Step 3's own commits and the pages it
  produced are not being reverted — they're what's live today — but new work should follow
  Step 4's direction, not Step 3's.
- **Step 4a — CI/CD pipeline: tests on every push, Doxygen → GitHub Pages on a version tag.**
  DONE. Live at **https://innotree-labs.github.io/KNX_Library/**. See below.
- **Step 4b — `Website_` fetches published docs at request time; Doxygen becomes a pure content
  generator (theme/chrome stripped).** PLANNED, NOT STARTED — deliberately deferred out of 4a.
  See below.

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

## Step 2 — DONE (committed `23215ad`)

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

`documentation.php` **was** a placeholder ("coming soon") — navbar + green `small-hero` header +
footer — when Step 2 finished; Step 3 below replaced it with a real redirect into the published
docs.

---

## Step 3 — DONE: site-chrome integration (tracked in the separate integration plan)

Full implementation plan and rationale:
`docs/superpowers/plans/2026-07-21-doxygen-site-integration.md` (design doc:
`docs/superpowers/specs/2026-07-21-doxygen-site-integration-design.md`). Summary of what it
built, task by task:

- **Real Innotree navbar/footer injected**, not a bundled-tool look. Done via Doxygen's
  `HTML_HEADER`/`HTML_FOOTER` templates (`doxygen-theme/header.html`, `doxygen-theme/footer.html`)
  — hardcoded copies of the site's rendered navbar/footer markup — plus vendored Bootstrap
  5.3.8 + Bootstrap Icons 1.11.3 + the site's own stylesheet (`doxygen-theme/bootstrap.min.css`,
  `bootstrap.bundle.min.js`, `bootstrap-icons.min.css` + `fonts/`, `site-style.css`).
- **A confirmed CSS collision fixed** with a scoped override, `doxygen-theme/site-chrome-
  override.css`: doxygen-awesome's global `a:link/:visited/:hover/:focus/:active { color:
  var(--primary-color) !important }` outranks Bootstrap's plain class selectors on specificity
  even though both sides use `!important`, so injected white nav/footer text rendered invisible
  (white-on-green) without this override.
- **Sidebar/search bar/TOC recolored** to the site's light green, `--side-nav-background:
  #EEF9E7` in `custom-innotree.css`, picked up by doxygen-awesome's existing variable
  references for all three surfaces.
- **A landing-page-only hero banner** ("Technical Documentation" + subtitle), inserted after the
  navbar only in `index.html` — see "Publishing a docs update" below, this step is NOT permanent.
- **A real layout bug was found and fixed** (not anticipated going in): doxygen-awesome's
  sidebar-only theme hardcodes `--top-height: 120px` on the assumption that `#top` (doxygen's own
  title/search bar) starts at viewport y=0. The injected navbar — and, on the landing page, the
  hero above it — pushes `#top` down, so the sidebar and right-hand TOC rendered too high,
  visibly overlapping the navbar (worst on the landing page, where the hero adds more offset).
  Fixed with a small script in `header.html` that measures the navbar's (and, if present, the
  hero's) live rendered height on each page load/resize and stores it in a CSS variable,
  `--chrome-offset`; `custom-innotree.css` adds that to the sidebar's `top`/`height` calc
  alongside the original `--top-height`. **This fix is permanent** — it's committed source
  (`header.html`, `custom-innotree.css`), applied automatically on every regeneration, unlike the
  hero insertion below.
- **A related concern was investigated and deliberately left alone**: `#side-nav` uses
  `position: absolute`, so scrolling the *outer* page far enough (past all doc content, to the
  injected footer) would scroll the sidebar away with it. Verified this does NOT happen during
  normal reading — `#doc-content` and `#nav-tree` both scroll internally (`overflow-y: auto`)
  with far more scrollable height than the viewport, so the outer page itself never needs to
  scroll under normal use. Switching the sidebar to `position: fixed` was considered and
  **rejected**: it would pin the sidebar in place permanently, making the footer unreachable —
  there would be no scroll position at which a fixed-position sidebar stops covering it. This is
  a **known, deliberate, accepted limitation, not a bug** — do not attempt to "fix" it again
  without re-deriving why `position: fixed` doesn't work here.
- **Verification, done via Playwright against the real XAMPP-served site** (not just the
  throwaway `python3 -m http.server`): `doxygen/warnings.txt` stayed empty through all chrome
  changes; navbar/footer/hero/sidebar computed colors all matched expected values; browser
  console clean (no asset 404s, no JS errors); mobile navbar hamburger toggle opens/closes
  correctly (exercises `bootstrap.bundle.min.js`); every nav link (Home, Dokumentation, footer
  links) resolves correctly with no redirect loop back through `documentation.php`.
- **Published into `Website_/documentation/`, but not committed there.** `Website_` is a separate
  GitHub repo (`VinRoh07/Website_`), likely owned by a teammate.
  `Website_/documentation.php` is now a real HTTP redirect (`header("Location:
  documentation/index.html"); exit;`, confirmed via `curl` → `302`), and `Website_/documentation/`
  holds a full copy of `doxygen/html/`. Both exist on disk and are verified working end-to-end,
  but are deliberately left as **uncommitted working-tree changes** in that repo for its owner to
  review — do not assume this is deployed/committed there just because it works locally.

### Publishing a docs update

After editing any header/source doc comment:

1. `export PATH="/opt/homebrew/bin:$PATH" && doxygen Doxyfile` — regenerate.
2. `cat doxygen/warnings.txt` — must stay empty.
3. Re-run the hero-insertion script (integration plan, Task 8 Step 1) against the fresh
   `doxygen/html/index.html` — it matches on the literal `</nav>` closing tag from the injected
   navbar (unique per page) and asserts it appears exactly once before inserting the hero
   `<header class="py-5 small-hero">…</header>` block right after it. **This step does not
   survive regeneration and must be repeated every time** — `doxygen/` is fully rebuilt from
   scratch and is gitignored, so nothing about the hero persists between runs. Contrast with the
   `--chrome-offset` sidebar fix above, which lives in committed source and needs no re-application.
4. `cp -r doxygen/html/. /Applications/XAMPP/xamppfiles/htdocs/Website_/documentation/` to publish.
5. Visually spot-check via Playwright (navbar, hero on the landing page only, sidebar/TOC color,
   footer, no console errors) before telling anyone the docs are updated.

**Drift risk, noted deliberately:** the navbar/footer markup baked into
`doxygen-theme/header.html`/`footer.html` and the vendored Bootstrap/site CSS in
`doxygen-theme/` (`bootstrap.min.css`, `bootstrap.bundle.min.js`, `bootstrap-icons.min.css`,
`site-style.css`) are static snapshots of `Website_/components/navbar.php`,
`components/footer.php`, and `style/style.css`. If the site's real nav, footer, or Bootstrap
version ever changes, these copies go stale silently — there is no shared templating between the
static Doxygen output and the live PHP site. Re-sync manually (repeat the integration plan's
Tasks 1–5 file edits against the current site source) when that happens.

## Step 4a — DONE: CI/CD pipeline (tests + Doxygen → GitHub Pages)

Design doc: `docs/superpowers/specs/2026-07-22-ci-cd-pipeline-design.md`. Implementation plan:
`docs/superpowers/plans/2026-07-22-ci-cd-pipeline.md`. **Scoped deliberately to the pipeline
only** — publishes today's Step 3 output (doxygen-awesome theme + baked Website_ chrome) as-is;
the "pure content generator" rework and `Website_`'s runtime fetch are Step 4b, still not
started.

**Live site: https://innotree-labs.github.io/KNX_Library/**

### What exists and works

- **`VERSION`** (repo root) — single source of truth for the library's version, replacing 7
  independently-hand-edited `lib/*/library.json` version fields.
- **`scripts/bump_version.py <X.Y.Z>`** — writes `VERSION`, syncs the same value into all 7
  `library.json` files, prints the follow-up `git` commands but never runs them itself (commit/
  tag/push stay an explicit human step — pushing the tag is the live-deploy trigger). Uses
  `json.dumps(..., ensure_ascii=False)` — without it, the `§`/`—` characters in `KnxObject`'s and
  `Konnextor`'s `description` fields get corrupted into `\uXXXX` escapes on every bump; caught in
  review before it shipped.
- **`.github/actions/setup-pio/action.yml`** — composite action (setup-python + cache
  `~/.platformio` keyed on `platformio.ini` + `pip install platformio`), shared by every job in
  both workflows below so the setup steps are written once, not duplicated 4×.
- **`.github/workflows/ci.yml`** — triggers on `pull_request` and `push` to **branches only**
  (`branches: ['**']` — deliberately excludes tag refs; see the "why CI doesn't also run on the
  tag" note below). Two jobs: `native-tests` (`pio test -e native`) and `firmware-build`
  (`pio run -e seeed_xiao_esp32c6`).
- **`.github/workflows/docs.yml`** — triggers only on `v*.*.*` tag pushes. Jobs in order:
  1. `verify-version` — strips the `v` from the tag, compares it against `VERSION` and all 7
     `library.json` files; fails immediately (no tests, no docs, no deploy) on any mismatch.
  2. `test` / `build` (both need `verify-version`) — re-run the native suite and firmware build
     against this exact tagged commit. Deliberately not trusted from `ci.yml`: a tag can point at
     an old commit `ci.yml` never ran against, so the release pipeline verifies its own SHA.
  3. `docs` (needs both) — downloads a **pinned Doxygen 1.17.0** binary (not `apt`'s likely-older
     package), runs `doxygen Doxyfile`, **fails the job if `doxygen/warnings.txt` is non-empty**
     (the zero-warnings invariant is now a real CI gate, not just a documented manual check),
     then `actions/upload-pages-artifact` + `actions/deploy-pages`.

### Why CI doesn't also run on the tag

Originally `ci.yml` triggered on any `push` (no ref filter), so pushing a tag fired *both*
`ci.yml` and `docs.yml`, running the native suite and firmware build twice for the same commit —
observed directly on the `v0.1.1` release (two concurrent "CI"/"Publish docs" runs). Fixed by
scoping `ci.yml`'s push trigger to `branches: ['**']`, which doesn't match tag refs. `docs.yml`
still re-verifies independently (see above) — that's a real safety property for arbitrary/old
tagged commits, not redundant with `ci.yml` in the general case, just redundant in the common
one (tag pushed right after a normal commit).

### A real gotcha hit on the first release, worth remembering

The first `v0.1.1` tag push failed at the `docs` job with **"Tag 'v0.1.1' is not allowed to
deploy to github-pages due to environment protection rules."** Enabling Pages (Settings → Pages
→ Source → GitHub Actions) auto-creates a `github-pages` deployment environment whose branch/tag
policy, by default, does not include tag refs — only branches. Fix: Settings → Environments →
`github-pages` → **Deployment branches and tags** → add a rule with Ref type **Tag**, pattern
`v*`. One-time, already done for this repo. Re-running the failed `docs` job afterward (no need
to re-tag) completed successfully.

### Release process

```bash
python3 scripts/bump_version.py 0.1.2        # picks the next version
git diff                                      # sanity-check: only version fields changed
git add VERSION lib/*/library.json
git commit -m "Bump version to 0.1.2"
git tag v0.1.2
git push && git push --tags                   # the tag push is what fires docs.yml
```

Watch the "Publish docs" run in the Actions tab — `verify-version` → `test`/`build` → `docs`.

## Step 4b — PLANNED, NOT STARTED: `Website_` runtime fetch + Doxygen theme rework

**Why this still needs to happen:** Step 4a intentionally publishes today's themed output (Step
3's doxygen-awesome CSS + baked-in Website_ navbar/footer) to GitHub Pages as-is. That carries
Step 3's already-documented drift risk (the baked chrome copy can go stale against
`Website_/components/navbar.php`/`footer.php`) onto a second, now-public URL — not a new problem,
just more visible. `Website_/documentation.php` also still points at the Step 3
manually-`cp -r`'d copy, not the new Pages URL.

**Direction agreed with the user (conversation, not yet written as a formal design doc/plan):**

- **Doxygen becomes a pure content generator, not a themed site generator.** No navbar, no
  footer, and — this is the bigger shift — **no doxygen-awesome theme CSS either**. The user's
  words: "just html, the formatting should be done in Website_." `Website_`'s own stylesheet
  should style Doxygen's raw output tags directly, not sit alongside a second competing theme.
  This means most of what Step 3 built becomes dead weight for this direction: `doxygen-theme/
  header.html`'s navbar injection, `footer.html`, `site-chrome-override.css` (the whole reason
  it exists — a color collision between doxygen-awesome and an injected Bootstrap navbar — stops
  applying once there's no injected navbar in the same document), and probably the
  `--chrome-offset` sidebar-position fix and doxygen-awesome-sidebar-only.css's fixed/absolute
  positioning model generally (that whole model assumes a full-viewport standalone app-shell
  page, which no longer applies once Doxygen's output is a fragment embedded inside a normal
  scrolling page). Confirm exactly what gets deleted vs. kept when this is actually planned in
  detail — don't assume the whole `doxygen-theme/` directory goes away without checking what,
  if anything, is still needed structurally (e.g. does Doxygen still need *some* HTML_HEADER/
  FOOTER override just to strip the `<html>/<head>/<body>` wrapper down to a bare fragment?).
- **Runtime, in `Website_`:** `documentation.php` does a **server-side fetch** (PHP
  `file_get_contents`/curl, not client-side JS, not a build-time copy) against the GitHub Pages
  URL (now live: `https://innotree-labs.github.io/KNX_Library/`) for the current content
  fragment, and echoes it into `<main>` below the hero — navbar, footer, and hero stay pure
  Website_ PHP, same as every other page on the site. Chosen over two alternatives: CI pushing
  fragment files into `Website_` at build time (rejected: reintroduces a cross-repo write, even
  if smaller than Step 3's), and client-side JS fetch+inject (rejected: content invisible until
  JS runs, worse for SEO/no-JS).
- **Content scope, decided:** README (Getting Started) + Examples + a **new "Hardware
  description" section** (not yet written — source content/authoring TBD) + **one combined
  "API Reference" page** covering all classes (explicitly NOT per-class routing/URLs — the user
  chose this over a full router when asked, to keep `Website_`'s side simple "for now").

**Open questions, not yet decided — resolve these before writing a real implementation plan:**

- Caching strategy for the PHP fetch — hitting GitHub on every single page view is fragile/slow;
  needs *some* cache layer (APCu, a file cache with a TTL, or HTTP conditional GET), but which
  one wasn't discussed.
- How much of doxygen-awesome (if any) survives — fully dropped in favor of raw output + a fresh
  `Website_`-authored stylesheet targeting Doxygen's known class names (`memitem`, `memdoc`,
  `memname`, etc.), or kept minimally for structural things (collapsible member lists, search)
  with only the *visual* theme stripped? This determines the Doxyfile config and how much of
  `doxygen-theme/` gets deleted vs. adapted.
- Where does "Hardware description" content actually come from — a new markdown file fed into
  Doxygen's `INPUT`/mainpage mechanism (like `README.md`/`examples.md` today), or hand-authored
  directly in `Website_`? (If it's meant to come from the library repo like everything else in
  this pipeline, it should probably be a new `.md` file alongside `README.md`.)
- What happens to the currently-published Step 3 output already sitting in `Website_/main`
  (commit `f153b37`) once Step 4b lands — replaced outright, or does `documentation.php` need a
  transition/fallback path?

**Not started.** No code, no Doxyfile changes, no `Website_` PHP changes exist yet for this. Next
session should brainstorm this properly (`superpowers:brainstorming`) given the number of open
questions above, then write a design doc + implementation plan the same way Step 3 and 4a did,
before touching any code.

## Open decisions (waiting on the user)

- **`PROJECT_NAME` is still `"Konnextor"`**, shown in the doxygen-awesome title chip. Deliberately
  kept — the integration plan states explicitly that project identity does not get renamed.
- **Code styling split:** block code is dark (matching the site's hero windows), inline `code` in
  prose is left light for readability. Confirm or make inline dark too.
- **JS extras beyond the chrome-offset script:** doxygen-awesome also offers a code copy button,
  dark-mode toggle, and interactive TOC. Not added — still skipped, no plan to add them yet.
  **Moot if Step 4 drops doxygen-awesome's theme entirely — re-check relevance before touching.**
- **`KnxEnums.h` is mixed-scope** — three documented user-facing enums live alongside untouched
  internal ones. Currently handled with `EXCLUDE_SYMBOLS`; revisit if the file grows.

## Files touched this phase

- Committed (`de253bc`): the 10 in-scope headers + previous PLAN.md.
- Committed (`23215ad` and the site-integration commits that follow it): `README.md`,
  `examples.md`, `Doxyfile`, `doxygen-theme/` (vendored doxygen-awesome + brand override + vendored
  Bootstrap/site assets + header/footer/chrome-override templates), `.gitignore` (+`doxygen/`),
  `lib/KnxValue/src/KnxValue.h` (struct fields given `///<` docs).
- Committed (Step 4a, `3cf6149`..`e1531b2`): `docs/superpowers/specs/2026-07-22-ci-cd-pipeline-
  design.md`, `docs/superpowers/plans/2026-07-22-ci-cd-pipeline.md`, `VERSION`,
  `scripts/bump_version.py`, `.github/actions/setup-pio/action.yml`, `.github/workflows/ci.yml`,
  `.github/workflows/docs.yml`, all 7 `lib/*/library.json` (version bumped to `0.1.1` as the
  first release through the new pipeline).
- Not committed in this repo — nothing; not committed in `Website_` (separate repo, by design):
  `Website_/documentation.php` (redirect) and `Website_/documentation/` (copied doc output).
