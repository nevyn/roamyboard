// The sheets of the wordless guide, in order. A sheet has `figures` (3D line art: a box in sheet mm, a camera
// direction, items, and `draw(P)` for annotations placed with P, which projects model-frame mm onto the sheet) and
// `draw()` for everything 2D. Points in the module frame: x across from the socket face, y along from the SW1 end,
// z up from the board's back.
import * as D from "./draw.js";

const { g, at, arrow, qty, stepNo, line, rect, circle, text, path, poly, dot } = D;
const BOARD = ["pcb", "ics", "sockets", "headers", "tails", "headerTails"];
const SWITCH = ["switches", "legs"];
const KM = "key-module";
const UP = [0, 180, 0];                         // upturned, about the module's long axis
const up = ([x, y, z]) => [32 - x, y, -z];      // a module-frame point on an upturned module
const add = (a, b) => a.map((v, i) => v + b[i]);
const mid = (a, b, t = 0.5) => a.map((v, i) => v + (b[i] - v) * t);
const explode = (groups, d) => Object.fromEntries(groups.map(k => [k, d]));

// Key module features, from the Cadova model and the KiCad board.
const SW = i => [9, 18.8 + 19 * i, 6.8];        // top of switch i (0 = SW1)
const PARTS_FRONT = { R1: [7.5, 28.4], R2: [14.45, 47.4], R3: [11.1, 46.9], C1: [7.75, 47.4], R4: [3.8, 65.8], R5: [15.1, 86.8] };
const ROWS = [31.7, 69.7, 88.7];                // connector rows along y
const SCREWS = [[3.2, 3.6], [17.2, 3.6], [3.2, 110], [17.2, 110]];

const half = (n, o = {}) => [
  { model: "terminator", groups: ["terminator", "termHeaders"] },
  ...Array.from({ length: n }, (_, i) => ({ model: KM, pow: i + 1, groups: ["shell", "floor", ...SWITCH, "caps"] })),
  { model: "mcu-module", pow: n, ...o },
];

const footer = code => text(14, 288, code, 2.6, { weight: 400 });

// ---- cover

const cover = {
  bare: true,
  figures: [{ box: { x: 6, y: 52, w: 198, h: 160 }, view: [0.3, -1.0, 1.25], items: half(5) }],
  draw: () => text(14, 32, "BYXBORD", 17) +
    D.person(170, 278, 48, "walk", { extra: D.wornHalves }) +
    footer("AA-2026110-1"),
};

// ---- do and don't

const people = {
  draw: () => [
    D.frame(12, 16, 88, 118), D.frame(110, 16, 88, 118),
    // hunched at a desk
    rect(46, 92, 46, 2.2, { r: 0.5 }), line([50, 94], [50, 122]), line([88, 94], [88, 122]),
    D.laptop(73, 91, 0.5),
    path("M24,96L42,96M30,96L30,122M40,96L40,122M24,96L24,76", { w: 0.7 }),
    D.person(28, 122, 62, "hunch", { mood: "sad" }),
    D.cross(22, 26),
    // walking and typing
    D.person(150, 124, 92, "walk", { extra: D.wornHalves }),
    path("M128,62L118,62M130,72L116,72M128,82L120,82", { w: 0.5 }),
    g(text(0, 0, "{ }", 9, { weight: 400 }), "translate(170 46) rotate(8)"),
    D.tick(120, 26),
    // stuck? write
    D.frame(12, 146, 186, 120),
    D.person(42, 250, 78, "scratch", { mood: "meh" }), D.question(56, 168, 1.2),
    arrow([[72, 206], [90, 206]]),
    D.envelope(105, 206, 1.1),
    arrow([[122, 206], [140, 206]]),
    D.person(168, 250, 78, "cheer"),
  ].join(""),
};

// ---- tools

const tools = {
  draw: () => [
    D.printer(40, 70, 1.05, rect(-5, -14, 10, 4, { w: 0.3 })),
    D.solderingIron(122, 46, 1.25, -25), D.solderSpool(170, 56),
    D.tweezers(102, 98, 1.3, 30),
    D.screwdriver(162, 100, 1.1, -30), g(text(0, 0, "PH0", 3.2), "translate(170 116)"),
    D.syringe(50, 140, 1.25, 20),
    D.multimeter(112, 140, 1.05), D.laptop(166, 152, 1), D.usbC(166, 166, 0.7),
    D.frame(60, 196, 90, 72), D.hammer(105, 230, 1.3, 30), D.cross(70, 206),
  ].join(""),
};

