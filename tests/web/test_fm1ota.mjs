// SPDX-License-Identifier: GPL-3.0-only
// TONITOR: the browser installer (docs/js/fm1ota.js) without a browser: its wire format and package checks
// against tools/fm1_install.py on a real package, then a whole install -- the running firmware, the update
// loader, the restart -- against a simulated FM-1, and the ways it can go wrong.
//   node tests/web/test_fm1ota.mjs PACKAGE.fwsc PYTHON_FACTS.json
import { readFileSync } from "node:fs";
import * as ota from "../../docs/js/fm1ota.js";

let fail = 0;
const check = (ok, what) => {
  if (!ok) {
    console.log(`FAIL web: ${what}`);
    fail++;
  }
};

const raw = new Uint8Array(readFileSync(process.argv[2]));
const py = JSON.parse(readFileSync(process.argv[3], "utf8"));

// ------------------------------------------------- the same as the Python ---
const { product, image } = ota.loadPackage(raw);
check(product === py.product, `product ${product} (python ${py.product})`);
check(image.length === py.image_len, `image length ${image.length} (python ${py.image_len})`);
check(ota.crc16(image) === py.image_crc, "logical image differs from the Python's");
check(JSON.stringify(ota.response(0x12345, [1, 2, 3, 250, 0x7F, 0x80], 3)) === JSON.stringify(py.response),
  "response() differs from the Python's");
check(ota.packageDamage(image) === null, "a good package reported damaged");
{
  const bad = Uint8Array.from(raw);
  bad[bad.length - 100] ^= 0x55;
  let threw = false;
  try {
    ota.loadPackage(bad);
  } catch (e) {
    threw = e.code === "badpkg";
  }
  check(threw, "a damaged package was not refused");
}
{
  const r = [0x00, 0x59, 0x30, 0, 0, 0, 7, 0x00, 0x10, 0x02, 0x00, 0x00, 0x02, 0x00];
  r.push(~r.slice(6).reduce((a, b) => a + b, 0) & 0xFF);
  const q = ota.parseRequest([0xF0, ...ota.pack7(r), 0xF7]);
  check(q && q.fl === 7 && q.addr === 0x21000 && q.len === 0x200, "parseRequest");
  check(JSON.stringify(ota.unpack7(ota.pack7([0, 255, 128, 1, 2, 3, 4, 5, 6, 7]))) === JSON.stringify([0, 255, 128, 1, 2, 3, 4, 5, 6, 7]),
    "pack7 / unpack7");
}

// ------------------------------------------------------ a simulated FM-1 ---
for (const k of Object.keys(ota.DELAY)) ota.DELAY[k] = Math.min(ota.DELAY[k], k.startsWith("idle") || k.startsWith("wait") ? 400 : 5);

function idPacket(text) {
  const d = [0x00, 0x59, 0x11, 0, 0, 0];
  for (let i = 0; i < 27; i++) d.push(i < text.length ? text.charCodeAt(i) : 0);
  d.push(0);
  return [0xF0, ...ota.pack7(d), 0xF7];
}
function reqPacket(addr, n, fl = 0) {
  const u = [0x00, 0x59, 0x30, 0, 0, 0, fl, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF,
    n & 0xFF, (n >> 8) & 0xFF, 0];
  u.push(~u.slice(6).reduce((a, b) => a + b, 0) & 0xFF);
  return [0xF0, ...ota.pack7(u), 0xF7];
}

class Port extends EventTarget {
  constructor(name, dev) {
    super();
    this.name = name;
    this.state = "connected";
    this.dev = dev;
  }
  async open() {}
  send(pkt) {
    if (this.state !== "connected") throw new Error("gone");
    setTimeout(() => this.dev.rx(Array.from(pkt)), 1);
  }
}

/** the FM-1: running `ident`; UPGRADE: reads a few blocks, FINISH_CHECK, restarts as the loader; the loader:
 * reads the whole image in order, FINISH_WRITE, restarts running `after`. `opts` breaks it on purpose. */
