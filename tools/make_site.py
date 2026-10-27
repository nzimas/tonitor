#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# TONITOR: the website's generated parts, in docs/ (GitHub Pages):
#   docs/index.html, docs/install.html   site/home.html, site/install.html in the common frame
#   docs/manual.html     the README's manual (from "# Manual" to "# Building from source")
#   docs/changelog.html  CHANGELOG.md
#   docs/firmware/       the package the browser installer writes, and latest.json (its file, version, SHA-256)
#   tools/make_site.py [PACKAGE.fwsc]        (default build/tonitor.fwsc; run tools/build.py first)
# The markdown is the little the README uses: headings, paragraphs, lists, tables, quotes, code, emphasis, links.
import hashlib
import html
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
REPO = "https://github.com/nzimas/tonitor"
# the stylesheet's address changes with it, so browsers never keep an old one
CSS_V = hashlib.sha256((Path(__file__).resolve().parent.parent / "docs/css/site.css").read_bytes()).hexdigest()[:10]
MANUAL_JS_V = hashlib.sha256((Path(__file__).resolve().parent.parent / "docs/js/manual.js").read_bytes()).hexdigest()[:10]


def slug(text):
    s = re.sub(r"<[^>]+>", "", text).lower()
    s = re.sub(r"[^\w\s-]", "", s, flags=re.U)
    return re.sub(r"\s+", "-", s.strip())


def inline(t):
    codes = []

    def keep(m):
        codes.append(f"<code>{html.escape(m[1])}</code>")
        return f"\0{len(codes) - 1}\0"
    t = re.sub(r"`([^`]+)`", keep, t)
    if not re.search(r"<[a-z/][^>]*>", t):
        t = html.escape(t, quote=False)

    def link(m):
        url = m[2]
        if not re.match(r"[a-z]+:|#", url):        # a file in the repository
            url = f"{REPO}/blob/main/{url}"
        return f'<a href="{html.escape(url)}">{m[1]}</a>'
    t = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", link, t)
    t = re.sub(r"<(https?://[^>]+)>", r'<a href="\1">\1</a>', t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"(?<![\w*])\*([^*\s][^*]*?)\*(?![\w*])", r"<i>\1</i>", t)
    return re.sub(r"\0(\d+)\0", lambda m: codes[int(m[1])], t)


def to_html(md, toc):
    out, lines, i = [], md.splitlines(), 0

    def para(buf):
        if buf:
            out.append("<p>" + inline(" ".join(buf)) + "</p>")
            buf.clear()
    buf = []
    while i < len(lines):
        ln = lines[i]
        if not ln.strip():
            para(buf)
            i += 1
        elif ln.startswith("```"):
            para(buf)
            j = i + 1
            while j < len(lines) and not lines[j].startswith("```"):
                j += 1
            out.append("<pre><code>" + html.escape("\n".join(lines[i + 1:j])) + "</code></pre>")
            i = j + 1
        elif m := re.match(r"(#{1,4})\s+(.*)", ln):
            para(buf)
            lv, text = len(m[1]), m[2]
            sid = slug(text)
            out.append(f'<h{lv} id="{sid}">{inline(text)}</h{lv}>')
            if lv in (2, 3):
                toc.append((lv, sid, re.sub(r"<[^>]+>", "", inline(text))))
            i += 1
        elif ln.startswith("|"):
            para(buf)
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                cells = [c.strip() for c in re.split(r"(?<!\\)\|", lines[i].strip())[1:-1]]
                if not all(re.fullmatch(r":?-+:?", c) for c in cells):
                    rows.append(cells)
                i += 1
            t = ["<table>", "<tr>" + "".join(f"<th>{inline(c)}</th>" for c in rows[0]) + "</tr>"]
            t += ["<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>" for r in rows[1:]]
            out.append("\n".join(t + ["</table>"]))
        elif ln.startswith(">"):
            para(buf)
            q = []
            while i < len(lines) and lines[i].startswith(">"):
                q.append(lines[i][1:].strip())
                i += 1
            q = [x for x in q if not re.fullmatch(r"\[!\w+\]", x)]
            out.append("<blockquote><p>" + inline(" ".join(q)) + "</p></blockquote>")
        elif re.match(r"\s*[-*] ", ln):
            para(buf)
            items, depth0 = [], len(ln) - len(ln.lstrip())
            while i < len(lines) and (re.match(r"\s*[-*] ", lines[i]) or (lines[i].startswith("  ") and lines[i].strip())):
                d = len(lines[i]) - len(lines[i].lstrip())
                if re.match(r"\s*[-*] ", lines[i]):
                    items.append([d > depth0, lines[i].strip()[2:]])
                else:
                    items[-1][1] += " " + lines[i].strip()
                i += 1
            h, open_sub = ["<ul>"], False
            for sub, text in items:
                if sub and not open_sub:
                    h[-1] = h[-1].removesuffix("</li>")
                    h.append("<ul>")
                    open_sub = True
                elif not sub and open_sub:
                    h.append("</ul></li>")
                    open_sub = False
                h.append(f"<li>{inline(text)}</li>")
            if open_sub:
                h.append("</ul></li>")
            out.append("\n".join(h + ["</ul>"]))
        elif ln.lstrip().startswith("<"):
            para(buf)
            out.append(ln)
            i += 1
        else:
            buf.append(ln.strip())
            i += 1
    para(buf)
    return "\n".join(out)


