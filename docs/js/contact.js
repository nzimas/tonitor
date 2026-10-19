/* The contact page. The address is in neither the page nor this file as text: it is put together only after a
 * person answers a small question (numbers in words), and only if what happened looks like a person did it --
 * the answer typed or pasted (trusted input events, not a script setting the field), the button pressed by
 * hand (a trusted submit), no automated browser (navigator.webdriver), the hidden field left empty (bots fill
 * every field), no sooner than three seconds after the page opened. A refusal reads like a wrong answer, so a
 * bot learns nothing from it. The mailto: link is made only when "Write an email" is pressed, so no address
 * sits in a link for a scraper. */
(function () {
  var form = document.getElementById("contact-check");
  if (!form) return;
  var CODE = [52, 27, 1, 2, 23, 14, 196, 236, 255, 248, 201, 203, 128, 214, 211, 174];
  var WORDS = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"];
  var opened = Date.now(), a = 0, b = 0, tries = 0, typed = false, to = "";
  var q = document.getElementById("contact-q"), input = document.getElementById("contact-answer");
  var trap = document.getElementById("contact-website");
  var msg = document.getElementById("contact-msg"), out = document.getElementById("contact-out");

  function ask() {
    a = 2 + Math.floor(Math.random() * 8);
    b = 1 + Math.floor(Math.random() * 9);
    q.textContent = WORDS[a] + " plus " + WORDS[b];
    input.value = "";
    typed = false;
  }
  function address() {
    var s = "";
    for (var i = 0; i < CODE.length; i++) s += String.fromCharCode(CODE[i] ^ ((0x5A + 7 * i) & 0xFF));
    return s;
  }
  function refuse() {
    tries++;
    if (tries >= 5) {
      msg.textContent = "Too many tries. Reload the page to start again.";
      form.querySelector("button").disabled = true;
      input.disabled = true;
    } else {
      msg.textContent = "Not quite. Here is another one.";
      ask();
    }
  }
  input.addEventListener("input", function (e) {
    if (e.isTrusted) typed = true;
  });
  ask();
  form.addEventListener("submit", function (e) {
    e.preventDefault();
    if (Date.now() - opened < 3000) {
      msg.textContent = "A moment, please, then try again.";
      return;
    }
    var human = e.isTrusted && typed && !navigator.webdriver && !(trap && trap.value);
    if (!human || parseInt(String(input.value).trim(), 10) !== a + b) {
      refuse();
      return;
    }
    to = address();
    document.getElementById("contact-addr").textContent = to;
    msg.textContent = "";
    form.hidden = true;
    out.hidden = false;
    document.getElementById("contact-mail").focus();
  });
  document.getElementById("contact-mail").addEventListener("click", function (e) {
    if (!e.isTrusted || !to) return;
    var subject = form.getAttribute("data-subject") || "Hello";
    window.location.href = "mailto:" + to + "?subject=" + encodeURIComponent(subject);
  });
})();
