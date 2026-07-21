# Doxygen Site Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wrap the generated Doxygen docs in the real Innotree site navbar/footer (and a
landing-page-only hero), so they read as part of innotree.at instead of a bundled external tool,
then publish them into the `Website_` site tree.

**Architecture:** Static chrome injection via Doxygen's `HTML_HEADER`/`HTML_FOOTER` templates —
hardcoded copies of the site's rendered navbar/footer markup, plus a small scoped override
stylesheet to fix a confirmed CSS collision (doxygen-awesome's global `a:link {color!important}`
rule beats Bootstrap's plain class selectors on specificity, making injected white nav text
render invisible green-on-green unless overridden).

**Tech Stack:** Doxygen 1.17.0 + doxygen-awesome-css v2.3.4 (this repo), Bootstrap 5.3.8 +
Bootstrap Icons v1.11.3 (vendored snapshot copies from the `Website_` repo), plain HTML/CSS
(no build tooling), Playwright MCP for visual verification.

## Global Constraints

- `doxygen` binary is at `/opt/homebrew/bin/doxygen`, not on default PATH — every doxygen command
  must be prefixed with `export PATH="/opt/homebrew/bin:$PATH"`.
- `OUTPUT_DIRECTORY = doxygen` and `CREATE_SUBDIRS = NO` are already set — output is a single
  flat directory, so every generated page sits at the same relative depth.
- `doxygen/warnings.txt` must stay empty after every regeneration (existing coverage gate) — this
  work only touches chrome/templates, not documented content, so it must not regress.
- Project identity in the doxygen-awesome title chip stays **"Konnextor"** — do not rename it.
- The hero banner (`Technical Documentation` + subtitle) appears **only on the docs' landing page
  (`index.html`)**, never on class-reference pages or the Examples page.
- `documentation.php` in `Website_` becomes a **real HTTP redirect**
  (`header('Location: documentation/index.html'); exit;`), not a meta-refresh.
- **`Website_` changes are made on disk and verified, but NOT committed or pushed** — it is a
  separate GitHub repo (`VinRoh07/Website_`), likely owned by a teammate. Leave the working tree
  changes there for the user to review and hand off.
- Changes inside `Softwarestack_STKNX` (this repo) **are** committed at the end of each task here,
  per this repo's established workflow (matches how Steps 1 and this design spec were committed).
- Sidebar/search-bar/TOC background: `--side-nav-background: #EEF9E7` (site's landing-hero light
  green), added to `custom-innotree.css`.
- Site-chrome override colors: white (`#fff`) nav/footer link text, matching the live site's
  `navbar-dark`/`custom-footer` styling.
- Design doc: `docs/superpowers/specs/2026-07-21-doxygen-site-integration-design.md` — refer back
  to it for the full rationale; this plan implements it task-by-task.

---

### Task 1: Vendor the site's Bootstrap/CSS/JS assets into `doxygen-theme/`

**Files:**
- Create: `doxygen-theme/bootstrap.min.css` (copy of `Website_/resources/vendor/bootstrap/css/bootstrap.min.css`, Bootstrap v5.3.8)
- Create: `doxygen-theme/bootstrap.bundle.min.js` (copy of `Website_/resources/vendor/bootstrap/js/bootstrap.bundle.min.js`)
- Create: `doxygen-theme/bootstrap-icons.min.css` (copy of `Website_/resources/vendor/bootstrap-icons/bootstrap-icons.min.css`, v1.11.3)
- Create: `doxygen-theme/fonts/bootstrap-icons.woff` and `doxygen-theme/fonts/bootstrap-icons.woff2` (copies of `Website_/resources/vendor/bootstrap-icons/fonts/*`)
- Create: `doxygen-theme/site-style.css` (copy of `Website_/style/style.css`)

**Interfaces:**
- Produces: four vendored asset files + a `fonts/` subfolder that Task 6 wires into
  `Doxyfile`'s `HTML_EXTRA_STYLESHEET` / `HTML_EXTRA_FILES`, and that Task 2/3's
  header/footer templates reference by filename.