def page(title, current, body, desc, head=""):
    """every page: the menu down the left (above the content on a narrow screen), the content, the footer"""
    nav = [("./", "Home"), ("install.html", "Install"), ("manual.html", "Manual"), ("changelog.html", "Changelog"), ("backup.html", "Backup"), ("projects.html", "Other projects"),
           (REPO, "GitHub")]
    links = "\n      ".join(f'<a href="{h}"' + (' aria-current="page"' if n == current else "") + f">{n}</a>"
                           for h, n in nav)
    return f"""<!doctype html>
<!-- SPDX-License-Identifier: MIT -- Tonitor website (generated by tools/make_site.py from site/, README.md, CHANGELOG.md) -->
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<meta name="description" content="{desc}">
<link rel="icon" href="assets/mark.svg" type="image/svg+xml">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Atkinson+Hyperlegible:wght@400;700&display=swap" rel="stylesheet">
<link rel="stylesheet" href="css/site.css?v={CSS_V}">{head}
</head>
<body>
<div class="layout">
  <aside class="side">
    <div class="top">
      <a class="brand" href="./"><img src="assets/mark.svg" alt="">TONITOR</a>
      <button class="menu-btn" type="button" aria-label="Menu" aria-controls="site-nav" aria-expanded="false"><span></span><span></span><span></span></button>
    </div>
    <div class="layers" aria-hidden="true"><span></span><span></span><span></span><span></span><span></span><span></span></div>
    <nav id="site-nav" aria-label="Site">
      <button class="menu-close" type="button" aria-label="Close the menu">&times;</button>
      {links}
    </nav>
    <div class="menu-shade" hidden></div>
    <form class="donate donate-side" action="https://www.paypal.com/cgi-bin/webscr" method="post" target="_top">
      <input type="hidden" name="cmd" value="_xclick">
      <input type="hidden" name="business" value="nzimas.lv@gmail.com">
      <input type="hidden" name="item_name" value="Support Tonitor">
      <input type="hidden" name="currency_code" value="EUR">
      <input type="image" src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" name="submit"
             title="PayPal - The safer, easier way to pay online!" alt="Donate with PayPal button">
    </form>
  </aside>
  <div class="content">
    <main>
{body}
    </main>
    <footer>
      <p>Tonitor is by Nuno Zimas, built on <a href="https://github.com/hugelton/Felucca">Felucca</a>. Firmware: GPL-3.0. Website and logo: MIT.
        <a href="{REPO}">Source on GitHub</a>.</p>
      <p>M-VAVE and FM-1 are trademarks of their owner, used only to say which device Tonitor runs on.</p>
      <form class="donate donate-foot" action="https://www.paypal.com/cgi-bin/webscr" method="post" target="_top">
        <input type="hidden" name="cmd" value="_xclick">
        <input type="hidden" name="business" value="nzimas.lv@gmail.com">
        <input type="hidden" name="item_name" value="Support Tonitor">
        <input type="hidden" name="currency_code" value="EUR">
        <input type="image" src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" name="submit"
               title="PayPal - The safer, easier way to pay online!" alt="Donate with PayPal button">
      </form>
    </footer>
  </div>
</div>
<script src="js/menu.js?v={CSS_V}"></script>
</body>
</html>
"""


