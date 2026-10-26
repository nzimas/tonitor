/* SPDX-License-Identifier: MIT -- Tonitor website */
(function () {   /* the manual: search, and the drawings' numbers tied to their rows */
  var input = document.getElementById("msearch"), box = document.getElementById("mresults");
  var root = document.querySelector(".manual");
  if (!input || !root) return;

  /* every paragraph, item, table row and heading, with the heading it falls under */
  var norm = function (s) {
    return s.toLowerCase().replace(/[−–—]/g, "-").replace(/♯/g, "#").normalize("NFD").replace(/[̀-ͯ]/g, "");
  };
  var items = [], h2 = "", h3 = "", h2id = "", h3id = "";
  root.querySelectorAll("h2, h3, p, li, tr, blockquote, pre").forEach(function (el) {
    if (el.closest(".mbar, .toc, figure")) return;
    if (el.tagName === "H2") { h2 = el.textContent; h2id = el.id; h3 = ""; h3id = ""; }
    if (el.tagName === "H3") { h3 = el.textContent; h3id = el.id; }
    if (el.tagName === "LI" && el.querySelector("li")) return;
    if (el.tagName === "TR" && el.querySelector("th")) return;
    var text = (el.tagName === "TR" ? Array.prototype.map.call(el.cells, function (c) { return c.textContent; }).join(" · ")
                                    : el.textContent).replace(/\s+/g, " ").trim();
    if (!text) return;
    items.push({ el: el, text: text, low: norm(text), where: h3 ? h2 + " › " + h3 : h2, head: /^H[23]$/.test(el.tagName) });
  });

  var esc = function (s) { return s.replace(/[&<>"]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]; }); };
  function snippet(it, terms) {   /* ~160 characters around the first term, the terms marked */
    var at = it.low.indexOf(terms[0]), from = Math.max(0, at - 60);
    if (from > 0) { var sp = it.text.indexOf(" ", from); if (sp >= 0 && sp < at) from = sp + 1; }   /* from a word's start */
    var s = it.text.slice(from, from + 170);
    var low = norm(s), out = "", i = 0;
    while (i < s.length) {
      var hit = null;
      for (var t = 0; t < terms.length; t++) if (low.startsWith(terms[t], i)) { hit = terms[t]; break; }
      if (hit) { out += "<mark>" + esc(s.slice(i, i + hit.length)) + "</mark>"; i += hit.length; }
      else { out += esc(s[i]); i++; }
    }
    return (from > 0 ? "… " : "") + out + (from + 170 < it.text.length ? " …" : "");
  }

  var shown = [], sel = -1;
  function close() { box.hidden = true; input.setAttribute("aria-expanded", "false"); sel = -1; }
  function mark(i) {
    var links = box.querySelectorAll("a");
    links.forEach(function (a, j) { a.setAttribute("aria-selected", j === i ? "true" : "false"); });
    sel = i;
    if (links[i]) links[i].scrollIntoView({ block: "nearest" });
  }
  function go(it) {
    close();
    it.el.scrollIntoView({ block: "start" });
    it.el.classList.remove("found");
    void it.el.offsetWidth;
    it.el.classList.add("found");
  }
  function search() {
    var terms = norm(input.value).split(/\s+/).filter(Boolean);
    if (!terms.length || (terms.length === 1 && terms[0].length < 2)) { close(); return; }
    shown = items.filter(function (it) { return terms.every(function (t) { return it.low.indexOf(t) >= 0; }); });
    shown.sort(function (a, b) { return b.head - a.head; });   /* headings first, then in the manual's order */
    shown = shown.slice(0, 60);
    box.innerHTML = shown.length ? shown.map(function (it, i) {
      return '<a href="#" role="option" data-i="' + i + '"><span class="where">' + esc(it.where || "Manual") +
             '</span><span class="what">' + snippet(it, terms) + "</span></a>";
    }).join("") : '<div class="none">Nothing found.</div>';
    box.hidden = false;
    input.setAttribute("aria-expanded", "true");
    sel = -1;
    if (shown.length) mark(0);
  }
  input.addEventListener("input", search);
  input.addEventListener("focus", function () { if (input.value) search(); });
  input.addEventListener("keydown", function (e) {
    if (e.key === "Escape") { close(); return; }
    if (box.hidden || !shown.length) return;
    if (e.key === "ArrowDown") { e.preventDefault(); mark(Math.min(sel + 1, shown.length - 1)); }
    else if (e.key === "ArrowUp") { e.preventDefault(); mark(Math.max(sel - 1, 0)); }
    else if (e.key === "Enter") { e.preventDefault(); go(shown[Math.max(sel, 0)]); }
  });
  box.addEventListener("mousedown", function (e) { e.preventDefault(); });   /* keep the focus, so the click lands */
  box.addEventListener("click", function (e) {
    var a = e.target.closest("a");
    if (a) { e.preventDefault(); go(shown[+a.dataset.i]); }
  });
  document.addEventListener("click", function (e) { if (!e.target.closest(".mbar")) close(); });
  document.addEventListener("keydown", function (e) {   /* "/" from anywhere: the search */
    if (e.key === "/" && document.activeElement.tagName !== "INPUT") { e.preventDefault(); input.focus(); input.select(); }
  });

  /* a number on a drawing and its row light together */
  function hot(n, on) {
    document.querySelectorAll('.callout[data-n="' + n + '"], tr[data-n="' + n + '"]').forEach(function (el) {
      el.classList.toggle("hot", on);
    });
  }
  document.querySelectorAll(".callout, table.controls tr[data-n]").forEach(function (el) {
    var n = el.getAttribute("data-n");
    el.addEventListener("mouseenter", function () { hot(n, true); });
    el.addEventListener("mouseleave", function () { hot(n, false); });
    el.addEventListener("focusin", function () { hot(n, true); });
    el.addEventListener("focusout", function () { hot(n, false); });
  });
  /* the web font loaded late moves everything: back to the address's anchor once it is in */
  if (location.hash && document.fonts) document.fonts.ready.then(function () {
    var el = document.getElementById(location.hash.slice(1));
    if (el && scrollY > 0) el.scrollIntoView();
  });
  /* a row reached from a drawing flashes; with rowspan, the row of 12 holds the text for 12-17 */
  window.addEventListener("hashchange", function () {
    var el = document.getElementById(location.hash.slice(1));
    if (el && el.matches("tr[data-n]")) { el.classList.remove("found"); void el.offsetWidth; el.classList.add("found"); }
  });
})();