// ---- parts for one key module

const cell = (x, y, w, h) => ({ x, y, w, h });
const inventory = {
  figures: [
    { box: cell(12, 14, 60, 92), view: [0.6, -0.5, 1], up: [0, 1, 0], items: [{ model: KM, groups: ["shell"] }],
      draw: () => qty(14, 22, 1) + D.partNo(42, 104, "100421") },
    { box: cell(76, 14, 60, 92), view: [0.6, -0.5, -1], up: [0, 1, 0], items: [{ model: KM, groups: ["floor"] }],
      draw: () => qty(78, 22, 1) + D.partNo(106, 104, "100422") },
    { box: cell(140, 14, 60, 92), view: [0.4, -0.4, 1], up: [0, 1, 0], items: [{ model: KM, groups: ["pcb"] }],
      draw: () => qty(142, 22, 1) + D.partNo(170, 104, "100165") },
    { box: cell(12, 118, 44, 40), view: [0.8, -0.9, 0.9], items: [{ model: KM, groups: SWITCH, clip: [8, 30] }],
      draw: () => qty(14, 126, 5) + D.partNo(34, 160, "101050") },
    { box: cell(60, 118, 44, 40), view: [0.8, -0.9, 0.9], items: [{ cap: true }],
      draw: () => qty(62, 126, 5) + D.partNo(82, 160, "101051") },
    { box: cell(118, 118, 28, 40), view: [0.7, -0.9, 0.5], items: [{ screw: true }], pad: 0.2,
      draw: () => qty(110, 126, 4) + D.partNo(132, 160, "100205") },
  ],
  draw: () => [
    line([12, 112], [198, 112], { w: 0.25 }),
    rect(160, 124, 30, 26, { r: 1.5, w: 0.25 }), D.screwLife(172, 132), D.lengthMark(177, 133.3, 5),
    line([12, 166], [198, 166], { w: 0.25 }),
    qty(14, 178, 1), D.soic(36, 194, 1.6), D.partNo(36, 214, "100374"),
    qty(62, 178, 5), D.hotswap(84, 194, 1.5), D.partNo(84, 214, "101052"),
    qty(110, 178, 5), D.resistor(134, 194, 2), D.partNo(134, 214, "100103"),
    qty(158, 178, 1), D.resistor(182, 194, 2, false), D.partNo(182, 214, "100104"),
    qty(14, 230, 3), D.connector(42, 248, 1.6), D.partNo(40, 268, "100889"),
    qty(62, 230, 3), D.connector(90, 248, 1.6, true), D.partNo(88, 268, "100791"),
  ].join(""),
};

// ---- 1: print

const print = {
  figures: [
    { box: cell(84, 30, 116, 74), view: [0.8, -1.1, 1.1], items: [{ model: "key-module-print" }], draw: () => qty(86, 40, 5) },
    { box: cell(84, 112, 116, 74), view: [0.7, -1, 1.2], items: [{ model: "solder-jig-print" }], draw: () => qty(86, 122, 1) },
    { box: cell(84, 194, 116, 80), view: [0.8, -1.1, 1.1], items: [{ model: "mcu-module-print" }], draw: () => qty(86, 204, 1) },
  ],
  draw: () => stepNo(1) + D.printer(40, 168, 1.3, rect(-6, -14, 12, 4, { w: 0.3 })) +
    arrow([[64, 100], [80, 70]]) + arrow([[66, 140], [80, 146]]) + arrow([[64, 166], [80, 226]]),
};

// ---- 2: front of the board