def fragments():
    src = ROOT / "site"
    (DOCS / "index.html").write_text(page(
        "Tonitor — a firmware for the M-VAVE FM-1", "Home", (src / "home.html").read_text(),
        "Tonitor: a firmware for the M-VAVE FM-1 for electroacoustic experimentation, live ambient music and sound "
        "exploration.",
        '\n<script src="js/demo.js" defer></script>'))
    (DOCS / "install.html").write_text(page(
        "Install Tonitor", "Install", (src / "install.html").read_text(),
        "Install Tonitor on an M-VAVE FM-1 from your browser, over USB MIDI.",
        '\n<script type="module" src="js/install.js"></script>'))
    (DOCS / "backup.html").write_text(page(
        "Back up Tonitor sessions", "Backup", (src / "backup.html").read_text(),
        "Save the sessions and presets on an FM-1 running Tonitor to a file, and restore them, from your browser.",
        '\n<script type="module" src="js/backup.js"></script>'))
    (DOCS / "projects.html").write_text(page(
        "Other projects by Nuno Zimas", "Other projects", (src / "projects.html").read_text(),
        "Firmware and instruments by Nuno Zimas for the Elektron Model:Cycles and Digitone II, the Ableton Move, "
        "the Akai MPC One and macOS."))


# The FM-1's face, in the units of Felucca's browser emulator (hugelton/Felucca, web/emu/index.html, its "wide"
# layout, 1000 x 624; GR!MM's page uses the same): the positions only, drawn here and numbered for the table.
FM1_KNOBS = {"MASTER": (105, 100, 24), "SELECT": (215, 100, 24), "PRESETS": (105, 205, 24), "ALGORITHM": (215, 205, 24),
             "KNOB 1": (610, 100, 22), "KNOB 2": (714, 100, 22), "KNOB 3": (818, 100, 22), "KNOB 4": (922, 100, 22)}
FM1_OCT = [(60, 258, 90, 34), (170, 258, 90, 34)]
FM1_GRID = (582, 174, 54, 48, 63, 62)            # the 2 x 6 buttons: x, y, w, h, step x, step y
FM1_KEYBED = (36, 332, 928, 256)
FM1_BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]   # the white key each black one follows
FM1_COLOURS = {"body": "#1d1d1f", "tray": "#141415", "part": "#2a2a2d", "edge": "#45454a"}

