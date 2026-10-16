/* SPDX-License-Identifier: MIT -- Tonitor website */
(function () {   /* narrow screens: the menu slides in from the right */
  var btn = document.querySelector(".menu-btn"), nav = document.getElementById("site-nav"),
      shade = document.querySelector(".menu-shade"), close = document.querySelector(".menu-close");
  function set(open) {
    document.body.classList.toggle("menu-open", open);
    btn.setAttribute("aria-expanded", open ? "true" : "false");
    shade.hidden = !open;
    if (open) close.focus(); else btn.focus();
  }
  btn.addEventListener("click", function () { set(true); });
  close.addEventListener("click", function () { set(false); });
  shade.addEventListener("click", function () { set(false); });
  document.addEventListener("keydown", function (e) {
    if (e.key === "Escape" && document.body.classList.contains("menu-open")) set(false);
  });
  nav.addEventListener("click", function (e) { if (e.target.tagName === "A") set(false); });
})();