class FakeFM1 {
  constructor(ident, after, opts = {}) {
    this.ident = ident;
    this.after = after;
    this.opts = opts;
    this.written = new Uint8Array(image.length);
    this.access = { inputs: new Map(), outputs: new Map() };
    this.other = new Port("IAC Driver Bus 1", { rx() {} });   // gear that must never be probed
    this.access.inputs.set("x", this.other);
    this.plug(ident.startsWith("ota-") ? "FM-1 OTA" : "FM-1 Tonitor");
  }
  plug(name) {
    this.inp = new Port(name, this);
    this.out = new Port(name, this);
    this.access.inputs.set("fm1", this.inp);
    this.access.outputs.set("fm1", this.out);
    this.pending = null;
  }
  unplug() {
    this.inp.state = this.out.state = "disconnected";
    this.access.inputs.delete("fm1");
    this.access.outputs.delete("fm1");
  }
  tx(pkt) {
    const p = this.inp;
    setTimeout(() => {
      if (p.state === "connected") p.dispatchEvent(Object.assign(new Event("midimessage"), { data: Uint8Array.from(pkt) }));
    }, 1);
  }
  restart(as, after_ms) {
    this.unplug();
    setTimeout(() => {
      this.ident = as;
      this.plug(as.startsWith("ota-") ? "FM-1 OTA" : "FM-1 Tonitor");
    }, after_ms);
  }
  rx(pkt) {
    if (JSON.stringify(pkt) === JSON.stringify(ota.HS_QUERY)) return this.tx(idPacket(this.ident));
    if (JSON.stringify(pkt) === JSON.stringify(ota.UPGRADE)) return this.upgrade();
    const u = ota.unpack7(pkt.slice(1, -1));
    if (u[1] === 0x59 && u[2] === 0x30 && this.pending) this.pending(u);
  }
  ask(addr, n) {
    return new Promise((res) => {
      this.pending = (u) => {
        this.pending = null;
        res(u.slice(14, 14 + n));
      };
      this.tx(reqPacket(addr, n));
    });
  }
  async upgrade() {
    const loader = this.ident.startsWith("ota-");
    if (!loader) {
      for (const a of [0, 0x40, 0x1000]) await this.ask(a, 64);
      if (this.opts.stopInCheck) return;
      await this.ask(ota.FINISH_CHECK, 8);
      this.restart("ota-FM-1_900", 20);
      return;
    }
    for (let a = 0; a < image.length; a += 512) {
      const n = Math.min(512, image.length - a);
      if (this.opts.unplugAt !== undefined && a >= this.opts.unplugAt) {
        this.unplug();
        return;
      }
      this.written.set(await this.ask(a, n), a);
    }
    await this.ask(ota.FINISH_WRITE, 8);
    this.restart(this.after, 20);
  }
}

async function run(dev, fm1) {
  const up = new ota.Updater(fm1.access);
  const steps = [];
  try {
    const found = await up.find();
    if (!found) return { err: "notfound", steps };
    const text = await up.install(found, image, product, (k, a) => steps.push(k));
    return { text, steps };
  } catch (e) {
    return { err: e.code, msg: e.message, steps };
  }
}

{   /* a whole install, from the running firmware */
  const fm1 = new FakeFM1("FM-1_700", "FM-1_700");
  const r = await run(null, fm1);
  check(r.text === "FM-1_700", `install: ${r.err || ""} ${r.msg || ""}`);
  check(JSON.stringify(Array.from(fm1.written)) === JSON.stringify(Array.from(image)), "install: the loader was not given the image");
  check(["start", "check", "loader", "write", "reboot", "done"].every((s) => r.steps.includes(s)), `install: steps ${[...new Set(r.steps)]}`);
}
{   /* an FM-1 left in update mode: the write is finished */
  const fm1 = new FakeFM1("ota-FM-1_900", "FM-1_700");
  const r = await run(null, fm1);
  check(r.text === "FM-1_700" && !r.steps.includes("check"), `resume: ${r.err || ""} ${r.steps}`);
}
{   /* another model, another firmware's loader, a stop before anything is written, a cable pulled while writing,
     * another identity after it */
  const cases = [
    [new FakeFM1("FM-2_100", "FM-2_100"), "model"],
    [new FakeFM1("ota-FM-1_500", "FM-1_700"), "foreign"],
    [new FakeFM1("FM-1_700", "FM-1_700", { stopInCheck: true }), "stopped"],
    [new FakeFM1("FM-1_700", "FM-1_700", { unplugAt: 0x8000 }), "lost"],
    [new FakeFM1("FM-1_700", "FM-1_906"), "mismatch"],
  ];
  for (const [fm1, want] of cases) {
    const r = await run(null, fm1);
    check(r.err === want, `expected ${want}, got ${r.err || r.text}`);
  }
}

console.log(`web installer: ${fail ? "FAILED" : "ok"}`);
process.exit(fail ? 1 : 0);