# the table under the drawings: (numbers, control, what it does under Tonitor); a row of several numbers spans them
CONTROLS = [
    ([1], "MASTER", "The output level."),
    ([2], "SELECT", "The chosen track's level. On the SCALE page (ARP): the root."),
    ([3], "PRESETS", "The chosen track's engine. <b>SHIFT +</b>: a new sound of the family before or after. On the "
                     "SESSIONS, PRESETS, MOD and GLOBAL SETUP pages: the slot or entry."),
    ([4], "ALGORITHM", "The chosen track's pan. On the SCALE page: the scale."),
    ([5], "OCT−", "The chosen track's octave down. <b>SHIFT +</b>: every loop one step of the scale down. "
                  "<b>SHIFT + a track button held +</b>: that track's loops only."),
    ([6], "OCT+", "The same, up."),
    ([7], "the screen", 'See <a href="#screen">Screen</a>.'),
    ([8], "KNOB 1", "The chosen track's filter (low-pass left, high-pass right)."),
    ([9], "KNOB 2", "The chosen track's granulator."),
    ([10], "KNOB 3", "The chosen track's crush. <b>SHIFT +</b>: its distortion."),
    ([11], "KNOB 4", "The chosen track's reverb send. <b>SHIFT +</b>: its delay send."),
    ([12, 13, 14, 15, 16, 17], ["FX (T1)", "SCL (T2)", "ENV (T3)", "LFO (T4)", "EDIT (T5)", "GLO (T6)"],
     "<b>T1</b> to <b>T6</b>: choose track 1 to 6. From any page, back to playing. "
     '<b>Held</b> (0.8 s): copy the track, or paste the copy onto it (<a href="#copying-a-track">Copying a track</a>).'),
    ([18], "HOME", "A new sound for the chosen track. <b>SHIFT +</b>: a new sound of the same family."),
    ([19], "SAVE", 'The <a href="#snapshots">SNAPSHOTS</a> page. <b>Held</b> (0.8 s): save the session. <b>SHIFT +</b>: the '
                   '<a href="#sessions">SESSIONS</a> page; <b>held</b>: GLOBAL SETUP (<a href="#global-setup-and-midi">MIDI</a>).'),
    ([20], "ARP", 'The SCALE page. <b>SHIFT +</b>: the engine\'s <a href="#presets">PRESETS page</a>.'),
    ([21], "SEQ", "The DELAY, REVERB and MOD pages in turn. <b>SHIFT +</b>: the chosen track's modulation drawn anew."),
    ([22], "PLAY", "Stop or play the chosen track's loops (on BALLSY's or BRICKLAYER's page, that sequencer). "
                   "<b>Held</b>: the loops on the white keys, to bring each in or take it out. <b>SHIFT +</b>: stop "
                   'everything, or play everything (<a href="#loopers">Loopers</a>).'),
    ([23], "REC", "Arm a loop, end it, overdub. <b>Held</b> (1 s): empty the track's loops. <b>SHIFT +</b>: VARY. "
                  "On BRICKLAYER's page: note entry."),
    ([24], "black key 1", 'The <b>AMBIENT</b> path; pressed again, OPEN (<a href="#generative-paths">Generative paths</a>). <b>SHIFT +</b>: a new <a href="#scales">scale and root</a> at random.'),
    ([25], "black key 2", "The <b>EXPERIMENTAL</b> path; pressed again, OPEN."),
    ([26], "black key 3", '<a href="#chords">Chord mode</a>: <b>held</b> (0.6 s), on or off; pressed, the next chord form.'),
    ([27], "black key 4", '<b>REVERSE</b>: the chosen track\'s loops backwards (<a href="#direction-and-speed">Direction '
                          "and speed</a>). Keys 4 to 8 are BRICKLAYER's own on its page."),
    ([28], "black key 5", "<b>PENDULUM</b>: forwards and backwards in turn."),
    ([29], "black key 6", "<b>RANDOM</b>: the loops' notes in an order drawn at random."),
    ([30], "black key 7", "<b>HALF SPEED</b>."),
    ([31], "black key 8", "<b>DOUBLE SPEED</b>."),
    ([32], "black key 9", 'The <a href="#envelope">ENVELOPE</a> page. <b>SHIFT +</b>: '
                          '<a href="#bricklayer">BRICKLAYER</a> on or off; <b>held</b> (0.6 s) while it is on, its page.'),
    ([33], "black key 10", '<b>EDIT</b>: KNOB 1–4 become the four <a href="#macros">macros</a> (not the EDIT button, T5). '
                           '<b>SHIFT +</b>: <a href="#ballsy">BALLSY</a> on or off; <b>held</b> (0.6 s) while it is on, '
                           "its page."),
    ([34], "black key 11", '<b>SHIFT</b>. White keys held, then SHIFT: they <a href="#latched-keys">latch</a>.'),
    ([35], "the 16 white keys", "Play the chosen track in the scale; the root's keys glow. With PLAY held: the loops "
                                "in and out. On BRICKLAYER's page, with REC on: its notes."),
]

# KNOB 1-4 on each page
KNOB_PAGES = [
    ("playing", "filter", "granulator", "crush", "reverb send"),
    ("SHIFT held", "", "", "distortion", "delay send"),
    ('EDIT (<a href="#macros">macros</a>)', "macro 1", "macro 2", "macro 3", "macro 4"),
    ('<a href="#envelope">ENVELOPE</a>', "ATTACK", "DECAY", "SUSTAIN", "RELEASE"),
    ('<a href="#delay">DELAY</a> (SEQ)', "TIME", "FEEDBACK", "DAMP", "MIX"),
    ('<a href="#reverb">REVERB</a> (SEQ twice)', "SIZE", "DECAY", "DAMP", "MIX"),
    ('<a href="#modulation">MOD</a> (SEQ three times)', "the entry's first row", "second row", "third row", "—"),
    ('<a href="#ballsy">BALLSY</a>', "balls", "speed", "octaves", "instability"),
    ('<a href="#bricklayer">BRICKLAYER</a>', "LENGTH", "RATE", "HARMONY", "VARIATION"),
]


