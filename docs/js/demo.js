/* SPDX-License-Identifier: MIT -- Tonitor website */
(function () {   /* the demo: nothing from YouTube until it is played; then its player in place of the frame */
  var box = document.querySelector(".demo");
  if (!box) return;
  box.querySelector(".demo-play").addEventListener("click", function () {
    var f = document.createElement("iframe");
    f.src = "https://www.youtube-nocookie.com/embed/" + box.dataset.video + "?autoplay=1&playsinline=1&rel=0";
    f.title = "Tonitor on the M-VAVE FM-1: demo";
    f.allow = "autoplay; encrypted-media; picture-in-picture; fullscreen";
    f.allowFullscreen = true;
    f.referrerPolicy = "strict-origin-when-cross-origin";
    this.remove();
    box.classList.add("playing");
    box.appendChild(f);
    f.focus();
  });
})();
