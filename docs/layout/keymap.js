// Parses a ZMK keymap file and describes its bindings as key caps. No DOM: layout.js renders, and
// keymap.test.js runs this in Node against zmk/config/*.keymap.
(function (root) {
  "use strict";

  const COLUMNS = 24, ROWS = 5;

  // ---- parsing

  const stripComments = src => src.replace(/\/\*[\s\S]*?\*\//g, " ").replace(/\/\/[^\n]*/g, " ");

  // Index of the brace that closes the one at `open`.
  function closingBrace(src, open) {
    let depth = 0;
    for (let i = open; i < src.length; i++) {
      if (src[i] === "{") depth++;
      else if (src[i] === "}" && --depth === 0) return i;
    }
    throw new Error(`unbalanced braces after offset ${open}`);
  }

  // The direct child nodes of a node body: [{ name, label, body }], in file order.
  function childNodes(body) {
    const nodes = [], head = /(?:([A-Za-z_][\w-]*)\s*:\s*)?([A-Za-z_][\w,@-]*)\s*\{/g;
    let m;
    while ((m = head.exec(body))) {
      const open = m.index + m[0].length - 1, close = closingBrace(body, open);
      nodes.push({ label: m[1] || null, name: m[2], body: body.slice(open + 1, close) });
      head.lastIndex = close + 1;
    }
    return nodes;
  }

  // Splits `&kp A &mo 1 &kp LS(LCTRL)` into [{ behavior, params, raw }].
  function splitBindings(text) {
    const tokens = [];
    for (const t of text.trim().split(/\s+/).filter(Boolean)) {
      const last = tokens[tokens.length - 1];
      if (last && (last.split("(").length > last.split(")").length)) tokens[tokens.length - 1] += t;
      else tokens.push(t);
    }
    const out = [];
    for (const t of tokens) {
      if (t.startsWith("&")) out.push({ behavior: t.slice(1), params: [] });
      else if (out.length) out[out.length - 1].params.push(t);
      else throw new Error(`binding parameter "${t}" has no behavior before it`);
    }
    for (const b of out) b.raw = ["&" + b.behavior, ...b.params].join(" ");
    return out;
  }

  /**
   * Parses the text of a ZMK .keymap file.
   *
   * Returns { layers, holdTaps }:
   * - layers: [{ id, name, bindings }] in keymap order, so a layer's index is its ZMK layer number. `name` is the
   *   layer's display-name, or its node name without one. `bindings` has COLUMNS * ROWS entries
   *   { behavior, params, raw }, row-major: position = row * COLUMNS + keymap column.
   * - holdTaps: { label: { tappingTermMs, hold, tap } } for every hold-tap behavior that the file defines, where
   *   hold and tap are behavior names without the "&".
   *
   * `#define NAME <number>` lines are substituted into binding parameters. Throws an Error naming the problem
   * when the file has no keymap node, a layer has no bindings, or a layer has the wrong number of bindings.
   */
  function parseKeymap(source) {
    const defines = {};
    for (const m of source.matchAll(/^\s*#define\s+(\w+)\s+(\d+)\s*$/gm)) defines[m[1]] = m[2];
    const src = stripComments(source);

    const holdTaps = {};
    for (const m of src.matchAll(/([A-Za-z_]\w*)\s*:\s*[\w-]+\s*\{([^{}]*)\}/g)) {
      const body = m[2];
      if (!/compatible\s*=\s*"zmk,behavior-hold-tap"/.test(body)) continue;
      const refs = [...(body.match(/(?:^|[^\w-])bindings\s*=\s*([^;]*);/) || ["", ""])[1].matchAll(/&(\w+)/g)].map(r => r[1]);
      const term = body.match(/tapping-term-ms\s*=\s*<\s*(\d+)\s*>/);
      holdTaps[m[1]] = { tappingTermMs: term ? +term[1] : null, hold: refs[0], tap: refs[1] };
    }

    const compat = src.search(/compatible\s*=\s*"zmk,keymap"/);
    if (compat < 0) throw new Error('no node with compatible = "zmk,keymap"');
    const open = src.lastIndexOf("{", compat);
    const keymapBody = src.slice(open + 1, closingBrace(src, open));

    const layers = childNodes(keymapBody).map(node => {
      const bindingsText = node.body.match(/(?:^|[^\w-])bindings\s*=\s*<([^>]*)>/);
      if (!bindingsText) throw new Error(`layer "${node.name}" has no bindings`);
      const bindings = splitBindings(bindingsText[1]).map(b => {
        b.params = b.params.map(p => defines[p] ?? p);
        return b;
      });
      if (bindings.length !== COLUMNS * ROWS)
        throw new Error(`layer "${node.name}" has ${bindings.length} bindings; expected ${COLUMNS * ROWS} (${ROWS} rows of ${COLUMNS})`);
      const name = node.body.match(/display-name\s*=\s*"([^"]*)"/);
      return { id: node.name, name: name ? name[1] : node.name, bindings };
    });
    if (!layers.length) throw new Error("the keymap node has no layers");
    return { layers, holdTaps };
  }

  // ---- key cap labels
  //
  // KEYS maps a ZMK key code (and its common aliases) to [cap label, long name, modifier name]. The modifier
  // name is set only for modifier keys and is how LS(LCTRL) reads "Shift + Control". Codes that are missing
  // render as their raw text, so extend the table when the keymap starts using a new code.

  const KEYS = {};
  const key = (codes, label, name, mod) => codes.split(" ").forEach(c => { KEYS[c] = { label, name, mod }; });

  "ABCDEFGHIJKLMNOPQRSTUVWXYZ".split("").forEach(c => key(c, c, c));
  for (let n = 0; n <= 9; n++) {
    key(`N${n} NUMBER_${n}`, `${n}`, `${n}`);
    key(`KP_N${n} KP_NUMBER_${n}`, `${n}`, `Keypad ${n}`);
  }
  for (let n = 1; n <= 24; n++) key(`F${n}`, `F${n}`, `F${n}`);

  key("ESC ESCAPE", "Esc", "Escape");
  key("GRAVE", "`", "Grave accent");
  key("MINUS", "-", "Minus");
  key("EQUAL", "=", "Equals");
  key("TAB", "⇥", "Tab");
  key("LBKT LEFT_BRACKET", "[", "Left bracket");
  key("RBKT RIGHT_BRACKET", "]", "Right bracket");
  key("SEMI SEMICOLON", ";", "Semicolon");
  key("SQT APOS APOSTROPHE SINGLE_QUOTE", "'", "Apostrophe");
  key("BSLH BACKSLASH", "\\", "Backslash");
  key("COMMA", ",", "Comma");
  key("DOT PERIOD", ".", "Period");
  key("FSLH SLASH", "/", "Slash");
  key("RET ENTER RETURN", "↩", "Return");
  key("SPACE", "␣", "Space");
  key("BSPC BACKSPACE", "⌫", "Backspace");
  key("DEL DELETE", "fn⌫", "Forward delete");
  key("CAPS CAPSLOCK CLCK", "Cps", "Caps Lock");
  key("PSCRN PRINTSCREEN", "PrS", "Print Screen");
  key("INS INSERT", "Ins", "Insert");
  key("HOME", "Hm", "Home");
  key("END", "End", "End");
  key("PG_UP PAGE_UP", "PgU", "Page Up");
  key("PG_DN PAGE_DOWN", "PgD", "Page Down");
  key("UP UP_ARROW", "↑", "Up arrow");
  key("DOWN DOWN_ARROW", "↓", "Down arrow");
  key("LEFT LEFT_ARROW", "←", "Left arrow");
  key("RIGHT RIGHT_ARROW", "→", "Right arrow");
  key("LPAR LEFT_PARENTHESIS", "(", "Left parenthesis");
  key("RPAR RIGHT_PARENTHESIS", ")", "Right parenthesis");
  key("LBRC LEFT_BRACE", "{", "Left brace");
  key("RBRC RIGHT_BRACE", "}", "Right brace");
  key("EXCL EXCLAMATION", "!", "Exclamation mark");
  key("AT AT_SIGN", "@", "At sign");
  key("HASH POUND", "#", "Hash");
  key("DLLR DOLLAR", "$", "Dollar");
  key("PRCNT PERCENT", "%", "Percent");
  key("CARET", "^", "Caret");
  key("AMPS AMPERSAND", "&", "Ampersand");
  key("STAR ASTRK ASTERISK", "*", "Asterisk");
  key("PIPE", "|", "Pipe");
  key("TILDE", "~", "Tilde");
  key("UNDER UNDERSCORE", "_", "Underscore");
  key("PLUS", "+", "Plus");
  key("COLON", ":", "Colon");
  key("DQT DOUBLE_QUOTES", "\"", "Double quote");
  key("LT LESS_THAN", "<", "Less than");
  key("GT GREATER_THAN", ">", "Greater than");
  key("QMARK QUESTION", "?", "Question mark");

  key("LSHFT LSHIFT LEFT_SHIFT", "⇧", "Left Shift", "Shift");
  key("RSHFT RSHIFT RIGHT_SHIFT", "⇧", "Right Shift", "Shift");
  key("LCTRL LCTL LEFT_CONTROL", "⌃", "Left Control", "Control");
  key("RCTRL RCTL RIGHT_CONTROL", "⌃", "Right Control", "Control");
  key("LALT LEFT_ALT", "⌥", "Left Alt (Option)", "Alt");
  key("RALT RIGHT_ALT", "⌥", "Right Alt (Option)", "Alt");
  key("LGUI LCMD LWIN LMETA LEFT_GUI", "⌘", "Left GUI (Command, Windows)", "GUI");
  key("RGUI RCMD RWIN RMETA RIGHT_GUI", "⌘", "Right GUI (Command, Windows)", "GUI");

  key("KP_DOT KP_PERIOD", ".", "Keypad decimal point");
  key("KP_PLUS", "+", "Keypad plus");
  key("KP_MINUS KP_SUBTRACT", "−", "Keypad minus");
  key("KP_MULTIPLY KP_ASTERISK", "×", "Keypad multiply");
  key("KP_DIVIDE KP_SLASH", "÷", "Keypad divide");
  key("KP_ENTER", "↩", "Keypad Enter");
  key("KP_EQUAL", "=", "Keypad equals");

  key("C_VOL_UP C_VOLUME_UP", "V+", "Volume up");
  key("C_VOL_DN C_VOLUME_DOWN", "V−", "Volume down");
  key("C_MUTE", "Mut", "Mute");
  key("C_PP C_PLAY_PAUSE", "Ply", "Play or pause");
  key("C_PREV C_PREVIOUS", "Prv", "Previous track");
  key("C_NEXT", "Nxt", "Next track");
  key("C_BRI_UP C_BRI_INC C_BRIGHTNESS_INC", "Br+", "Brightness up");
  key("C_BRI_DN C_BRI_DEC C_BRIGHTNESS_DEC", "Br−", "Brightness down");

  // Modifier functions: LS(X) presses Shift with X.
  const MOD_FNS = {
    LS: ["⇧", "Shift"], RS: ["⇧", "Right Shift"], LC: ["⌃", "Control"], RC: ["⌃", "Right Control"],
    LA: ["⌥", "Alt"], RA: ["⌥", "Right Alt"], LG: ["⌘", "GUI"], RG: ["⌘", "Right GUI"],
  };

  function describeKeyCode(code) {
    const fn = code.match(/^(\w+)\((.*)\)$/);
    if (fn && MOD_FNS[fn[1]]) {
      const inner = describeKeyCode(fn[2]);
      if (!inner) return null;
      const [symbol, modName] = MOD_FNS[fn[1]];
      return { label: symbol + inner.label, name: `${modName} + ${inner.mod ?? inner.name}` };
    }
    return KEYS[code] || null;
  }

  const layerName = (layers, n) => layers[+n] ? layers[+n].name : `layer ${n}`;
  const BT = { BT_CLR: ["BT×", "Clear the selected Bluetooth profile's pairing"], BT_CLR_ALL: ["BT×*", "Clear every Bluetooth profile's pairing"],
    BT_NXT: ["BT→", "Next Bluetooth profile"], BT_PRV: ["BT←", "Previous Bluetooth profile"] };
  const OUT = { OUT_BLE: ["BLE", "Send keys over Bluetooth"], OUT_USB: ["USB", "Send keys over USB"], OUT_TOG: ["Out", "Toggle between USB and Bluetooth"] };

  // Behaviors without parameters: [cap label, long name, detail].
  const PLAIN = {
    trans: ["︶", "Transparent: the layer below decides"],
    none: ["", "Nothing"],
    soft_off: ["Pwr", "Turn off", "ZMK soft off. Only the reset button turns the keyboard back on, because the polled chain cannot wake the nice!nano."],
    sys_reset: ["Rst", "Restart the firmware"],
    bootloader: ["Boot", "Bootloader", "Enters the UF2 bootloader."],
    boot_screen: ["Boot", "Bootloader", "Shows the bootloader view on the status screen, then enters the UF2 bootloader on the half that it is pressed on."],
    caps_word: ["CW", "Caps word"],
    key_repeat: ["Rep", "Repeat the last key"],
  };

  /**
   * Describes one binding as a key cap: { label, name, detail, kind, target }.
   * - label: the short text on the key cap ("" for &none).
   * - name: the long name for the tooltip and the screen reader.
   * - detail: an optional sentence with more about the behavior.
   * - kind: "key", "layer" (target is the layer number that the key reaches), "trans", "none", "system", or
   *   "unknown" when the behavior or key code is not in the tables; then label and name are the raw binding.
   * `keymap` is parseKeymap's result, for layer names and the keymap's own hold-tap behaviors.
   */
  function describeBinding(binding, keymap) {
    const { behavior, params, raw } = binding;
    const unknown = { label: raw, name: raw, kind: "unknown" };
    const p = params;

    if (behavior === "kp" && p.length === 1) {
      const k = describeKeyCode(p[0]);
      return k ? { label: k.label, name: k.name, kind: "key" } : unknown;
    }
    if (behavior === "trans") return { label: PLAIN.trans[0], name: PLAIN.trans[1], kind: "trans" };
    if (behavior === "none") return { label: "", name: PLAIN.none[1], kind: "none" };
    if (/^\d+$/.test(p[0] ?? "")) {
      const name = layerName(keymap.layers, p[0]);
      if (behavior === "mo" && p.length === 1) return { label: `L${p[0]}`, name: `Hold: ${name}`, kind: "layer", target: +p[0] };
      if (behavior === "to" && p.length === 1) return { label: `To${p[0]}`, name: `Switch to ${name}`, kind: "layer", target: +p[0] };
      if (behavior === "tog" && p.length === 1) return { label: `Tg${p[0]}`, name: `Toggle ${name}`, kind: "layer", target: +p[0] };
      if (behavior === "sl" && p.length === 1) return { label: `SL${p[0]}`, name: `${name} for the next key`, kind: "layer", target: +p[0] };
      if (behavior === "lt" && p.length === 2) {
        const k = describeKeyCode(p[1]);
        if (k) return { label: `L${p[0]}/${k.label}`, name: `Tap: ${k.name}. Hold: ${name}`, kind: "layer", target: +p[0] };
      }
    }
    if (behavior === "bt") {
      if (p[0] === "BT_SEL" && /^\d+$/.test(p[1] ?? "")) return { label: `BT${+p[1] + 1}`, name: `Bluetooth profile ${+p[1] + 1}`, detail: `Selects profile ${p[1]} (BT_SEL ${p[1]}).`, kind: "system" };
      if (BT[p[0]] && p.length === 1) return { label: BT[p[0]][0], name: BT[p[0]][1], kind: "system" };
    }
    if (behavior === "out" && OUT[p[0]] && p.length === 1) return { label: OUT[p[0]][0], name: OUT[p[0]][1], kind: "system" };
    if (PLAIN[behavior] && (p.length === 0 || behavior === "boot_screen")) {
      const [label, name, detail] = PLAIN[behavior];
      return { label, name, detail, kind: "system" };
    }

    const ht = keymap.holdTaps[behavior];
    if (ht && p.length === 2) {
      const hold = describeBinding({ behavior: ht.hold, params: [p[0]], raw: `&${ht.hold} ${p[0]}` }, keymap);
      const tap = describeBinding({ behavior: ht.tap, params: [p[1]], raw: `&${ht.tap} ${p[1]}` }, keymap);
      if (hold.kind === "unknown" || tap.kind === "unknown") return unknown;
      const holdFor = ht.tappingTermMs ? ` ${ht.tappingTermMs / 1000} s` : "";
      return {
        label: `${tap.label}/${hold.label}`, name: `Tap: ${tap.name}. Hold${holdFor}: ${hold.name}`,
        detail: hold.detail, kind: hold.kind === "layer" ? "layer" : "system", target: hold.target,
      };
    }
    return unknown;
  }

  const api = { COLUMNS, ROWS, parseKeymap, describeBinding };
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.RoamyKeymap = api;
})(this);