These are one-time snapshot copies (same rationale as the already-vendored
`doxygen-awesome.css`) — if the site upgrades Bootstrap or its icon set, this copy needs manual
re-sync. No build step pulls them live from `Website_`.

- [ ] **Step 1: Copy the five assets**

```bash
SITE=/Applications/XAMPP/xamppfiles/htdocs/Website_
DEST=/Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration/doxygen-theme
mkdir -p "$DEST/fonts"
cp "$SITE/resources/vendor/bootstrap/css/bootstrap.min.css" "$DEST/bootstrap.min.css"
cp "$SITE/resources/vendor/bootstrap/js/bootstrap.bundle.min.js" "$DEST/bootstrap.bundle.min.js"
cp "$SITE/resources/vendor/bootstrap-icons/bootstrap-icons.min.css" "$DEST/bootstrap-icons.min.css"
cp "$SITE/resources/vendor/bootstrap-icons/fonts/bootstrap-icons.woff" "$DEST/fonts/bootstrap-icons.woff"
cp "$SITE/resources/vendor/bootstrap-icons/fonts/bootstrap-icons.woff2" "$DEST/fonts/bootstrap-icons.woff2"
cp "$SITE/style/style.css" "$DEST/site-style.css"
```

- [ ] **Step 2: Verify all six files landed**

Run: `ls -la /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration/doxygen-theme/ /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration/doxygen-theme/fonts/`
Expected: `bootstrap.min.css`, `bootstrap.bundle.min.js`, `bootstrap-icons.min.css`, `site-style.css`
in the theme dir; `bootstrap-icons.woff` and `bootstrap-icons.woff2` in `fonts/`.

- [ ] **Step 3: Commit**

```bash
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
git add doxygen-theme/bootstrap.min.css doxygen-theme/bootstrap.bundle.min.js \
        doxygen-theme/bootstrap-icons.min.css doxygen-theme/fonts doxygen-theme/site-style.css
git commit -m "Vendor Bootstrap 5.3.8 + Bootstrap Icons 1.11.3 + site CSS for docs chrome"
```

---

### Task 2: Write the customized Doxygen header template

**Files:**
- Create: `doxygen-theme/header.html`

**Interfaces:**
- Consumes: `doxygen-theme/bootstrap.min.css`, `doxygen-theme/bootstrap-icons.min.css`,
  `doxygen-theme/site-style.css` (Task 1), `doxygen-theme/site-chrome-override.css` (Task 4 —
  referenced here but not required to exist yet for this task's own verification, since Doxygen
  only warns, not errors, on a missing extra stylesheet; Task 4 fills it in before the first real
  regeneration in Task 7).
- Produces: the `<body>`-opening chunk every generated page shares, wrapped in
  `<!-- INNOTREE NAVBAR -->` / `<!-- END INNOTREE NAVBAR -->` comments for readability. The
  actual anchor Task 8's hero-insertion script matches on is the literal `</nav>` closing tag —
  confirmed unique per page, since doxygen's own generated markup contains no `<nav>` elements of
  its own (verified against the current `doxygen/html/index.html` before this plan's changes).

- [ ] **Step 1: Extract Doxygen's default header/footer templates**

```bash
export PATH="/opt/homebrew/bin:$PATH"
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
doxygen -w html doxygen-theme/header.html doxygen-theme/footer.html doxygen-theme/stylesheet.css.tmp
rm doxygen-theme/stylesheet.css.tmp
```

Expected: `doxygen-theme/header.html` and `doxygen-theme/footer.html` now exist, containing
Doxygen 1.17.0's default templates (with `$relpath^`, `$projectname`, `$extrastylesheet` etc.
placeholders intact).

- [ ] **Step 2: Insert the extra stylesheet links**

In `doxygen-theme/header.html`, find this existing line (Doxygen's default template puts it right
before `</head>`):