const front = {
  figures: [{
    box: cell(36, 26, 110, 250), view: [0.15, -0.25, 1], up: [0, 1, 0], items: [{ model: KM, groups: ["pcb"] }],
    draw: P => {
      const z = 1.6, out = [];
      for (const [k, [x, y]] of Object.entries(PARTS_FRONT)) {
        const p = P([x, y, z]), c = k === "C1";
        const side = x < 10 ? -1 : 1, o = [p[0] + side * 34, p[1] + (k === "R3" ? 10 : k === "C1" ? -10 : 0)];
        out.push(D.callout(o[0], o[1], 9, D.resistor(0, 0, 2.2, !c), p));
      }
      return out.join("");
    },
  }],
  draw: () => stepNo(2) + qty(160, 34, 5) + D.resistor(178, 30, 1.6) + qty(160, 50, 1) + D.resistor(178, 46, 1.6, false) +
    D.tweezers(186, 140, 1.2, -60) + D.solderingIron(178, 230, 1.1, -70),
};

// ---- 3: the jig: U1, hotswap sockets, then the connectors, headers at 8 degrees

const jig = {
  figures: [
    { box: cell(10, 30, 164, 112), view: [0.9, -0.8, 1.3], items: [{ model: "solder-jig", explode: explode(BOARD, [0, 0, 22]) }],
      draw: P => arrow([P([2, -12, 24]), P([2, -12, 10])], { w: 0.9 }) + arrow([P([2, -100, 24]), P([2, -100, 10])], { w: 0.9 }) },
    { box: cell(10, 172, 120, 100), view: [0.9, -0.8, 1.3], items: [{ model: "solder-jig" }],
      draw: P => D.callout(164, 236, 32, header8(), P([22, -ROWS[1], 7])) },
  ],
  draw: () => stepNo(3) + D.solderingIron(188, 84, 1, -75) +
    line([12, 150], [198, 150], { w: 0.25 }) +
    qty(14, 162, 1) + D.soic(34, 160, 1) + qty(52, 162, 5) + D.hotswap(72, 160, 1) +
    qty(92, 162, 3) + D.connector(112, 160, 0.9, true) + qty(130, 162, 3) + D.connector(150, 160, 0.9),
};

// The header at 8 degrees in the jig's cradle, seen from the end: body and pin tilted off the board's plane.
function header8() {
  const a = -8, r = a * Math.PI / 180;
  return line([-30, 10], [30, 10], { w: 1.2 }) + line([-30, 12], [30, 12], { w: 0.4 }) +
    g(rect(-20, -1, 14, 8, { r: 0.6 }) + rect(-6, 1.6, 30, 2.4, { w: 0.5 }), `translate(0 2) rotate(${a} -20 7)`) +
    line([-20, 9], [26, 9], { w: 0.3, dash: "1.4 1" }) +
    path(`M14,9A34,34 0 0 0 ${-20 + 34 * Math.cos(r)},${9 + 34 * Math.sin(r)}`, { w: 0.4 }) +
    text(4, 24, "8°", 6);
}

// ---- 4: glue and wait

const glue = {
  figures: [
    { box: cell(10, 30, 120, 150), view: [0.3, -0.4, -1], up: [0, 1, 0], items: [{ model: KM, groups: BOARD }],
      draw: P => D.callout(160, 70, 30, bead(true), P([21, ROWS[2], -2])) + D.callout(160, 146, 26, bead(false), P([4, ROWS[1], -2])) },
  ],
  draw: () => stepNo(4) + D.syringe(70, 210, 1.5, -15) + qty(14, 196, 6) + D.clock(150, 236, 1.6) +
    text(166, 264, "24h", 6),
};

// A connector body on the board, end view: a glue bead along its foot, or (wrong) glue on the tails.
function bead(good) {
  const blob = (x) => path(`M${x - 3},6C${x - 3},2 ${x + 3},2 ${x + 3},6Z`, { w: 0.3, fill: "#000" });
  return rect(-40, 6, 80, 4, { w: 0.6 }) + rect(-10, -14, 20, 20, { r: 1 }) +
    line([10, 2], [26, 2], { w: 1 }) + line([-10, 2], [-26, 2], { w: 1 }) +
    (good ? blob(-12) + blob(12) : path("M17,-1C17,-5 27,-5 27,-1L27,6L17,6Z", { w: 0.3, fill: "#000" })) +
    (good ? D.tick(0, -22, 0.9) : D.cross(0, -18, 0.9));
}

// ---- 5: board into the upturned shell, floor on, four screws

