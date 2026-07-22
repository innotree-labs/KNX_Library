# CI/CD Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `VERSION`-driven single source of truth for the library's version, a CI workflow that runs the native test suite and firmware build on every push/PR, and a CD workflow that publishes Doxygen output to GitHub Pages on a `v*.*.*` tag push, gated on tests passing and all version files agreeing.

**Architecture:** Two independent pieces. (1) A root `VERSION` file plus `scripts/bump_version.py`, which syncs that value into the 7 `lib/*/library.json` files — no CI dependency, pure local tooling. (2) A `.github/actions/setup-pio` composite action (checkout is caller's responsibility) shared by two workflows: `ci.yml` (push/PR — native tests + firmware build) and `docs.yml` (tag push only — version-agreement check, then re-run tests/build, then Doxygen + `actions/deploy-pages`). The composite action exists so the PlatformIO setup+cache steps are written once, not duplicated across the 4 places they're used (`ci.yml`'s 2 jobs, `docs.yml`'s 2 jobs).

**Tech Stack:** GitHub Actions (`actions/checkout@v4`, `actions/setup-python@v5`, `actions/cache@v4`, `actions/upload-pages-artifact@v3`, `actions/deploy-pages@v4`), PlatformIO CLI, Python 3 (`bump_version.py`, `json` stdlib only — no new dependency), Doxygen 1.17.0 (pinned binary, not `apt`).

## Global Constraints

- Version strings are `X.Y.Z` (no pre-release/build metadata) — matches the existing `"0.1.0"` values in all 7 `lib/*/library.json` files.
- `bump_version.py` only edits files locally — it must never run `git commit`/`tag`/`push`. That stays a manual step (design doc §1, decision 2).
- `docs.yml` triggers **only** on tags matching `v*.*.*` — never on push-to-main (design doc, decision 3).
- Doxygen in CI must be **1.17.0**, downloaded directly — not installed via `apt` (design doc, decision 5). Confirmed asset name this session: `doxygen-1.17.0.linux.bin.tar.gz` at `https://github.com/doxygen/doxygen/releases/download/Release_1_17_0/doxygen-1.17.0.linux.bin.tar.gz`.
- The CD `docs` job must fail if `doxygen/warnings.txt` is non-empty (design doc, decision 6) — this is a hard gate, not a warning.
- **No task in this plan pushes a git tag or triggers a live GitHub Pages deploy.** Enabling Pages (Settings → Source → GitHub Actions) is a manual step for the user; the first real tag push is theirs to make after reviewing this work (advisor guidance this session — the live deploy is the actual release action).
- `library.json` files must round-trip through `json.dumps(..., indent=2)` with **no unrelated formatting drift** — same key order, same 2-space indent, trailing newline — since a noisy reformat on every bump is exactly the drift smell this feature exists to kill.

---

## Task 1: Version single source of truth

**Files:**
- Create: `VERSION`
- Create: `scripts/bump_version.py`
- Modify (via running the script, not by hand): `lib/KnxDriver/library.json`, `lib/KnxCommon/library.json`, `lib/KnxCoordinator/library.json`, `lib/KnxObject/library.json`, `lib/Konnextor/library.json`, `lib/KnxTelegram/library.json`, `lib/KnxValue/library.json`

**Interfaces:**
- Produces: a root file `VERSION` containing a single line `X.Y.Z\n`, read by `docs.yml`'s `verify-version` job in Task 3 as `cat VERSION`.
- Produces: `scripts/bump_version.py <new_version>` — CLI script, no importable functions needed by any other task.

This task has no dependency on Task 2/3 and can be built and verified standalone.

- [ ] **Step 1: Create the VERSION file**

Write `/Users/florian/Coding/Softwarestack_STKNX/VERSION` with exactly:

```
0.1.0
```

(matches the current value already in all 7 `library.json` files — this task establishes the single source of truth without changing the actual version number).

- [ ] **Step 2: Write `scripts/bump_version.py`**

```python
#!/usr/bin/env python3
"""Sync the project version into VERSION and every lib/*/library.json file."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = ROOT / "VERSION"
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")


def main() -> None:
    if len(sys.argv) != 2:
        print("usage: bump_version.py <new_version>", file=sys.stderr)
        sys.exit(1)

    new_version = sys.argv[1]
    if not VERSION_RE.match(new_version):
        print(f"error: '{new_version}' is not of the form X.Y.Z", file=sys.stderr)
        sys.exit(1)

    library_jsons = sorted(ROOT.glob("lib/*/library.json"))
    if not library_jsons:
        print("error: no lib/*/library.json files found", file=sys.stderr)
        sys.exit(1)

    VERSION_FILE.write_text(new_version + "\n")
    print(f"wrote VERSION ({new_version})")

    for path in library_jsons:
        data = json.loads(path.read_text())
        data["version"] = new_version
        path.write_text(json.dumps(data, indent=2) + "\n")
        print(f"updated {path.relative_to(ROOT)}")

    rel_paths = " ".join(str(p.relative_to(ROOT)) for p in library_jsons)
    print(f"\nVersion set to {new_version}. Next steps (not run automatically):")
    print(f"  git add VERSION {rel_paths}")
    print(f"  git commit -m 'Bump version to {new_version}'")
    print(f"  git tag v{new_version}")
    print("  git push --tags")


if __name__ == "__main__":
    main()
```

Make it executable:

```bash
chmod +x scripts/bump_version.py
```

- [ ] **Step 3: Verify round-trip fidelity (no version change)**

Run the script with the version that's already current, and confirm it produces **zero diff** on the 7 `library.json` files (proves `json.dumps(indent=2)` doesn't silently reformat key order/spacing beyond the version field):

