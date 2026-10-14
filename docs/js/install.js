// SPDX-License-Identifier: GPL-3.0-only
// TONITOR: the installer page -- the firmware fetched and checked, the FM-1 found over Web MIDI, the update
// run by fm1ota.js with its progress in words and a bar.
import { Updater, loadPackage, InstallError } from "./fm1ota.js";

const $ = (id) => document.getElementById(id);
const ui = {
  status: $("status"), detail: $("detail"), facts: $("facts"), fw: $("f-fw"), dev: $("f-dev"),
  bar: $("bar"), fill: $("bar").firstElementChild, agree: $("agree"), connect: $("connect"), install: $("install"),
};
let pkg = null, updater = null, device = null, busy = false;

function say(status, detail = "", kind = "") {
  ui.status.textContent = status;
  ui.status.className = "status" + (kind ? " " + kind : "");
  ui.detail.textContent = detail;
}
function refresh() {
  ui.connect.disabled = busy || !pkg || !ui.agree.checked;
  ui.install.disabled = busy || !pkg || !device || !ui.agree.checked;
}
const guard = (e) => {
  e.preventDefault();
  e.returnValue = "";
};

async function loadFirmware() {
  try {
    const meta = await (await fetch("firmware/latest.json", { cache: "no-store" })).json();
    const r = await fetch(`firmware/${meta.file}`, { cache: "no-store" });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    const raw = new Uint8Array(await r.arrayBuffer());
    const digest = new Uint8Array(await crypto.subtle.digest("SHA-256", raw));
    const hex = [...digest].map((b) => b.toString(16).padStart(2, "0")).join("");
    if (hex !== meta.sha256) throw new InstallError("badpkg", "The firmware did not download intact. Reload the page.");
    pkg = { ...loadPackage(raw), version: meta.version };
    ui.fw.textContent = `Tonitor ${meta.version} (${pkg.product})`;
    ui.facts.hidden = false;
    if (!navigator.requestMIDIAccess) {
      say("This browser cannot reach the FM-1",
        "It has no Web MIDI. Open this page in a desktop Chrome, Edge or Opera.", "bad");
      pkg = null;
    } else if (!window.isSecureContext) {
      say("Open this page over https", "Web MIDI only works on a secure page.", "bad");
      pkg = null;
    } else
      say("Ready", "Tick the box below, then find your FM-1.");
  } catch (e) {
    say("The firmware could not be loaded", e.message, "bad");
  }
  refresh();
}

async function connect() {
  busy = true;
  refresh();
  say("Looking for the FM-1…", "Allow MIDI access if the browser asks.");
  try {
    if (!updater) updater = new Updater(await navigator.requestMIDIAccess({ sysex: true }));
    if (device) device.link.close();
    device = await updater.find();
    if (!device) {
      ui.dev.textContent = "not found";
      say("No FM-1 found", "Is it switched on and connected with a data cable? Is another program using it? " +
        "Then try again.", "bad");
    } else {
      const mode = device.id.loader ? " — in update mode: the install will finish the write" : "";
      ui.dev.textContent = `${device.id.text}${mode}`;
      say("FM-1 found", "Press Install Tonitor. Do not unplug it or close this page until it has restarted.");
    }
  } catch (e) {
    say("MIDI access was refused", "This page needs MIDI with system-exclusive messages to update the FM-1. " +
      "Allow it in the site settings, then reload.", "bad");
  }
  busy = false;
  refresh();
}

const STEP = {
  start: () => ["Starting the update", "The FM-1 checks the firmware."],
  check: (n) => ["Checking", `The FM-1 has read ${n} parts of the firmware.`],
  loader: () => ["Switching to update mode", "The FM-1 restarts into its update loader. It may take a few seconds."],
  write: (p) => [`Writing ${p}%`, "Do not unplug the FM-1 or close this page."],
  reboot: () => ["Restarting", "The FM-1 restarts into Tonitor."],
};

async function install() {
  if (!device || !pkg) return;
  busy = true;
  refresh();
  window.addEventListener("beforeunload", guard);
  ui.bar.hidden = false;
  ui.fill.style.width = "0%";
  try {
    const text = await updater.install(device, pkg.image, pkg.product, (k, a) => {
      if (k === "done") return;
      const [s, d] = STEP[k](a);
      say(s, d);
      if (k === "write") ui.fill.style.width = `${a}%`;
    });
    ui.fill.style.width = "100%";
    ui.dev.textContent = text;
    say("Done: your FM-1 runs Tonitor", "Read the manual to get started.", "ok");
  } catch (e) {
    const msg = e instanceof InstallError ? e.message : `Unexpected error: ${e.message}`;
    say("The install stopped", msg + "\nReload this page and try again; see “If something goes wrong” below.", "bad");
  }
  device = null;
  window.removeEventListener("beforeunload", guard);
  busy = false;
  refresh();
}

ui.agree.addEventListener("change", refresh);
ui.connect.addEventListener("click", connect);
ui.install.addEventListener("click", install);
loadFirmware();