const closeUp = {
  figures: [{
    box: cell(8, 30, 194, 200), view: [0.9, -0.9, 1.15],
    items: [
      { model: KM, rot: UP, groups: ["shell", "floor", ...BOARD], explode: { ...explode(BOARD, [0, 0, 22]), floor: [0, 0, 44] } },
      ...SCREWS.map(([x, y]) => ({ screw: true, at: [32 - x, y, 64] })),
    ],
    draw: P => [
      arrow([P(add(up([10, 18, 0]), [0, 0, 18])), P(add(up([10, 18, 0]), [0, 0, 6]))], { w: 0.9 }),
      arrow([P(add(up([10, 96, 0]), [0, 0, 18])), P(add(up([10, 96, 0]), [0, 0, 6]))], { w: 0.9 }),
      arrow([P([16, 56, 40]), P([16, 56, 30])], { w: 0.9 }),
      ...SCREWS.map(([x, y]) => line(P([32 - x, y, 58]), P([32 - x, y, 50]), { w: 0.35, dash: "1.2 1" })),
    ].join(""),
  }],
  draw: () => stepNo(5) + qty(150, 238, 4) + D.screwLife(166, 232) + D.screwdriver(60, 256, 1.6, -15),
};

// ---- 6: turn over, switches, keycaps; five times

const keys = {
  figures: [
    { box: cell(30, 26, 60, 60), view: [0.9, -0.9, 1.15], items: [{ model: KM, rot: UP, groups: ["shell", "floor"] }] },
    { box: cell(110, 26, 60, 60), view: [0.9, -0.9, 1.15], items: [{ model: KM, groups: ["shell", "floor"] }] },
    { box: cell(8, 92, 150, 150), view: [0.9, -0.9, 1.15],
      items: [{ model: KM, groups: ["shell", "floor", ...SWITCH, "caps"], explode: { ...explode(SWITCH, [0, 0, 16]), caps: [0, 0, 32] } }],
      draw: P => [0, 4].map(i => arrow([P(add(SW(i), [0, 0, 8])), P(add(SW(i), [0, 0, 1]))], { w: 0.8 })).join("") +
        D.callout(168, 130, 22, clickDetail(), P(add(SW(2), [0, 0, 14]))) },
    { box: cell(120, 220, 80, 60), view: [0.9, -0.9, 1.15], items: [{ model: KM, groups: ["shell", "floor", ...SWITCH, "caps"] }] },
  ],
  draw: () => stepNo(6) + D.turn(100, 56, 9, 9, 200, 340, { w: 0.8 }) + arrow([[92, 56], [108, 56]], { w: 0.5, dash: "1.5 1.2" }) +
    qty(14, 104, 5) + text(108, 262, "5x", 9),
};

// A switch's side clip catching under the plate.
function clickDetail() {
  return rect(-30, -2, 60, 4, { w: 0.6 }) + rect(-10, -14, 20, 20, { r: 1 }) + rect(-4, -18, 8, 4) +
    poly([[10, -2], [13, 2], [10, 4]]) + poly([[-10, -2], [-13, 2], [-10, 4]]) + D.clicks(14, -8, 0.9, -30);
}

// ---- 7: the terminator: one wire, two headers into a paused print

const terminator = {
  figures: [
    { box: cell(84, 140, 116, 92), view: [1.1, -0.9, 1.2],
      items: [{ model: "terminator-print", explode: { termHeaders: [0, 0, 18] } }],
      draw: P => arrow([P([8, 34, 22]), P([8, 34, 10])], { w: 0.9 }) + dimension(P([-10.9, 2, 0]), P([-10.9, 2, 7.4]), "7.4") },
  ],
  draw: () => stepNo(7) + [
    qty(14, 40, 2), D.connector(48, 62, 2.2), D.connector(96, 62, 2.2),
    path("M53,70C60,90 92,92 108,66", { w: 0.9 }), circle([53, 70], 1.6), circle([108, 66], 1.6),
    D.solderingIron(150, 60, 1.1, -30),
    D.multimeter(160, 110, 0.9), D.tick(186, 96, 0.7),
    line([12, 132], [198, 132], { w: 0.25 }),
    D.printer(44, 220, 1.3), circle([44, 248], 7, { w: 0.6 }), rect(40.6, 244, 2.4, 8, { w: 0, fill: "#000" }), rect(45, 244, 2.4, 8, { w: 0, fill: "#000" }),
    D.syringe(150, 260, 0.9, -10),
  ].join(""),
};

