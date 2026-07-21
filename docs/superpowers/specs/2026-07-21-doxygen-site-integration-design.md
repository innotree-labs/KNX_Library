# Doxygen docs → Innotree site integration — design

## Context

PLAN.md Step 2 (Doxygen site) produced a themed draft that already matches the site's color
palette (green header, dark IDE-style code blocks, green section accents) via
`doxygen-theme/doxygen-awesome.css` + `custom-innotree.css`. It has never been reviewed visually
until this session, and it is not integrated into the website — it exists only as standalone
HTML under `doxygen/html`, with doxygen's own generic topbar/sidebar chrome, no Innotree navbar
or footer.

This design covers closing that integration gap: wrapping the generated docs in the site's real
navbar and footer so they read as part of innotree.at, not a bundled external tool.

Verified this session (Playwright, both against the live XAMPP site and a disposable spike
build):
- The current themed draft renders correctly and matches the palette (screenshot-compared
  against `documentation.php` and `index.php`).
- Playwright can now drive real Chrome directly (the earlier blocker in PLAN.md — no browser
  installed — is resolved; the user installed Chrome).
- **Confirmed collision, confirmed fix:** `doxygen-awesome.css` sets
  `a:link, a:visited, a:hover, a:focus, a:active { color: var(--primary-color) !important; }`
  globally. Its specificity (one type selector + one pseudo-class) beats Bootstrap's plain class
  selectors (`.navbar-brand`, `.nav-link`, even `.text-white!important`) regardless of
  stylesheet load order, because specificity is compared before `!important`-vs-`!important` source
  order matters. A naive copy-paste of the site's navbar/footer markup into a Doxygen header/footer
  template renders with invisible (green-on-green) text. A scoped override stylesheet with
  higher-specificity selectors (`.navbar.custom-navbar .nav-link { color:#fff!important }` etc.),
  loaded last, fixes it cleanly — verified working in a spike build.
- The light-green sidebar request (`--side-nav-background: #EEF9E7`) also verified in the spike:
  it drives the left nav tree, the search bar, and the right-hand per-page TOC panel
  consistently, with the same green-text-on-light-green contrast already proven on the site's own
  hero section.

## Decisions (confirmed with the user this session)

1. **Scope: full site integration, not just deeper color polish.** Navbar + footer get injected
   as real site chrome, not just closer color-matching of the standalone page.
2. **Integration mechanism: static chrome injection**, not an iframe embed and not a bare
   link-out. Doxygen's `HTML_HEADER`/`HTML_FOOTER` templates get a hardcoded copy of the site's
   rendered navbar/footer markup.
   - **Accepted tradeoff:** this copy is now duplicated outside the PHP site. If
     `components/navbar.php` or `components/footer.php` change, this copy goes stale silently —
     there is no shared templating between the static Doxygen output and the PHP site. Documented
     as a standing note in PLAN.md so it isn't forgotten.
3. **Project identity: keep "Konnextor"** in the doxygen-awesome title chip. The Innotree navbar
   above it establishes the company; the chip names the product, the way a product name sits
   under a company logo.
4. **Hero banner: landing page only.** The green "Technical Documentation" hero from
   `documentation.php` appears once, on the docs' entry page, not repeated above every generated
   class-reference page.
5. **`documentation.php` becomes a thin redirect** into the generated docs' entry page. No
   PHP-rendered placeholder step for the user to click through.
6. **Sidebar background: light green (`#EEF9E7`)**, matching the site's landing-page hero
   background — a new `--side-nav-background` override in `custom-innotree.css`. This also
   affects the search bar and the right-hand per-page TOC (both consume the same variable),
   which is consistent, not incidental.

## Architecture

**Doxygen header template (`doxygen-theme/header.html`, new — generated via
`doxygen -w html header.html footer.html`, then hand-edited):**
- After doxygen's own stylesheet `<link>` tags, add: `bootstrap.min.css`, `bootstrap-icons.min.css`,
  the site's `style.css`, and a new `site-chrome-override.css` — in that order, so
  `site-chrome-override.css` loads last and wins the specificity fight described above.
