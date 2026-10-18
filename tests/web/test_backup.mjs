// SPDX-License-Identifier: GPL-3.0-only
// TONITOR: the backup page's protocol (docs/js/fm1backup.js) against the firmware's (firmware/src/backup.c, run by
// build/host bkserve): every record listed and read whole, the flash wiped, written back, a restart, the same
// records -- and a session put back loads.
import { spawn } from "node:child_process";
import { createInterface } from "node:readline";
import { list, readRecord, writeRecord, PRE_NUM } from "../../docs/js/fm1backup.js";

const dev = spawn(process.argv[2] || "build/host", ["bkserve"]);
const lines = [], waiters = [];
createInterface({ input: dev.stdout }).on("line", (l) => (waiters.length ? waiters.shift()(l) : lines.push(l)));
const next = (ms) => new Promise((res) => {
  if (lines.length) return res(lines.shift());
  const t = setTimeout(() => { waiters.splice(waiters.indexOf(f), 1); res(null); }, ms);
  const f = (l) => { clearTimeout(t); res(l); };
  waiters.push(f);
});
const hex = (a) => a.map((b) => b.toString(16).padStart(2, "0")).join("");
const unhex = (s) => s.match(/../g).map((h) => parseInt(h, 16));
const link = {
  send(p) { dev.stdin.write(hex(p) + "\n"); return true; },
  async read(ms) { const l = await next(ms); return l && /^f0/.test(l) ? unhex(l) : null; },
  drain() {},
};
const say = async (cmd) => { dev.stdin.write(cmd + "\n"); return next(5000); };
let fail = 0;
const check = (ok, what) => { if (!ok) { console.log("FAIL backup page: " + what); fail++; } };

if ((await next(10000)) !== "READY") check(false, "the device did not start");
const recs = await list(link);
check(recs && recs.length === 4, `the list (${JSON.stringify(recs)})`);
const saved = [];
for (const r of recs || []) saved.push({ number: r.number, bytes: await readRecord(link, r) });
check(saved.some((r) => r.number >= PRE_NUM) && saved.some((r) => r.number === 3), "sessions and presets read");
check(await say("WIPE") === "OK", "wipe");
check((await list(link)).length === 0, "the flash not wiped");
for (const r of saved) await writeRecord(link, r.number, r.bytes);
check(await say("SCAN") === "OK", "restart");
const again = await list(link);
check(again && again.length === recs.length, "not all back after a restart");
for (const r of again || []) {
  const b = await readRecord(link, r), was = saved.find((x) => x.number === r.number).bytes;
  check(b.length === was.length && b.slice(32).every((x, i) => x === was[32 + i]), `record ${r.number} differs`);
}
check(await say("LOAD 3") === "OK", "session 3 does not load");
let refused = false;                                   // a damaged record: refused
const bad = saved[0].bytes.slice();
bad[bad.length - 1] ^= 0x55;
try { await writeRecord(link, saved[0].number, bad); } catch (e) { refused = /not whole/.test(e.message); }
check(refused, "a damaged record not refused");
dev.stdin.end();
console.log(fail ? "backup page: FAILED" : `backup page: ok (${recs.length} records, ${saved.reduce((a, r) => a + r.bytes.length, 0)} bytes, out and back)`);
process.exit(fail ? 1 : 0);
