// Checks keymap.js against the repo's keymaps: node --test docs/layout/keymap.test.js
"use strict";
const test = require("node:test");
const assert = require("node:assert");
const fs = require("node:fs");
const path = require("node:path");
const { COLUMNS, ROWS, parseKeymap, describeBinding } = require("./keymap.js");

const config = path.join(__dirname, "../../zmk/config");

for (const file of ["roamyboard_split.keymap", "roamyboard.keymap"]) {
  test(`${file}: four layers of ${COLUMNS * ROWS} bindings, all with labels`, () => {
    const keymap = parseKeymap(fs.readFileSync(path.join(config, file), "utf8"));
    assert.deepStrictEqual(keymap.layers.map(l => l.name), ["QWERTY", "Keypad", "System", "Gaming"]);
    for (const layer of keymap.layers) {
      assert.strictEqual(layer.bindings.length, COLUMNS * ROWS, layer.name);
      layer.bindings.forEach((b, i) => {
        const d = describeBinding(b, keymap);
        assert.notStrictEqual(d.kind, "unknown", `${layer.name} position ${i}: "${b.raw}" has no label`);
      });
    }
  });
}

test("labels from the layout's legend", () => {
  const keymap = parseKeymap(fs.readFileSync(path.join(config, "roamyboard_split.keymap"), "utf8"));
  const d = raw => {
    const [behavior, ...params] = raw.slice(1).split(" ");
    const { label, name } = describeBinding({ behavior, params, raw }, keymap);
    return [label, name];
  };
  assert.deepStrictEqual(d("&kp LSHFT"), ["⇧", "Left Shift"]);
  assert.deepStrictEqual(d("&kp LS(LCTRL)"), ["⇧⌃", "Shift + Control"]);
  assert.deepStrictEqual(d("&kp DEL"), ["fn⌫", "Forward delete"]);
  assert.deepStrictEqual(d("&mo 1"), ["L1", "Hold: Keypad"]);
  assert.deepStrictEqual(d("&to 3"), ["To3", "Switch to Gaming"]);
  assert.deepStrictEqual(d("&bt BT_SEL 0"), ["BT1", "Bluetooth profile 1"]);
  assert.deepStrictEqual(d("&bt BT_SEL 4"), ["BT5", "Bluetooth profile 5"]);
  assert.deepStrictEqual(d("&ret_boot 0 RET"), ["↩/Boot", "Tap: Return. Hold 1.5 s: Bootloader"]);
  assert.deepStrictEqual(d("&ota_boot"), ["OTA", "Bluetooth bootloader"]);
  assert.deepStrictEqual(d("&mkp LCLK"), ["M1", "Left click"]);
  assert.deepStrictEqual(d("&mmv MOVE_UP"), ["M↑", "Move the pointer up"]);
  assert.deepStrictEqual(d("&msc SCRL_DOWN"), ["W↓", "Scroll down"]);
  assert.deepStrictEqual(d("&none"), ["", "Nothing"]);
  assert.deepStrictEqual(d("&kp NOT_A_KEY"), ["&kp NOT_A_KEY", "&kp NOT_A_KEY"]);
});

test("parse errors name the problem", () => {
  assert.throws(() => parseKeymap("/ { };"), /zmk,keymap/);
  assert.throws(() => parseKeymap('/ { keymap { compatible = "zmk,keymap"; base { bindings = <&kp A>; }; }; };'), /"base" has 1 bindings; expected 120/);
});