// ---- 8 and 9: the MCU module: wiring, then assembly

const mcuWire = () => [
  rect(16, 40, 34, 80, { r: 1 }), ...[0, 1, 2].map(i => rect(16, 52 + i * 22, 8, 10, { r: 0.4 })),
  rect(120, 44, 30, 60, { r: 2 }), rect(128, 40, 14, 6, { r: 2 }), dot([135, 52], 0.7), dot([135, 96], 0.7),
  rect(64, 104, 36, 24, { r: 1 }), rect(68, 108, 28, 16, { w: 0.3 }), D.statusScreen(82, 116, 1.1),
  ...[0, 1, 2].flatMap(r => [0, 1, 2].map(p => path(`M24,${55 + r * 22 + p * 2.6}C60,${55 + r * 22 + p * 2.6} 90,${60 + r * 10 + p * 5} 120,${60 + r * 10 + p * 5}`, { w: 0.35 }))).filter((_, i) => i !== 0),
  ...[0, 1, 2, 3, 4].map(i => path(`M${70 + i * 5},104C${70 + i * 5},90 ${110},${86 - i * 2} 120,${86 - i * 2}`, { w: 0.35 })),
  D.battery(70, 150, 1.2), path("M47,159C60,170 100,170 118,104", { w: 0.45 }),
  rect(150, 118, 10, 6, { r: 0.6 }), rect(153, 115, 4, 3), rect(150, 132, 8, 8, { r: 1 }), circle([154, 136], 2.2),
  path("M150,121C140,121 140,104 140,104M150,136C136,136 132,104 132,104", { w: 0.35 }),
  D.solderingIron(186, 140, 1, -60),
].join("");

const mcuWiring = {
  draw: () => stepNo(8) + mcuWire() + line([12, 186], [198, 186], { w: 0.25 }) +
    D.battery(56, 236, 1.6) + D.multimeter(140, 232, 1.1) +
    path("M23,250C40,262 100,268 124,256", { w: 0.3, dash: "1.2 1" }) + text(16, 262, "+", 7) + text(16, 274, "−", 7) +
    D.tick(176, 214, 0.8),
};

const MCU_PARTS = { view: [0, 0, 24], socketBoard: [0, 0, 50], sockets: [0, 0, 50], tails: [0, 0, 50],
                    battery: [0, 0, 82], nano: [0, 0, 82], switch: [0, 0, 82], reset: [0, 0, 82] };
const mcu = {
  figures: [
    { box: cell(10, 28, 190, 160), view: [0.3, -1.1, 0.45],
      items: [{ model: "mcu-module", rot: UP, pivot: [52, 56.8, 0], groups: ["shell", ...Object.keys(MCU_PARTS)], explode: MCU_PARTS }],
      draw: P => [[[73, 57], 20, 8], [[78, 100], 46, 30], [[49, 30], 76, 50], [[49, 96], 76, 50]]
        .map(([[x, y], z1, z0]) => arrow([P([x, y, z1]), P([x, y, z0])], { w: 0.7 })).join("") },
    { box: cell(10, 196, 150, 92), view: [0.5, -0.9, 1.2],
      items: [{ model: "mcu-module", rot: UP, pivot: [52, 56.8, 0], explode: { jack: [0, 0, 40], floor: [0, 0, 40] } }],
      draw: P => arrow([P([52, 56, 36]), P([52, 56, 26])], { w: 0.9 }) },
  ],
  draw: () => stepNo(9) + line([12, 192], [198, 192], { w: 0.25 }) + qty(162, 214, 6) + D.screwLife(178, 208) +
    D.screwdriver(178, 252, 1.1, -60),
};

// A dimension line between two sheet points, with its value beside it.
function dimension(a, b, v) {
  const o = [-5, 0], a2 = add(a, o), b2 = add(b, o);
  return line(a, add(a2, [-1.5, 0]), { w: 0.2 }) + line(b, add(b2, [-1.5, 0]), { w: 0.2 }) +
    arrow([mid(a2, b2), b2], { w: 0.3, head: 1.6 }) + arrow([mid(a2, b2), a2], { w: 0.3, head: 1.6 }) +
    text(a2[0] - 2, mid(a2, b2)[1] + 1.5, v, 4, { anchor: "end" });
}