```html
$extrastylesheet
</head>
```

Replace with:

```html
$extrastylesheet
<link rel="stylesheet" href="bootstrap.min.css" />
<link rel="stylesheet" href="bootstrap-icons.min.css" />
<link rel="stylesheet" href="site-style.css" />
<link rel="stylesheet" href="site-chrome-override.css" />
</head>
```

- [ ] **Step 3: Inject the navbar right after `<body>`**

Find this existing line in `doxygen-theme/header.html`:

```html
<body>
<!--BEGIN FULL_SIDEBAR-->
```

Replace with:

```html
<body>
<!-- INNOTREE NAVBAR -->
<nav class="navbar navbar-expand-lg navbar-dark custom-navbar sticky-top">
    <div class="container">
        <a class="navbar-brand fw-bold" href="../index.php#">Innotree</a>
        <button class="navbar-toggler" type="button" data-bs-toggle="collapse" data-bs-target="#navbarNav" aria-controls="navbarNav" aria-expanded="false" aria-label="Toggle navigation">
            <span class="navbar-toggler-icon"></span>
        </button>
        <div class="collapse navbar-collapse" id="navbarNav">
            <ul class="navbar-nav ms-auto">
                <li class="nav-item"><a class="nav-link" href="../index.php">Home</a></li>
                <li class="nav-item"><a class="nav-link" href="../products.php">Products</a></li>
                <li class="nav-item"><a class="nav-link active" aria-current="page" href="index.html">Dokumentation</a></li>
                <li class="nav-item"><a class="nav-link" href="../blog.php">Blog</a></li>
                <li class="nav-item"><a class="nav-link" href="../about.php#AboutUs">About Us</a></li>
            </ul>
        </div>
    </div>
</nav>
<!-- END INNOTREE NAVBAR -->
<!--BEGIN FULL_SIDEBAR-->
```

Note: hrefs are relative to the docs' eventual home, `Website_/documentation/` (one level below
the site root) — `../index.php` etc. reach the site root; `Dokumentation` points at `index.html`
(this docs tree's own entry page, not back through `documentation.php`'s redirect) since we're
already on a docs page.

- [ ] **Step 4: Verify the file is well-formed HTML fragment-wise**

Run: `grep -c "INNOTREE NAVBAR" /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration/doxygen-theme/header.html`
Expected: `2` (the opening and closing comment markers).

- [ ] **Step 5: Commit**

```bash
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
git add doxygen-theme/header.html
git commit -m "Add customized Doxygen header template with Innotree navbar"
```

---

### Task 3: Write the customized Doxygen footer template

**Files:**
- Modify: `doxygen-theme/footer.html` (extracted in Task 2, Step 1)

**Interfaces:**
- Consumes: `doxygen-theme/bootstrap.bundle.min.js` (Task 1).
- Produces: the `</body>`-closing chunk every generated page shares.

- [ ] **Step 1: Inject the footer before `</body>`**