```bash
python3 scripts/bump_version.py 0.1.0
git diff --stat
```

Expected: `git diff --stat` shows only `VERSION` as new (untracked, won't appear in `diff --stat` — check with `git status` instead) and **no changes** to any `lib/*/library.json` (they already say `0.1.0`, and the rewrite must be byte-for-byte identical to what's there now). If any `library.json` shows a diff, compare it to the original — if it's just whitespace/key-order, the script has a bug; if it's a genuine format mismatch (e.g., original wasn't 2-space indented), note the actual formatting in this task before proceeding.

- [ ] **Step 4: Verify an actual bump works end-to-end**

```bash
python3 scripts/bump_version.py 0.1.1
cat VERSION
grep '"version"' lib/*/library.json
```

Expected: `VERSION` contains `0.1.1`, all 7 `grep` lines show `"version": "0.1.1",`.

- [ ] **Step 5: Revert the test bump, keep VERSION at 0.1.0**

```bash
python3 scripts/bump_version.py 0.1.0
git status
```

Expected: `git status` shows `VERSION` as a new untracked file and `scripts/bump_version.py` as new; no `lib/*/library.json` files listed as modified (Step 3/4/5 round-tripped back to the committed value).

- [ ] **Step 6: Commit**

```bash
git add VERSION scripts/bump_version.py
git commit -m "Add VERSION as single source of truth, synced via bump_version.py"
```

---

## Task 2: Shared PlatformIO setup + CI workflow

**Files:**
- Create: `.github/actions/setup-pio/action.yml`
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: a composite action at path `./.github/actions/setup-pio`, invoked as `uses: ./.github/actions/setup-pio` by any workflow job that has already run `actions/checkout@v4`. It installs PlatformIO (`pip install -U platformio`) with `~/.platformio` cached by `hashFiles('platformio.ini')`. **Task 3's `docs.yml` reuses this exact action** — its `test`/`build` jobs must call it the same way.

No dependency on Task 1; can run in parallel with it. Task 3 depends on this task's composite action existing.

- [ ] **Step 1: Write the composite action**

Create `.github/actions/setup-pio/action.yml`:

```yaml
name: Set up PlatformIO
description: >
  Installs PlatformIO with a cached ~/.platformio. Assumes actions/checkout
  has already run in the calling job.
runs:
  using: composite
  steps:
    - uses: actions/setup-python@v5
      with:
        python-version: '3.x'

    - uses: actions/cache@v4
      with:
        path: ~/.platformio
        key: ${{ runner.os }}-platformio-${{ hashFiles('platformio.ini') }}
        restore-keys: |
          ${{ runner.os }}-platformio-

    - run: pip install -U platformio
      shell: bash
```

- [ ] **Step 2: Write the CI workflow**

Create `.github/workflows/ci.yml`:

```yaml
name: CI

on:
  push:
  pull_request:

jobs:
  native-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ./.github/actions/setup-pio
      - run: pio test -e native

  firmware-build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ./.github/actions/setup-pio
      - run: pio run -e seeed_xiao_esp32c6
```

- [ ] **Step 3: Validate YAML syntax locally**

```bash
python3 -c "import yaml, sys; yaml.safe_load(open('.github/actions/setup-pio/action.yml')); yaml.safe_load(open('.github/workflows/ci.yml')); print('valid')"
```

Expected: `valid`. (If `pyyaml` isn't installed, `pip install --user pyyaml` first — this is a one-off local syntax check, not a project dependency.)

- [ ] **Step 4: Commit**

```bash
git add .github/actions/setup-pio/action.yml .github/workflows/ci.yml
git commit -m "Add CI workflow: native tests + firmware build on every push/PR"
```

- [ ] **Step 5: Push and confirm CI goes green**

```bash
git push
```

Then check the Actions tab (or `git log -1 --format=%H` and look up the run for that SHA) for the `native-tests` and `firmware-build` jobs. Expected: both green — `main` already passes 62/62 native tests and builds the firmware today (per PLAN.md), so this is confirming the workflow itself is correct, not fixing product code.

---

## Task 3: CD workflow — version verification + Doxygen publish to GitHub Pages

**Files:**
- Create: `.github/workflows/docs.yml`

**Interfaces:**
- Consumes: `VERSION` file format from Task 1 (`cat VERSION` → bare `X.Y.Z`, no `v` prefix, trailing newline stripped by shell `$()`); `.github/actions/setup-pio` composite action from Task 2, invoked identically (`uses: ./.github/actions/setup-pio` after `actions/checkout@v4`).
- Produces: nothing consumed by other tasks — this is the terminal workflow.

Depends on Task 1 (for `VERSION` to exist and be meaningful) and Task 2 (for the composite action to exist) being committed first.

- [ ] **Step 1: Write the CD workflow**

Create `.github/workflows/docs.yml`:

```yaml
name: Publish docs

on:
  push:
    tags:
      - 'v*.*.*'

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: pages
  cancel-in-progress: false

jobs:
  verify-version:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check tag, VERSION, and every library.json agree
        run: |
          set -euo pipefail
          TAG_VERSION="${GITHUB_REF_NAME#v}"
          FILE_VERSION="$(cat VERSION)"

          if [ "$TAG_VERSION" != "$FILE_VERSION" ]; then
            echo "::error::tag v$TAG_VERSION does not match VERSION file ($FILE_VERSION)"
            exit 1
          fi

          for f in lib/*/library.json; do
            LIB_VERSION="$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['version'])" "$f")"
            if [ "$LIB_VERSION" != "$FILE_VERSION" ]; then
              echo "::error::$f version ($LIB_VERSION) does not match VERSION file ($FILE_VERSION)"
              exit 1
            fi
          done

          echo "All versions agree: $FILE_VERSION"

  test:
    needs: verify-version
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ./.github/actions/setup-pio
      - run: pio test -e native

  build:
    needs: verify-version
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ./.github/actions/setup-pio
      - run: pio run -e seeed_xiao_esp32c6

  docs:
    needs: [test, build]
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - uses: actions/checkout@v4

      - name: Install pinned Doxygen 1.17.0
        run: |
          set -euo pipefail
          curl -fsSL -o doxygen.tar.gz \
            https://github.com/doxygen/doxygen/releases/download/Release_1_17_0/doxygen-1.17.0.linux.bin.tar.gz
          tar xzf doxygen.tar.gz
          DOXYGEN_BIN="$(find "$PWD" -maxdepth 3 -type f -name doxygen -perm -u+x | head -n1)"
          if [ -z "$DOXYGEN_BIN" ]; then
            echo "::error::doxygen binary not found after extracting archive"
            exit 1
          fi
          dirname "$DOXYGEN_BIN" >> "$GITHUB_PATH"

      - name: Generate docs
        run: doxygen Doxyfile

      - name: Fail if Doxygen emitted warnings
        run: |
          if [ -s doxygen/warnings.txt ]; then
            echo "::error::doxygen/warnings.txt is non-empty:"
            cat doxygen/warnings.txt
            exit 1
          fi
          echo "warnings.txt is empty, as required"

      - uses: actions/upload-pages-artifact@v3
        with:
          path: doxygen/html

      - uses: actions/deploy-pages@v4
        id: deployment
```

- [ ] **Step 2: Validate YAML syntax locally**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/docs.yml')); print('valid')"
```

Expected: `valid`.

- [ ] **Step 3: Verify the version-check logic locally (without pushing a tag)**

Simulate the `verify-version` step's shell logic directly, using the checked-in `VERSION` (`0.1.0`) as if it were tag `v0.1.0`:

```bash
GITHUB_REF_NAME=v0.1.0 bash -c '
set -euo pipefail
TAG_VERSION="${GITHUB_REF_NAME#v}"
FILE_VERSION="$(cat VERSION)"
if [ "$TAG_VERSION" != "$FILE_VERSION" ]; then
  echo "MISMATCH: tag=$TAG_VERSION file=$FILE_VERSION"; exit 1
fi
for f in lib/*/library.json; do
  LIB_VERSION="$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))[\"version\"])" "$f")"
  if [ "$LIB_VERSION" != "$FILE_VERSION" ]; then
    echo "MISMATCH: $f=$LIB_VERSION file=$FILE_VERSION"; exit 1
  fi
done
echo "All versions agree: $FILE_VERSION"
'
```

Expected: `All versions agree: 0.1.0`.

- [ ] **Step 4: Verify the failure path**

Temporarily break one `library.json` and confirm the same logic catches it:

```bash
cp lib/Konnextor/library.json /tmp/konnextor-library.json.bak
python3 -c "
import json
p = 'lib/Konnextor/library.json'
d = json.load(open(p))
d['version'] = '9.9.9'
json.dump(d, open(p, 'w'), indent=2)
"
GITHUB_REF_NAME=v0.1.0 bash -c '
set -euo pipefail
TAG_VERSION="${GITHUB_REF_NAME#v}"
FILE_VERSION="$(cat VERSION)"
for f in lib/*/library.json; do
  LIB_VERSION="$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))[\"version\"])" "$f")"
  if [ "$LIB_VERSION" != "$FILE_VERSION" ]; then
    echo "MISMATCH: $f=$LIB_VERSION file=$FILE_VERSION"; exit 1
  fi
done
echo "SHOULD NOT REACH HERE"
' ; echo "exit code: $?"
cp /tmp/konnextor-library.json.bak lib/Konnextor/library.json
git diff --stat
```

Expected: the block prints `MISMATCH: lib/Konnextor/library.json=9.9.9 file=0.1.0` and `exit code: 1`; after restoring, `git diff --stat` shows no changes (confirms the restore was clean).

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/docs.yml
git commit -m "Add CD workflow: version-gated Doxygen publish to GitHub Pages"
```

**Do not push a tag as part of this task.** The first real `v*.*.*` push — which triggers a live, public GitHub Pages deploy — is the user's call, and only works after they've manually switched Settings → Pages → Source to "GitHub Actions" (currently unset). Report this as the explicit next step when Task 3 is done; do not do it automatically.

---

## Self-Review Notes

- **Spec coverage:** design doc §1 (VERSION SoT) → Task 1. §2 (CI workflow) → Task 2. §3 (CD workflow, verify-version, pinned Doxygen, warnings gate, Pages deploy) → Task 3. The "one-time manual Pages setup" and "Future work" sections are explicitly not implementation tasks — called out as such in Task 3's closing note and left to the user.
- **Placeholder scan:** no TBD/TODO; every step has complete, runnable code or commands.
- **Type/interface consistency:** `VERSION`'s format (bare `X.Y.Z`, trailing newline) is defined once in Task 1 Step 1 and consumed identically in Task 3 Step 1 (`cat VERSION`) and Step 3/4 (local simulation). The composite action's invocation (`uses: ./.github/actions/setup-pio` after `actions/checkout@v4`) is defined in Task 2 Step 1 and reused verbatim in Task 3 Step 1's `test`/`build` jobs — no drift between the two workflows' setup steps.