// ---- 10: join a half, from the terminator to the MCU module

const join = {
  figures: [
    { box: cell(10, 30, 120, 80), view: [0.35, -1.2, 0.9],
      items: [{ model: "terminator", groups: ["terminator", "termHeaders"] }, { model: KM, pow: 1, at: [22, 0, 0], groups: ["shell", "floor", ...SWITCH, "caps"] }],
      draw: P => arrow([P([26, 118, 2], 1), P([10, 118, 2], 1)], { w: 0.9 }) },
    { box: cell(10, 116, 120, 80), view: [0.35, -1.2, 0.9],
      items: [{ model: KM, groups: ["shell", "floor", ...SWITCH, "caps"] }, { model: KM, pow: 1, at: [22, 0, 0], groups: ["shell", "floor", ...SWITCH, "caps"] }],
      draw: P => arrow([P([26, 118, 2], 1), P([10, 118, 2], 1)], { w: 0.9 }) },
    { box: cell(10, 200, 190, 86), view: [0.3, -1.0, 1.25], items: half(5) },
  ],
  draw: () => stepNo(10) + D.callout(166, 86, 26, clickDetail(), null) + text(140, 160, "4x", 8),
};

// ---- 11: flash

const flash = {
  figures: [{ box: cell(100, 46, 100, 100), view: [0.2, -0.4, 1], up: [0, 1, 0], items: [{ model: "mcu-module", pow: 0 }],
              draw: P => {
                const port = add(P([55, 114, -8]), [0, -1]), end = [port[0], port[1] - 12];
                return rect(port[0] - 2.4, port[1] - 12, 4.8, 9, { r: 1 }) + rect(port[0] - 1.4, port[1] - 3, 2.8, 3, { r: 1 }) +
                  path(`M${end}C${end[0]},${end[1] - 16} 96,26 84,60S78,104 72,110`, { w: 0.9 }) +
                  D.finger(...add(P([83, 73, -12]), [6, 0]), 1, 180) + D.taps(...add(P([83, 73, -12]), [8, -4]), 1);
              } }],
  draw: () => stepNo(11) + D.laptop(50, 110, 1.4) +
    text(184, 104, "2x", 6) +
    line([12, 160], [198, 160], { w: 0.25 }) +
    D.laptop(105, 236, 2.2, D.drive(10, 0, 0.8) + D.file(-14, -2, 0.8) + arrow([[-8, 0], [0, 0]], { w: 0.5, head: 2 })),
};

// ---- 12: on, pair, wear, walk

const bluetooth = (x, y, s = 1) => path("M-3,-4L3,2L0,5L0,-5L3,-2L-3,4", { w: 0.7 }).replace("<path", `<path transform="translate(${x} ${y}) scale(${s})"`);

const wear = {
  figures: [{ box: cell(10, 28, 110, 80), view: [0.3, -1.0, 1.25], items: half(5) }],
  draw: () => stepNo(12) + D.statusScreen(164, 52, 2.4) + path("M112,62L134,56", { w: 0.4, dash: "1.5 1" }) +
    D.laptop(164, 112, 1) + bluetooth(164, 100, 1.1) + path("M128,88C136,96 142,100 146,102", { w: 0.4, dash: "1.5 1" }) +
    line([12, 128], [198, 128], { w: 0.25 }) +
    D.person(84, 282, 140, "walk", { extra: D.wornHalves }) +
    path("M46,170L30,170M44,190L24,190M48,210L32,210", { w: 0.6 }) +
    D.heart(150, 162, 1.6),
};

const back = {
  bare: true,
  draw: () => D.person(105, 220, 130, "walk", { extra: D.wornHalves }) + D.heart(140, 100, 1.4) +
    text(14, 280, "BYXBORD", 6) + footer("AA-2026110-1").replace('y="288"', 'y="286"'),
};

export const SHEETS = [cover, people, tools, inventory, print, front, jig, glue, closeUp, keys, terminator, mcuWiring, mcu, join, flash, wear, back];
