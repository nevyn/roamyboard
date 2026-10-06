// Checks the order page's data and geometry: node --test docs/order/order.test.js
"use strict";
const test = require("node:test");
const assert = require("node:assert");
const { PITCH, WIDTH, arcRadius, radiusFromSagitta } = require("./geometry.js");
const C = require("./catalog.js");
const { PARTS, RATES, FILAMENT } = require("../parts.js");

test("the arc matches the case model", () => {
  // case/Case design.md: one 8° rotation about a centre 149 mm below the board.
  assert.ok(Math.abs(arcRadius(8) - 149) < 0.5, `8° gives ${arcRadius(8)} mm, expected 149`);
  assert.strictEqual(arcRadius(0), Infinity);
  assert.ok(Math.abs(PITCH - 20.85) < 1e-9);
  // A bigger angle is a tighter curve, always.
  for (let d = 1; d < 30; d++) assert.ok(arcRadius(d) > arcRadius(d + 1), `${d}° should bend wider than ${d + 1}°`);
});

test("the sagitta formula inverts a known arc", () => {
  const R = 149, span = 180;
  const drop = R - Math.sqrt(R * R - (span / 2) ** 2);
  assert.ok(Math.abs(radiusFromSagitta(span, drop) - R) < 0.5);
  assert.strictEqual(radiusFromSagitta(180, 0), null);
});

test("every curve on offer is one the geometry can draw", () => {
  assert.ok(C.CURVES.some(c => c.default), "one curve is the default");
  assert.strictEqual(C.CURVES.filter(c => c.default).length, 1);
  for (const c of C.CURVES) {
    assert.ok(Number.isFinite(c.angle) && c.angle >= 0 && c.angle < 30, `${c.name}: ${c.angle}°`);
    const r = arcRadius(c.angle);
    assert.ok(r > 50, `${c.name}: ${r} mm would wrap tighter than any leg`);
  }
});

test("what the page starts on exists in the catalogue", () => {
  assert.ok(C.FILAMENTS[C.DEFAULTS.material], C.DEFAULTS.material);
  assert.ok(C.FILAMENTS[C.DEFAULTS.material].some(f => f[0] === C.DEFAULTS.caseColour), C.DEFAULTS.caseColour);
  assert.ok(C.CAPS.some(c => c[0] === C.DEFAULTS.capColour), C.DEFAULTS.capColour);
  assert.ok(C.SWITCHES.some(s => s.name === C.DEFAULTS.switch), C.DEFAULTS.switch);
});

test("filament colours are named hex, and both materials have some", () => {
  for (const [material, list] of Object.entries(C.FILAMENTS)) {
    assert.ok(list.length > 5, material);
    const codes = new Set();
    for (const [name, hex, code] of list) {
      assert.match(hex, /^#[0-9A-F]{6}$/, `${material} ${name}`);
      assert.ok(name && code, `${material} ${name}`);
      assert.ok(!codes.has(code), `${material}: filament code ${code} twice`);
      codes.add(code);
    }
  }
});

test("every keycap colour is a hex with a splitkb product behind it", () => {
  const names = new Set();
  for (const [name, hex, handle] of C.CAPS) {
    assert.match(hex, /^#[0-9A-F]{6}$/, name);
    assert.ok(handle && !handle.includes("/"), `${name}: ${handle} should be a bare product handle`);
    assert.ok(!names.has(name), `${name} twice`);
    names.add(name);
  }
  for (const [kind, pack] of Object.entries(C.CAP_PACKS)) {
    assert.ok(pack.pack > 0 && pack.price > 0, kind);
    assert.ok(pack.label, kind);
  }
});

test("every switch is Choc v1, priced, with a feel and a force", () => {
  for (const s of C.SWITCHES) {
    assert.ok(s.price > 0 && s.pack > 0, s.name);
    assert.ok(s.force >= 15 && s.force <= 80, `${s.name}: ${s.force} gf`);
    assert.match(s.feel, /^(linear|tactile|clicky|silent linear|silent tactile)$/, s.name);
    assert.ok(s.handle && !s.handle.includes("/"), s.name);
  }
});

// A part with no price silently costs nothing, which is the one way this page can lie about money.
test("every part is priced, or says who prices it", () => {
  const byHand = new Set(["Hookup wire, thin", "Insulated wire, up to 1.3 mm across", "Two-component epoxy"]);
  for (const row of PARTS) {
    if (row.group) continue;
    assert.ok(row.name && row.per, `a part row needs a name and a quantity: ${JSON.stringify(row)}`);
    if (row.per.text) { assert.ok(byHand.has(row.name), `${row.name} is measured by eye; add it to byHand`); continue; }
    const priced = row.price || row.grams || row.pricedBy;
    assert.ok(priced, `${row.name} has no price, no filament weight and no chooser to price it`);
    if (row.price) {
      assert.ok(row.price.amount > 0 && row.price.each > 0, row.name);
      assert.ok(RATES[row.price.currency], `${row.name}: no rate for ${row.price.currency}`);
      assert.ok(row.price.vendor && /^https:\/\//.test(row.price.url), `${row.name} needs a vendor and a link`);
    }
    if (row.pricedBy) assert.match(row.pricedBy, /^(switch|keycaps)$/, row.name);
  }
});

test("a build's quantities and bill come out sane", () => {
  const cols = 7, rows = 5, halves = 2;
  const c = { key: cols * halves, mcu: halves, term: halves, build: 1 };
  const qty = per => (per.key || 0) * c.key + (per.keyRow || 0) * c.key * rows +
    (per.mcu || 0) * c.mcu + (per.term || 0) * c.term + (per.build || 0);

  const switches = PARTS.find(r => r.pricedBy === "switch");
  assert.strictEqual(qty(switches.per), cols * rows * halves, "one switch per key");
  const caps = PARTS.find(r => r.pricedBy === "keycaps");
  assert.strictEqual(qty(caps.per), cols * rows * halves, "one keycap per key");
  const nano = PARTS.find(r => r.name === "nice!nano v2");
  assert.strictEqual(qty(nano.per), halves, "one controller per half");

  let parts = 0, grams = 0;
  for (const row of PARTS) {
    if (row.group || !row.per || row.per.text) continue;
    if (row.grams) { grams += row.grams * qty(row.per); continue; }
    if (row.price) parts += (row.price.amount / RATES[row.price.currency] / row.price.each) * qty(row.per);
  }
  assert.ok(grams > 200 && grams < 400, `${grams} g of filament for a 7-column split looks wrong`);
  assert.ok(parts > 50 && parts < 400, `€${parts} of parts for a 7-column split looks wrong`);

  const hours = halves * (C.ASSEMBLY.hours.key * cols + C.ASSEMBLY.hours.mcu + C.ASSEMBLY.hours.term);
  assert.strictEqual(hours, 8.5);
  assert.ok(FILAMENT.amount > 0 && RATES[FILAMENT.currency]);
});
