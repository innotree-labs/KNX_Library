# Strip the Doxygen theme/chrome — design

## Context

Step 4a (CI/CD pipeline, `docs/superpowers/specs/2026-07-22-ci-cd-pipeline-design.md`) shipped
publishing today's Doxygen output — Step 3's doxygen-awesome theme plus the baked-in Innotree
navbar/footer — to GitHub Pages as-is, deliberately deferring the "pure content generator"
rework PLAN.md's Step 4b already described: *"Doxygen becomes a pure content generator, not a
themed site generator... just html, the formatting should be done in Website_."*

This round implements that half of Step 4b — the theme/chrome removal — and nothing else.
`Website_`'s server-side fetch of the published content, and `Website_`'s own restyling of
Doxygen's raw output, are explicitly out of scope: the user will do that work in a separate
session against the `Website_` repo.

## Decisions (confirmed with the user this session)

1. **Strip visuals, keep structure.** Everything this repo added — doxygen-awesome's theme CSS,
   the vendored Bootstrap CSS/JS, `custom-innotree.css`, the injected Innotree navbar/footer, the
   `--chrome-offset` script — is removed. Doxygen's own native structural features (tree-view
   sidebar, search, collapsible sections) stay on, just in Doxygen's stock unstyled appearance —
   those come from Doxygen's own default CSS/JS (`doxygen.css`, `tabs.css`, `navtree.css`,
   `dynsections.js`, etc.), not from anything in `doxygen-theme/`, and were never part of what's
   being stripped.
2. **`doxygen-theme/` is deleted entirely**, not left dormant. After the Doxyfile change below,
   nothing references any file in it. Recoverable from git history if a future `Website_` session
   wants to see how Step 3 did navbar/footer injection.
3. **No `ci.yml`/`docs.yml` changes.** Both just run `doxygen Doxyfile` — neither references the
   theme internals.
4. **This won't appear on the live site until the next version tag.** `docs.yml` only
   regenerates and republishes on a `v*.*.*` push; today's `v0.1.1` output (themed) stays live on
   https://innotree-labs.github.io/KNX_Library/ until a new tag is cut. Not part of this task —
   noted so it isn't a surprise.

## Architecture

**`Doxyfile`:** delete four settings entirely (no replacement values):
- `HTML_HEADER = doxygen-theme/header.html`
- `HTML_FOOTER = doxygen-theme/footer.html`
- `HTML_EXTRA_STYLESHEET` (7-file list: both doxygen-awesome files, `custom-innotree.css`,
  `bootstrap.min.css`, `bootstrap-icons.min.css`, `site-style.css`, `site-chrome-override.css`)
- `HTML_EXTRA_FILES` (3-file list: the two Bootstrap Icons font files, `bootstrap.bundle.min.js`)

Leave untouched: `GENERATE_HTML`, `HTML_OUTPUT`, `GENERATE_TREEVIEW`, `DISABLE_INDEX`,
`FULL_SIDEBAR`, `HTML_COLORSTYLE` — Doxygen's own structural/native settings, not part of the
custom theme.

**`doxygen-theme/`:** delete the directory and everything in it — `LICENSE`,
`bootstrap-icons.min.css`, `bootstrap.bundle.min.js`, `bootstrap.min.css`, `custom-innotree.css`,
`doxygen-awesome-sidebar-only.css`, `doxygen-awesome.css`, `fonts/` (2 files),
`footer.html`, `header.html`, `site-chrome-override.css`, `site-style.css`.

## Testing / verification

- `export PATH="/opt/homebrew/bin:$PATH" && doxygen Doxyfile` — regenerate.
- `cat doxygen/warnings.txt` — must stay empty (same zero-warnings invariant as before; unrelated
  to theme, so this should be unaffected, but confirm rather than assume).
- Open `doxygen/html/index.html` locally and confirm: no Innotree navbar/footer, no green
  branding — Doxygen's stock look — with the left sidebar tree, search box, and collapsible
  sections still functioning (proves the "keep structure" decision actually held).
- Confirm `.github/workflows/ci.yml` and `.github/workflows/docs.yml` need no edits (spot-check:
  neither greps for anything under `doxygen-theme/`).

## Out of scope (explicitly, per the user this session)

- `Website_/documentation.php`'s server-side fetch against the published GitHub Pages content.
- `Website_` authoring its own stylesheet targeting Doxygen's default class names
  (`memitem`, `memdoc`, `memname`, etc.).
- Caching strategy for that fetch, the "Hardware description" content source, and what happens
  to the currently-published Step 3 output sitting in `Website_/main` — all still open questions,
  to be resolved in the future `Website_`-repo session.