def _svg_callouts(calls):
    """numbered leader lines: each a path from the control (a dot) to its number (a circle, a link to its row)"""
    out = []
    for n, pts in calls:
        d = " ".join(f"{x:g},{y:g}" for x, y in pts)
        (x0, y0), (x1, y1) = pts[0], pts[-1]
        out.append(f'<a href="#ctl-{n}" class="callout" data-n="{n}" aria-label="{n}">'
                   f'<polyline points="{d}" class="lead"/><circle cx="{x0:g}" cy="{y0:g}" r="4.5" class="tip"/>'
                   f'<circle cx="{x1:g}" cy="{y1:g}" r="17" class="num"/>'
                   f'<text x="{x1:g}" y="{y1 + 6.5:g}">{n}</text></a>')
    return "\n".join(out)


def panel_svg():
    """the face above the keys, its controls numbered 1-23"""
    c = FM1_COLOURS
    s = [f'<path d="M4,318 V44 A40,40 0 0 1 44,4 H956 A40,40 0 0 1 996,44 V318 Z" fill="{c["body"]}"/>',
         '<line x1="4" y1="318" x2="996" y2="318" class="cut"/>',
         f'<rect x="50" y="250" width="220" height="50" rx="14" fill="{c["tray"]}"/>',
         f'<rect x="568" y="160" width="400" height="140" rx="14" fill="{c["tray"]}"/>',
         '<rect x="292" y="44" width="256" height="256" rx="18" fill="#0b0b0c"/>',
         f'<rect x="300" y="52" width="240" height="240" rx="4" fill="#141418" stroke="{c["edge"]}"/>']
    for x, y, r in FM1_KNOBS.values():
        s.append(f'<circle cx="{x}" cy="{y}" r="{r + 5}" fill="{c["tray"]}"/>'
                 f'<circle cx="{x}" cy="{y}" r="{r}" fill="{c["part"]}" stroke="{c["edge"]}" stroke-width="2"/>'
                 f'<line x1="{x}" y1="{y - r + 5}" x2="{x}" y2="{y - r + 13}" stroke="#d6d6d8" stroke-width="3" stroke-linecap="round"/>')
    gx, gy, gw, gh, dx, dy = FM1_GRID
    rects = list(FM1_OCT) + [(gx + i * dx, gy + j * dy, gw, gh) for j in (0, 1) for i in range(6)]
    for x, y, w, h in rects:
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="{c["part"]}" stroke="{c["edge"]}" stroke-width="1.5"/>')
    k = FM1_KNOBS
    calls = [(1, [(k["MASTER"][0] - 29, 100), (-40, 100)]),
             (2, [(215, 100 - 29), (215, -40)]),
             (3, [(105 - 29, 205), (-40, 205)]),
             (4, [(194, 185), (160, 152), (-40, 152)]),
             (5, [(105, 292), (105, 372)]),
             (6, [(215, 292), (215, 372)]),
             (7, [(420, 44), (420, -40)])]
    calls += [(8 + i, [(k[f"KNOB {i + 1}"][0], 100 - 27), (k[f"KNOB {i + 1}"][0], -40)]) for i in range(4)]
    # the top row's leaders go up between the knobs (14 and 15 share a gap: 15 goes higher), the bottom row's down
    for i, tx in enumerate([566, 672, 750, 782, 861, 968]):
        cx = gx + gw // 2 + i * dx
        pts = [(cx, gy)] + ([(cx, 150), (tx, 150)] if tx != cx else []) + [(tx, -140 if i == 3 else -95)]
        calls.append((12 + i, pts))
    calls += [(18 + i, [(gx + gw // 2 + i * dx, gy + dy + gh), (gx + gw // 2 + i * dx, 372)]) for i in range(6)]
    return ('<svg class="fm1" viewBox="-70 -170 1085 565" role="img" aria-labelledby="fig-panel">\n'
            + "\n".join(s) + "\n" + _svg_callouts(calls) + "\n</svg>")


def keys_svg():
    """the keys below the face, numbered 24-35"""
    c = FM1_COLOURS
    x0, y0, w, h = FM1_KEYBED
    pitch = (w - 24) / 16
    kw, kh = pitch * 0.78, h * 0.425
    s = [f'<path d="M4,318 V580 A40,40 0 0 0 44,620 H956 A40,40 0 0 0 996,580 V318 Z" fill="{c["body"]}"/>',
         '<line x1="4" y1="318" x2="996" y2="318" class="cut"/>',
         f'<rect x="{x0}" y="{y0}" width="{w}" height="{h}" rx="18" fill="{c["tray"]}"/>']
    key = lambda cx, cy, fill: (f'<rect x="{cx - kw / 2:.1f}" y="{cy - kh / 2:.1f}" width="{kw:.1f}" height="{kh:.1f}" '
                                f'rx="{kw / 2:.1f}" fill="{fill}" stroke="{c["edge"]}" stroke-width="1.5"/>')
    whites = [x0 + 12 + (i + 0.5) * pitch for i in range(16)]
    blacks = [x0 + 12 + (a + 1) * pitch for a in FM1_BLACK_AFTER]
    s += [key(x, y0 + h * 0.75, "#d9d6d0") for x in whites]
    s += [key(x, y0 + h * 0.25, c["part"]) for x in blacks]
    top = y0 + h * 0.25 - kh / 2
    calls = [(24 + j, [(x, top), (x, 282)]) for j, x in enumerate(blacks)]
    # one bracket under all the white keys
    by, l, r = 600, whites[0] - kw / 2, whites[-1] + kw / 2
    s.append(f'<polyline points="{l:.1f},{by - 8} {l:.1f},{by} {r:.1f},{by} {r:.1f},{by - 8}" class="lead"/>')
    calls.append((35, [(500, by), (500, 662)]))
    return ('<svg class="fm1" viewBox="-20 255 1040 430" role="img" aria-labelledby="fig-keys">\n'
            + "\n".join(s) + "\n" + _svg_callouts(calls) + "\n</svg>")


def controls():
    """the section "Controls": the two drawings and the table of what each number does"""
    rows = []
    for nums, name, what in CONTROLS:
        names = name if isinstance(name, list) else [name]
        for i, (n, nm) in enumerate(zip(nums, names)):
            cell = f'<td rowspan="{len(nums)}">{what}</td>' if i == 0 else ""
            rows.append(f'<tr id="ctl-{n}" data-n="{n}"><td class="n"><a href="#fig-{"panel" if n < 24 else "keys"}">{n}</a></td>'
                        f"<td><b>{nm}</b></td>{cell}</tr>")
    pages = "\n".join("<tr>" + "".join(f"<td>{x}</td>" for x in r) + "</tr>" for r in KNOB_PAGES)
    return f"""<h2 id="controls">Controls</h2>
<p>The numbers on the drawings are in the table below them; a number links to its row, and a row's number back to its drawing. The names are those printed on the FM-1. Black keys are counted from the left.</p>
<figure class="fm1-fig" id="fig-panel">
<div class="fm1-scroll">{panel_svg()}</div>
<figcaption>The panel (1–23).<span class="narrow"> Swipe sideways to see all of it.</span></figcaption>
</figure>
<figure class="fm1-fig" id="fig-keys">
<div class="fm1-scroll">{keys_svg()}</div>
<figcaption>The keys (24–35).<span class="narrow"> Swipe sideways to see all of it.</span></figcaption>
</figure>
<table class="controls">
<tr><th>No.</th><th>Control</th><th>Under Tonitor</th></tr>
{chr(10).join(rows)}
</table>
<p>KNOB 1–4 on each page:</p>
<table class="knob-pages">
<tr><th>Page</th><th>KNOB 1</th><th>KNOB 2</th><th>KNOB 3</th><th>KNOB 4</th></tr>
{pages}
</table>
<p>Every gesture in full: <a href="#playing-it">Playing it</a>.</p>"""


def manual():
    md = (ROOT / "README.md").read_text()
    part = md[md.index("\n# Manual\n"):md.index("\n# Building from source\n")]
    part = part.replace("\n# Manual\n", "\n", 1)
    toc = [(2, "controls", "Controls")]
    body = controls() + "\n" + to_html(part, toc)
    t = "\n".join(f'<a href="#{s}"' + (' class="sub"' if lv == 3 else "") + f">{html.escape(n)}</a>" for lv, s, n in toc)
    bar = ('<div class="mbar" role="search">\n'
           '<input type="search" id="msearch" placeholder="Search the manual" aria-label="Search the manual" '
           'autocomplete="off" aria-controls="mresults" aria-expanded="false">\n'
           '<div id="mresults" class="mresults" role="listbox" hidden></div>\n'
           '</div>')
    # bottom right, once the contents are scrolled past: back to the contents (a list), back to the top (an arrow)
    icon = lambda d: (f'<svg viewBox="0 0 24 24" width="22" height="22" aria-hidden="true" fill="none" stroke="currentColor" '
                      f'stroke-width="2" stroke-linecap="round" stroke-linejoin="round">{d}</svg>')
    float_nav = ('<nav class="mfloat" aria-label="Page">\n'
                 '<a href="#contents" data-tip="Contents" aria-label="Contents">'
                 + icon('<path d="M9 6h11M9 12h11M9 18h11"/><circle cx="4.5" cy="6" r="1"/><circle cx="4.5" cy="12" r="1"/>'
                        '<circle cx="4.5" cy="18" r="1"/>') + '</a>\n'
                 '<a href="#top" data-tip="Back to top" aria-label="Back to top">'
                 + icon('<path d="M12 19V5M5 12l7-7 7 7"/>') + '</a>\n'
                 '</nav>')
    intro = ('<h1 id="top">Manual</h1>\n' + bar + '\n<p>Everything the FM-1 does under Tonitor. New here? Start with '
             '<a href="#controls">Controls</a> and <a href="#playing-it">Playing it</a>; to install, see '
             '<a href="install.html">Install</a>.</p>\n'
             '<h2 id="not-a-groovebox">Not a groovebox</h2>\n'
             '<p class="big">No tempo, no steps, no patterns, no drum kit.</p>\n'
             '<p>Tonitor is for sound heard for itself: texture, grain, movement and slow transformation. You play '
             'drones, gestures and textures, layer them, loop them freely out of time with one another, and shape '
             'them while they play. It suits acousmatic and electroacoustic work as much as live ambient music, '
             'dark ambient above all.</p>\n'
             f'<nav class="toc" id="contents" aria-label="Contents">\n{t}\n</nav>')
    (DOCS / "manual.html").write_text(page("Tonitor manual", "Manual", '<div class="manual">\n' + intro + "\n" + body + "\n" + float_nav + "\n</div>",
                                           "The Tonitor manual: every control of the FM-1 under Tonitor.",
                                           f'\n<script src="js/manual.js?v={MANUAL_JS_V}" defer></script>'))


def changelog():
    md = (ROOT / "CHANGELOG.md").read_text()
    body = to_html(md, [])
    (DOCS / "changelog.html").write_text(page("Tonitor changelog", "Changelog", body, "What changed in each Tonitor version."))


def firmware(pkg):
    raw = pkg.read_bytes()
    sys.path.insert(0, str(ROOT / "tools"))
    import fm1_install as fi
    product, _ = fi.load_package(str(pkg), False)
    ver = re.search(r'^VERSION = "([^"]+)"', (ROOT / "tools" / "build.py").read_text(), re.M)[1]
    d = DOCS / "firmware"
    d.mkdir(exist_ok=True)
    for old in d.glob("*.fwsc"):
        old.unlink()
    name = "tonitor.fwsc"
    shutil.copyfile(pkg, d / name)
    (d / "latest.json").write_text(json.dumps({"file": name, "version": ver, "product": product,
                                               "size": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}, indent=1) + "\n")
    print(f"site: firmware {ver} ({product}, {len(raw)} B)")


if __name__ == "__main__":
    fragments()
    manual()
    changelog()
    firmware(Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "tonitor.fwsc")
    print("site: docs/index.html, install.html, backup.html, manual.html, changelog.html, projects.html")