Find this existing line in `doxygen-theme/footer.html` (Doxygen's default template):

```html
</small></address>
</div><!-- doc-content -->
<!--END !GENERATE_TREEVIEW-->
</body>
</html>
```

Replace with:

```html
</small></address>
</div><!-- doc-content -->
<!--END !GENERATE_TREEVIEW-->
<footer class="text-center py-4 text-white custom-footer">
<div class="container">
    <div class="py-5 text-white">
        <div class="row text-start">
            <div class="col-6 col-md-2 mb-3">
                <h5>Legal</h5>
                <ul class="nav flex-column">
                    <li class="nav-item mb-2"><a href="../legal.php#imprint" class="nav-link p-0 text-white">Privacy &amp; Legal</a></li>
                    <li class="nav-item mb-2"><a href="../agb.php" class="nav-link p-0 text-white">Terms of Service (AGB)</a></li>
                    <li class="nav-item mb-2"><a href="../policy.php#shipping" class="nav-link p-0 text-white">Shipping &amp; Returns</a></li>
                    <li class="nav-item mb-2"><a href="../policy.php#accessibility" class="nav-link p-0 text-white">Accessibility</a></li>
                    <li class="nav-item mb-2"><a href="mailto:office@innotree.at" class="nav-link p-0 text-white">Contact Us</a></li>
                </ul>
            </div>
            <div class="col-6 col-md-2 mb-3">
                <h5>Information</h5>
                <ul class="nav flex-column">
                    <li class="nav-item mb-2"><a href="../about.php#AboutUs" class="nav-link p-0 text-white">About Us</a></li>
                    <li class="nav-item mb-2"><a href="../policy.php#FAQ" class="nav-link p-0 text-white">FAQ</a></li>
                    <li class="nav-item mb-2"><a href="#" class="nav-link p-0 text-white">Forum</a></li>
                    <li class="nav-item mb-2"><a href="index.html" class="nav-link p-0 text-white">Dokumentation</a></li>
                    <li class="nav-item mb-2"><a href="../blog.php" class="nav-link p-0 text-white">Blog</a></li>
                </ul>
            </div>
            <div class="col-md-5 offset-md-1 mb-3">
                <form>
                    <h5>Subscribe to our newsletter</h5>
                    <p>Monthly digest of what's new and exciting from us.</p>
                    <div class="d-flex flex-column flex-sm-row w-100 gap-2">
                        <label for="newsletter1" class="visually-hidden">Email address</label>
                        <input id="newsletter1" type="email" class="form-control footer-newsletter-input" placeholder="Email address">
                        <button class="btn custom-footer-white-button" type="button">Subscribe</button>
                    </div>
                    <div class="mt-2">
                        <small class="text-white-50">By subscribing, you agree to our <a href="../legal.php#imprint" class="text-white text-decoration-underline">Privacy Policy</a>.</small>
                    </div>
                </form>
            </div>
        </div>
        <div class="d-flex flex-column flex-sm-row justify-content-between py-4 my-4 border-top">
            <p>&copy; 2026 Innotree  All rights reserved.</p>
            <ul class="list-unstyled d-flex">
                <li class="ms-3"><a href="#" aria-label="Youtube"><i class="text-white bi bi-youtube fs-4"></i></a></li>
                <li class="ms-3"><a href="#" aria-label="linked-in"><i class="text-white bi bi-linkedin fs-4"></i></a></li>
                <li class="ms-3"><a href="#" aria-label="Instagram"><i class="text-white bi bi-instagram fs-4"></i></a></li>
                <li class="ms-3"><a href="#" aria-label="Facebook"><i class="text-white bi bi-facebook fs-4"></i></a></li>
            </ul>
        </div>
    </div>
</div>
</footer>
<script src="bootstrap.bundle.min.js"></script>
</body>
</html>
```

- [ ] **Step 2: Verify the footer marker is present**

Run: `grep -c "custom-footer" /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration/doxygen-theme/footer.html`
Expected: `1` or more.

- [ ] **Step 3: Commit**

```bash
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
git add doxygen-theme/footer.html
git commit -m "Add customized Doxygen footer template with Innotree footer"
```

---

### Task 4: Create the scoped chrome-override stylesheet

**Files:**
- Create: `doxygen-theme/site-chrome-override.css`

**Interfaces:**
- Produces: `site-chrome-override.css`, referenced by `header.html` (Task 2, Step 2) and wired
  into `HTML_EXTRA_STYLESHEET` (Task 6).

This is the fix for the confirmed collision: `doxygen-awesome.css` sets
`a:link, a:visited, a:hover, a:focus, a:active { color: var(--primary-color) !important; }`
globally. Its specificity (one type selector + one pseudo-class = 0,1,1) beats Bootstrap's plain
class selectors (`.navbar-brand` = 0,1,0; even `.text-white{color:#fff!important}` = 0,1,0) even
though both use `!important` — specificity is compared before source order breaks `!important`
ties. Verified in this session's spike: without this override, navbar/footer text renders
invisible (white-intended text landing on the same green as its background).

- [ ] **Step 1: Write the override stylesheet**

```css
/* Innotree site-chrome override for Doxygen output.
 * doxygen-awesome.css sets `a:link, a:visited, ... { color: var(--primary-color) !important; }`
 * globally. Its specificity (0,1,1) beats Bootstrap's plain class selectors on the injected
 * navbar/footer (0,1,0), even though both sides use !important. These selectors add enough
 * classes to win outright. Loaded last (after site-style.css) via HTML_EXTRA_STYLESHEET.
 */
.navbar.custom-navbar .navbar-brand,
.navbar.custom-navbar .nav-link,
.custom-footer a {
    color: #fff !important;
}

.navbar.custom-navbar .nav-link.active {
    color: #fff !important;
    font-weight: 600;
}

.custom-footer a:hover {
    color: #EEF9E7 !important;
}
```

- [ ] **Step 2: Verify the file exists and has no syntax typos**

Run: `python3 -c "import tinycss2" 2>/dev/null && python3 -c "
import tinycss2
with open('/Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration/doxygen-theme/site-chrome-override.css') as f:
    rules = tinycss2.parse_stylesheet(f.read(), skip_whitespace=True, skip_comments=True)
errors = [r for r in rules if r.type == 'error']
print('errors:', errors)
" || echo "tinycss2 not installed, skipping parse check — will be validated visually in Task 7"

Expected: `errors: []`, or the fallback message if `tinycss2` isn't available (not a blocker —
Task 7's Playwright check is the real validation).

- [ ] **Step 3: Commit**

```bash
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
git add doxygen-theme/site-chrome-override.css
git commit -m "Add scoped override fixing navbar/footer link-color collision"
```

---

### Task 5: Add the light-green sidebar variable

**Files:**
- Modify: `doxygen-theme/custom-innotree.css`

**Interfaces:**
- Produces: `--side-nav-background: #EEF9E7` — consumed by `doxygen-awesome-sidebar-only.css`'s
  existing `background: var(--side-nav-background)` rule (left sidebar), and by
  `doxygen-awesome.css`'s `--searchbar-background: var(--side-nav-background)` and
  `--toc-background: var(--side-nav-background)` (search bar + right-hand per-page TOC), so all
  three pick up the change from one variable.

- [ ] **Step 1: Add the variable to the existing `:root`/`html` block**

In `doxygen-theme/custom-innotree.css`, find:

```css
    /* Soft page background, matching the site's #FAFAFA */
    --page-background-color: #FAFAFA;
    --page-foreground-color: #044A3C;
    --separator-color:       rgba(6, 109, 89, 0.18);
```

Replace with:

```css
    /* Soft page background, matching the site's #FAFAFA */
    --page-background-color: #FAFAFA;
    --page-foreground-color: #044A3C;
    --separator-color:       rgba(6, 109, 89, 0.18);

    /* Sidebar / search bar / per-page TOC — matches the site's landing-hero light green */
    --side-nav-background:   #EEF9E7;
```

- [ ] **Step 2: Verify the variable is present**

Run: `grep -c "side-nav-background" /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration/doxygen-theme/custom-innotree.css`
Expected: `1`

- [ ] **Step 3: Commit**

```bash
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
git add doxygen-theme/custom-innotree.css
git commit -m "Set light-green sidebar background matching site landing hero"
```

---

### Task 6: Wire the new templates and assets into the Doxyfile

**Files:**
- Modify: `Doxyfile`

**Interfaces:**
- Consumes: `doxygen-theme/header.html` (Task 2), `doxygen-theme/footer.html` (Task 3),
  `doxygen-theme/site-chrome-override.css` (Task 4), `doxygen-theme/fonts/` (Task 1).
- Produces: a `Doxyfile` that, when run, generates fully-chromed HTML into `doxygen/html/`.

- [ ] **Step 1: Add `HTML_HEADER`/`HTML_FOOTER`**

Find the existing block in `Doxyfile`:

```
HTML_EXTRA_STYLESHEET  = doxygen-theme/doxygen-awesome.css \
                         doxygen-theme/doxygen-awesome-sidebar-only.css \
                         doxygen-theme/custom-innotree.css
```

Replace with:

```
HTML_HEADER            = doxygen-theme/header.html
HTML_FOOTER            = doxygen-theme/footer.html
HTML_EXTRA_STYLESHEET  = doxygen-theme/doxygen-awesome.css \
                         doxygen-theme/doxygen-awesome-sidebar-only.css \
                         doxygen-theme/custom-innotree.css \
                         doxygen-theme/bootstrap.min.css \
                         doxygen-theme/bootstrap-icons.min.css \
                         doxygen-theme/site-style.css \
                         doxygen-theme/site-chrome-override.css
HTML_EXTRA_FILES       = doxygen-theme/fonts
```

- [ ] **Step 2: Regenerate and check for new warnings**

```bash
export PATH="/opt/homebrew/bin:$PATH"
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
doxygen Doxyfile
cat doxygen/warnings.txt
```

Expected: empty output (no warnings) — the header/footer/stylesheet changes must not introduce
any new "undocumented" or template-parsing warnings.

- [ ] **Step 3: Verify the vendored assets landed in the output**

Run: `ls doxygen/html/ | grep -E "bootstrap|site-style|site-chrome"`
Expected: `bootstrap-icons.min.css`, `bootstrap.bundle.min.js`, `bootstrap.min.css`,
`site-chrome-override.css`, `site-style.css` all listed.

Run: `ls doxygen/html/fonts/`
Expected: `bootstrap-icons.woff` and `bootstrap-icons.woff2` listed.

- [ ] **Step 4: Commit**

```bash
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
git add Doxyfile
git commit -m "Wire custom header/footer templates and site assets into Doxyfile"
```

---

### Task 7: Verify the injected chrome renders correctly

**Files:**
- None created/modified — this task is verification-only, using the output from Task 6.

**Interfaces:**
- Consumes: `doxygen/html/*.html` (Task 6's regenerated output).

- [ ] **Step 1: Serve the regenerated output**

```bash
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
nohup python3 -m http.server 8080 --directory doxygen/html > /tmp/doxyserver.log 2>&1 & disown
sleep 1
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/index.html
```

Expected: `200`

- [ ] **Step 2: Navigate to the landing page and check computed styles**

Use the Playwright MCP tool `browser_navigate` to `http://localhost:8080/index.html`, then
`browser_evaluate` with this function:

```js
() => {
  const brand = document.querySelector('.navbar-brand');
  const navLink = document.querySelector('.nav-link.active');
  const footerLink = document.querySelector('.custom-footer a.nav-link');
  const sidebar = document.querySelector('#side-nav');
  return {
    brandColor: getComputedStyle(brand).color,
    navActiveColor: getComputedStyle(navLink).color,
    footerLinkColor: getComputedStyle(footerLink).color,
    sidebarBg: sidebar ? getComputedStyle(sidebar).backgroundColor : 'NOT FOUND',
  };
}
```

Expected: `brandColor` and `navActiveColor` and `footerLinkColor` are all `"rgb(255, 255, 255)"`
(not `"rgb(6, 109, 89)"` — that would mean the override didn't take effect). `sidebarBg` is
`"rgb(238, 249, 231)"` (the `#EEF9E7` light green).

- [ ] **Step 3: Check the browser console for asset errors**

Use `browser_console_messages` (or re-check the console entries returned by `browser_navigate`).
Expected: no 404s for `bootstrap.min.css`, `bootstrap-icons.min.css`, `site-style.css`,
`site-chrome-override.css`, `bootstrap.bundle.min.js`, or `fonts/bootstrap-icons.woff2`. (A 404 on
`favicon.ico` is pre-existing and harmless — ignore it.)

- [ ] **Step 4: Repeat the check on a class-reference page**

Navigate to `http://localhost:8080/class_konnextor.html`, take a full-page screenshot, and confirm
visually: navbar present and readable, footer present and readable, sidebar and right-hand TOC
panel both light green, no hero banner (this page must NOT have the hero — only `index.html`
does, and it isn't added until Task 8).

If any check fails, STOP and report which one before touching anything else (Rule 0) — do not
proceed to Task 8 with unverified chrome.

---

### Task 8: Insert the landing-page-only hero into `index.html`

**Files:**
- Modify: `doxygen/html/index.html` (generated output, not source-controlled — this step must be
  re-run after every future `doxygen Doxyfile` regeneration; document this in Task 10's PLAN.md
  update).

**Interfaces:**
- Consumes: the literal `</nav>` closing tag that Task 2's injected navbar produces in every
  generated page — used as the unique insertion anchor.

- [ ] **Step 1: Insert the hero block after the navbar, only in `index.html`**

```bash
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
python3 - <<'PYEOF'
path = "doxygen/html/index.html"
with open(path) as f:
    content = f.read()

hero = '''
<header class="py-5 small-hero">
    <div class="container text-center text-lg-start">
        <h1 class="display-4 fw-bold mb-2 small-hero-title">Technical Documentation</h1>
        <p class="lead mb-0 text-secondary">Guides, API specifications, and hardware integration manuals.</p>
    </div>
</header>
'''

marker = "</nav>"
count = content.count(marker)
assert count == 1, f"expected exactly one </nav> in index.html, found {count}"
idx = content.index(marker) + len(marker)
content = content[:idx] + hero + content[idx:]

with open(path, "w") as f:
    f.write(content)
print("hero inserted")
PYEOF
```

Expected output: `hero inserted` (the `assert` fails loudly if the navbar marker isn't found
exactly once — if it fails, STOP and check Task 2's output before re-running).

- [ ] **Step 2: Verify the hero appears only on `index.html`**

Run: `grep -l "small-hero" doxygen/html/*.html | wc -l`
Expected: `1` (only `index.html`).

- [ ] **Step 3: Visual check**

Restart the local server if needed (`http://localhost:8080/index.html`), take a full-page
screenshot with Playwright, and confirm: navbar, then the green hero
("Technical Documentation" + subtitle), then the doxygen-awesome title chip ("Konnextor — An
Arduino KNX library"), then sidebar+content, then footer — matching the "keep both" stacking
decision from the design doc.

- [ ] **Step 4: No commit for this task**

This step modifies generated output under `doxygen/` (gitignored, rebuilt from scratch by
`doxygen Doxyfile`). Nothing to commit here — the *procedure* is what Task 10 documents in
PLAN.md so it survives the next regeneration.

---

### Task 9: Publish into `Website_` and verify end-to-end (no commit)

**Files:**
- Create (in the `Website_` repo, **not committed**): `Website_/documentation/` (copy of
  `doxygen/html/`)
- Modify (in the `Website_` repo, **not committed**): `Website_/documentation.php`

**Interfaces:**
- Consumes: `doxygen/html/*` (Tasks 6–8's fully-chromed, hero-inserted output).

- [ ] **Step 1: Copy the generated docs into the site tree**

```bash
SITE=/Applications/XAMPP/xamppfiles/htdocs/Website_
rm -rf "$SITE/documentation"
mkdir -p "$SITE/documentation"
cp -r /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration/doxygen/html/. "$SITE/documentation/"
ls "$SITE/documentation" | grep -c html
```

Expected: a nonzero count of `.html` files copied.

- [ ] **Step 2: Replace `documentation.php` with a redirect**

Replace the full contents of `/Applications/XAMPP/xamppfiles/htdocs/Website_/documentation.php`
with:

```php
<?php
header("Location: documentation/index.html");
exit;
```

- [ ] **Step 3: Verify the redirect works**

```bash
curl -s -o /dev/null -w "%{http_code} -> %{redirect_url}\n" http://localhost/Website_/documentation.php
```

Expected: a `30x` status code and a `redirect_url` ending in `documentation/index.html`.

- [ ] **Step 4: Full end-to-end Playwright check**

Navigate to `http://localhost/Website_/documentation.php` and confirm (via `browser_navigate`,
which follows the redirect) that the final URL is the docs' `index.html`, with the navbar, hero,
doxygen chip, sidebar, and footer all present — this is the real site, real port-80 XAMPP
serving, not the port-8080 throwaway server from Task 7.

Then check the reverse link: from within a docs page, click (or navigate directly to) the
"Dokumentation" nav link — confirm it stays on `index.html` (no redirect loop, no 404). Click
"Home" in the navbar — confirm it resolves to `http://localhost/Website_/index.php` (not a 404
from a wrong relative path).

- [ ] **Step 5: Mobile navbar toggle check**

Use `browser_resize` to set the viewport to `375x667` (mobile width), take a screenshot, confirm
the hamburger toggle button is visible and the nav links are collapsed. Click the toggle (via
`browser_click`), take another screenshot, confirm the nav links expand — this exercises
`bootstrap.bundle.min.js`, wired in Task 3.

- [ ] **Step 6: Report status, do not commit**

State explicitly to the user: "`Website_/documentation.php` and `Website_/documentation/` are
updated on disk and verified working, but not committed — that repo is git-tracked separately
under `VinRoh07/Website_`. Review with `cd /Applications/XAMPP/xamppfiles/htdocs/Website_ && git
status` / `git diff` before committing or pushing there."

---

### Task 10: Document the workflow in PLAN.md and commit

**Files:**
- Modify: `PLAN.md`

**Interfaces:**
- None — this is documentation only.

- [ ] **Step 1: Update PLAN.md's "Current problems" and "Next steps" sections**

Replace the three "Current problems" bullets and the "Next steps" numbered list with a summary
reflecting what this plan completed: navbar/footer/hero chrome injected and verified, sidebar
recolored, docs published into `Website_/documentation/` (uncommitted there, pending teammate
review). Add a new subsection documenting the **repeatable publish procedure** so the next
regeneration doesn't require re-deriving it:

```markdown
### Publishing a docs update

After editing any header/source doc comment:

1. `export PATH="/opt/homebrew/bin:$PATH" && doxygen Doxyfile` — regenerate.
2. `cat doxygen/warnings.txt` — must stay empty.
3. Re-run the hero-insertion script (Task 8, Step 1 of the integration plan) against the fresh
   `doxygen/html/index.html` — **this step does not survive regeneration and must be repeated
   every time.**
4. `cp -r doxygen/html/. /Applications/XAMPP/xamppfiles/htdocs/Website_/documentation/` to publish.
5. Visually spot-check via Playwright before telling anyone the docs are updated.

**Drift risk, noted deliberately:** the navbar/footer/hero markup baked into
`doxygen-theme/header.html`/`footer.html` and the vendored Bootstrap/site CSS in
`doxygen-theme/` are static snapshots of `Website_/components/navbar.php`,
`components/footer.php`, and `style/style.css`. If the site's real nav, footer, or Bootstrap
version changes, these copies go stale silently — there is no shared templating between the
static Doxygen output and the PHP site. Re-sync manually (repeat Tasks 1–5's file edits) when
that happens.
```

- [ ] **Step 2: Verify the new content is present**

Run: `grep -c "Publishing a docs update" /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration/PLAN.md`
Expected: `1`

- [ ] **Step 3: Commit**

```bash
cd /Users/florian/Coding/Softwarestack_STKNX/.worktrees/doxygen-site-integration
git add PLAN.md
git commit -m "Document the Doxygen site-integration workflow and publish procedure"
```
