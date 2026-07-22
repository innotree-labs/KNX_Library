# CI/CD pipeline: tests + Doxygen → GitHub Pages — design

## Context

PLAN.md Step 4 ("PLANNED, NOT STARTED") bundles two things: a CI/CD pipeline that publishes
Doxygen output to GitHub Pages on a version bump, and `Website_`'s `documentation.php` fetching
that published content at request time (replacing Step 3's manual `cp -r` into `Website_`). This
round covers only the first half — the pipeline. `Website_`'s runtime fetch, and the accompanying
"Doxygen becomes a pure content generator, no theme CSS" rework PLAN.md describes, are deliberately
deferred (see Future work).

Confirmed this session:
- **`innotree-labs/KNX_Library` is public** (`private: false` via the GitHub API) — GitHub Pages
  via `actions/deploy-pages` works without a paid plan. This was an open question in PLAN.md;
  it's now resolved.
- **No CI exists today** — no `.github/workflows/`.
- **No single version number exists today** — 7 parallel `lib/*/library.json` files, each with an
  independent `"version": "0.1.0"` field. PlatformIO requires a literal version string per
  library (no cross-file templating), which is why they're duplicated; the dependencies between
  them use `"*"` wildcards, so the values aren't actually load-bearing for the build — just
  informational, and currently unenforced.

## Decisions (confirmed with the user this session)

1. **Scope: pipeline only.** GitHub Pages will serve today's Doxygen output as-is — the
   doxygen-awesome theme plus the static Website_ navbar/footer baked into
   `doxygen-theme/header.html`/`footer.html` (Step 3's mechanism). This carries Step 3's
   already-documented drift risk (the baked navbar/footer copy can go stale against
   `Website_/components/navbar.php`/`footer.php`) onto a second, now-public URL. Not a new
   problem — just more visible. Resolved deliberately, not by oversight: the "pure content
   generator" rework is real, separable work, tracked as Future work below.
2. **Version single source of truth: new root `VERSION` file**, synced into the 7
   `library.json` files by a script, not hand-edited in 7 places.
3. **CD trigger: git tag push (`v*.*.*`)**, not a push-to-main diff. Explicit — the person
   cutting a release chooses the moment, matching PLAN.md's original idea.
4. **CI runs both** the native host test suite and a full firmware build, on every push and PR
   (not gated behind version bumps — this is baseline health, separate from the docs release).
5. **Doxygen version in CI is pinned to 1.17.0** (direct binary download), matching what's
   documented as the local toolchain in PLAN.md Step 2, rather than trusting `apt`'s likely-older
   packaged version — protects the documented zero-warnings invariant from silently drifting
   because of a toolchain version difference.
6. **The zero-warnings invariant becomes a CI gate**, not just a documented manual check: the
   docs job fails if `doxygen/warnings.txt` is non-empty.
7. **Enabling GitHub Pages (Settings → Source → GitHub Actions) is a one-time manual step**,
   done by the user — not automatable without a repo-admin token this session doesn't have.

## Architecture

### 1. Version single source of truth

**New root file `VERSION`** — plain text, single line, e.g. `0.2.0`. The only place a human edits
the version.

**New `scripts/bump_version.py <new_version>`:**
- Validates `new_version` looks like `X.Y.Z` (semver-shaped; no pre-release/build metadata
  handling needed — nothing here consumes those).
- Writes `VERSION`.
- Rewrites the `"version"` field in each of the 7 `lib/*/library.json` files in place (via
  Python's `json` module, preserving key order and 2-space indent to match existing formatting).
- Prints the next manual steps (`git add`, `git commit`, `git tag vX.Y.Z`, `git push --tags`) but
  does **not** run them — committing and especially pushing a tag are the actual release
  trigger (public Pages deploy), and stay an explicit, separate human action.

### 2. CI workflow — `.github/workflows/ci.yml`

Triggers: `push` (any branch), `pull_request`.

Two independent jobs, no dependency between them:

- **`native-tests`**: `actions/checkout` → `actions/setup-python` → `pip install platformio` →
  `pio test -e native`. This is the 62-test host Unity suite over the Arduino-free layers.
- **`firmware-build`**: same setup → `pio run -e seeed_xiao_esp32c6`. Compiles only — no
  `--target upload`, no hardware attached to the runner.

Both cache `~/.platformio` via `actions/cache`, keyed on `hashFiles('platformio.ini')`, so the
Espressif toolchain download (the slow part) is skipped on cache hit.

### 3. CD workflow — `.github/workflows/docs.yml`

Trigger: `push` to tags matching `v*.*.*` only.

Permissions: `contents: read`, `pages: write`, `id-token: write`. Environment: `github-pages`.

Jobs, in dependency order:

1. **`verify-version`** — checkout, then a small shell/Python step: strip the leading `v` from
   `github.ref_name`, compare it against `VERSION`'s contents and against the `"version"` field
   read out of all 7 `lib/*/library.json` files. Fails the workflow immediately (no tests run, no
   docs published) if any of the 8 values disagree — this is what makes the single-source-of-truth
   decision actually enforced, not just documented convention.
2. **`test`** (needs: `verify-version`) — `pio test -e native`. Re-run here rather than trusted
   from `ci.yml`: a tag can point at any commit, including one `ci.yml` never ran against for
   this exact ref, so the docs pipeline verifies its own commit rather than assuming a prior run
   covered it.
3. **`build`** (needs: `verify-version`) — `pio run -e seeed_xiao_esp32c6`, same reasoning as
   above.
4. **`docs`** (needs: `test`, `build`):
   - Download and extract the pinned Doxygen 1.17.0 Linux binary release directly (not `apt`).
   - `doxygen Doxyfile`.
   - Fail the job if `doxygen/warnings.txt` is non-empty (`test -s doxygen/warnings.txt && exit 1`
     — the file existing empty is fine, the file having content is the failure).
   - `actions/upload-pages-artifact` with `path: doxygen/html`.
   - `actions/deploy-pages` to publish.

### One-time manual setup (not part of this implementation)

Repo Settings → Pages → Source must be switched to "GitHub Actions" before the first `docs.yml`
run can succeed (currently `has_pages: false`, no Pages source configured). This needs
repo-admin access in the GitHub UI — flagged here so it isn't a surprise first-run failure.

## Testing / verification

- `scripts/bump_version.py`: run locally against a scratch copy of the 7 `library.json` files
  (or via `git stash` after a real run), confirm all 7 end up with the identical new version
  string and valid JSON (no trailing-comma / formatting breakage).
- `ci.yml`: push a throwaway branch, confirm both jobs run and go green on the current `main`
  (which already passes 62/62 native tests and builds firmware today).
- `docs.yml`: after the manual Pages-source step, push a test tag (e.g. `v0.1.1` — bump script
  run first so `VERSION`/`library.json` agree with the tag), confirm `verify-version` passes,
  `test`/`build` pass, `docs` publishes, and the resulting GitHub Pages URL renders the same
  content as today's local `doxygen/html` (spot-check, not full Playwright parity — the
  chrome/theme is unchanged from what Step 3 already verified).
- Confirm the **failure path**: hand-edit one `library.json`'s version away from `VERSION` on a
  throwaway branch, push a matching tag, confirm `verify-version` fails the workflow before any
  Doxygen run or Pages deploy happens.

## Future work (explicitly out of scope this round)

- `Website_/documentation.php` server-side fetch (PHP `file_get_contents`/curl) against the
  published GitHub Pages URL, replacing Step 3's manual `cp -r`. Needs a caching strategy
  (APCu / file cache with TTL / conditional GET) — not yet decided.
- The "pure content generator" rework: dropping doxygen-awesome's theme CSS and the
  navbar/footer injection templates entirely, so Doxygen emits bare content fragments that
  `Website_`'s own stylesheet styles directly (targeting Doxygen's known class names —
  `memitem`, `memdoc`, `memname`, etc.). This is *why* the current round's GitHub Pages output
  still carries the baked-in Website_ chrome — that duplication only goes away once this lands.
- "Hardware description" content section (source TBD — new `.md` file fed into Doxygen's
  `INPUT`, or hand-authored in `Website_` directly).
- What happens to the currently-published Step 3 output sitting in `Website_/main`
  (commit `f153b37`) once the runtime-fetch path lands.