- Immediately after `<body>`, inject a static copy of `components/navbar.php`'s rendered output —
  `Dokumentation` hardcoded as the active link (no `$_SERVER`-based active-page logic needed;
  every page under this template *is* a docs page).

**Doxygen footer template (`doxygen-theme/footer.html`, new):**
- Before `</body>`, inject a static copy of `components/footer.php`'s rendered output.
- Add `bootstrap.bundle.min.js` (required for the navbar's mobile collapse toggle).

**New file — `doxygen-theme/site-chrome-override.css`:**
- Scoped, higher-specificity rules that restore Bootstrap's intended white nav/footer link color
  against doxygen-awesome's global `!important` anchor rule, e.g.:
  ```css
  .navbar.custom-navbar .navbar-brand,
  .navbar.custom-navbar .nav-link,
  .custom-footer a {
      color: #fff !important;
  }
  .navbar.custom-navbar .nav-link.active {
      color: #fff !important;
      font-weight: 600;
  }
  ```
- Verified working in the spike (see Context above).

**`custom-innotree.css` addition:**
```css
html {
    --side-nav-background: #EEF9E7;
}
```

**Landing-page hero:** a static copy of `documentation.php`'s
`<header class="py-5 small-hero">...</header>` block, injected only into the generated
`index.html` (a one-off post-generation edit or small script step, not part of the shared
header.html — it must not appear on every page).

**Doxyfile changes:**
- `HTML_HEADER = doxygen-theme/header.html`
- `HTML_FOOTER = doxygen-theme/footer.html`
- `HTML_EXTRA_STYLESHEET` gains `site-chrome-override.css` after `custom-innotree.css`.

**Output & asset layout:**
- Generated docs copied to `Website_/documentation/` (flat directory; `CREATE_SUBDIRS=NO` is
  already set, confirmed, so every generated page sits at the same depth and a single `href="…"`
  reference works uniformly — no relative-path depth calculation needed).
- `bootstrap.min.css`, `bootstrap.bundle.min.js`, `bootstrap-icons.min.css` (+ its `fonts/`
  directory), and `style.css` are copied alongside the generated HTML rather than referenced from
  the live site tree, so the docs folder is self-contained.
- `documentation.php` is replaced with a thin PHP redirect (`header('Location: documentation/index.html')` followed by `exit;`) to the docs' entry page — a real HTTP redirect, not a meta-refresh, so there's no flash of placeholder content and no arbitrary delay.

## Testing / Verification

1. `doxygen Doxyfile` regenerates cleanly; `cat doxygen/warnings.txt` stays empty (existing
   coverage gate, unaffected by this change — it only touches chrome, not doc content).
2. Playwright visual check of at least three page types after copying into
   `Website_/documentation/` and serving through XAMPP:
   - the landing page (hero + navbar + footer all present, correctly colored)
   - a class reference page (navbar/footer present, no hero, sidebar + TOC light green)
   - the Examples page
3. Browser console check for 404s on the copied Bootstrap/site assets.
4. Confirm the navbar's mobile hamburger toggle still functions (Bootstrap JS wired correctly)
   by resizing the viewport in Playwright.
5. Click through: `documentation.php` → redirects to the docs' entry page; footer's
   "Dokumentation" link and the navbar's "Dokumentation" link both resolve correctly from within
   a generated docs page (they point back at the PHP site's own pages, e.g. `blog.php`,
   `about.php#AboutUs` — confirm these resolve given the docs' new location under
   `Website_/documentation/`).

## Out of scope (deferred, not part of this change)

- Inline `code` styling (currently light, kept light for prose readability) — open decision in
  PLAN.md, untouched here.
- JS extras: dark-mode toggle, code-copy button, interactive TOC — require the same
  `header.html`/`footer.html` customization mechanism this design introduces, but are separate,
  optional follow-ups per PLAN.md.
- Any change to `KnxEnums.h` scope handling (`EXCLUDE_SYMBOLS`) — unrelated to chrome integration.
