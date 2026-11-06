// 2D drawing kit for the sheets: SVG strings in sheet mm. Line weights follow the rendered figures.

export const LW = 0.42;
const f = n => +n.toFixed(2);
const pt = ([x, y]) => `${f(x)},${f(y)}`;

export const g = (body, t = "") => `<g${t ? ` transform="${t}"` : ""}>${body}</g>`;
export const at = (x, y, s = 1, rot = 0) => `translate(${f(x)} ${f(y)})${rot ? ` rotate(${rot})` : ""}${s !== 1 ? ` scale(${s})` : ""}`;
export const path = (d, { w = LW, fill = "none", dash } = {}) =>
  `<path d="${d}" fill="${fill}" stroke="#000" stroke-width="${w}" stroke-linecap="round" stroke-linejoin="round"${dash ? ` stroke-dasharray="${dash}"` : ""}/>`;
export const line = (a, b, o) => path(`M${pt(a)}L${pt(b)}`, o);
export const poly = (pts, o) => path(`M${pts.map(pt).join("L")}Z`, { fill: "#fff", ...o });
export const circle = (c, r, { w = LW, fill = "#fff" } = {}) =>
  `<circle cx="${f(c[0])}" cy="${f(c[1])}" r="${f(r)}" fill="${fill}" stroke="#000" stroke-width="${w}"/>`;
export const dot = (c, r = 0.5) => `<circle cx="${f(c[0])}" cy="${f(c[1])}" r="${r}"/>`;
export const rect = (x, y, w, h, { r = 0, ...o } = {}) =>
  `<rect x="${f(x)}" y="${f(y)}" width="${f(w)}" height="${f(h)}" rx="${r}" fill="${o.fill ?? "#fff"}" stroke="#000" stroke-width="${o.w ?? LW}"${o.dash ? ` stroke-dasharray="${o.dash}"` : ""}/>`;
export const text = (x, y, s, size = 5, { anchor = "start", weight = 700 } = {}) =>
  `<text x="${f(x)}" y="${f(y)}" font-size="${size}" font-weight="${weight}" text-anchor="${anchor}">${s}</text>`;

// ---- page furniture

export const stepNo = n => text(12, 25, n, 15);
export const qty = (x, y, n, size = 5.5) => text(x, y, `${n}x`, size);
export const partNo = (x, y, n) => text(x, y, n, 2.6, { anchor: "middle", weight: 400 });
export const frame = (x, y, w, h) => rect(x, y, w, h, { r: 3, w: 0.35 });

// An arrow along a polyline, or a quadratic curve through three points with `curve`.
export function arrow(pts, { w = 0.7, head = 3.2, curve = false, dash } = {}) {
  const [a, b] = [pts.at(-2), pts.at(-1)];
  const ang = Math.atan2(b[1] - a[1], b[0] - a[0]);
  const back = [b[0] - Math.cos(ang) * head * 0.8, b[1] - Math.sin(ang) * head * 0.8];
  const body = [...pts.slice(0, -1), back];
  const d = curve && pts.length === 3 ? `M${pt(body[0])}Q${pt(body[1])} ${pt(body[2])}` : `M${body.map(pt).join("L")}`;
  const side = s => [b[0] - Math.cos(ang) * head + Math.cos(ang + s * Math.PI / 2) * head * 0.45,
                     b[1] - Math.sin(ang) * head + Math.sin(ang + s * Math.PI / 2) * head * 0.45];
  return path(d, { w, dash }) + `<path d="M${pt(b)}L${pt(side(1))}L${pt(side(-1))}Z"/>`;
}

// A circular arrow around (cx, cy), from angle a0 to a1 in degrees, clockwise when a1 > a0.
export function turn(cx, cy, rx, ry, a0, a1, o = {}) {
  const pts = [];
  for (let i = 0; i <= 24; i++) {
    const a = (a0 + (a1 - a0) * i / 24) * Math.PI / 180;
    pts.push([cx + rx * Math.cos(a), cy + ry * Math.sin(a)]);
  }
  return arrow(pts, o);
}

// Magnified detail: a circle holding `inner` (drawn in the circle's own coordinates, centred on 0,0, radius r),
// joined to the spot it magnifies.
let clipN = 0;
export function callout(cx, cy, r, inner, target) {
  const id = `c${clipN++}`;
  const link = target ? line(target, [cx + (target[0] - cx) * r / Math.hypot(target[0] - cx, target[1] - cy),
                                      cy + (target[1] - cy) * r / Math.hypot(target[0] - cx, target[1] - cy)], { w: 0.3 }) +
                        circle(target, 1.6, { w: 0.3, fill: "none" }) : "";
  return link + `<clipPath id="${id}"><circle cx="${cx}" cy="${cy}" r="${r}"/></clipPath>` +
    circle([cx, cy], r, { w: 0.6 }) + `<g clip-path="url(#${id})"><g transform="translate(${cx} ${cy})">${inner}</g></g>` +
    circle([cx, cy], r, { w: 0.6, fill: "none" });
}

export const tick = (x, y, s = 1) => g(circle([0, 0], 5, { w: 0.6 }) + path("M-2.6,0.2L-0.7,2.2L2.8,-2.3", { w: 1.1 }), at(x, y, s));
export const cross = (x, y, s = 1) => g(circle([0, 0], 5, { w: 0.6 }) + path("M-2.3,-2.3L2.3,2.3M2.3,-2.3L-2.3,2.3", { w: 1.1 }), at(x, y, s));
export const clicks = (x, y, s = 1, rot = 0) =>
  g([-40, 0, 40].map(a => g(line([4, 0], [7.5, 0], { w: 0.5 }), `rotate(${a})`)).join(""), at(x, y, s, rot));
export const question = (x, y, s = 1) => g(path("M-3,-4C-3,-9 4,-9 4,-4C4,-1 0,-1 0,2.5", { w: 1.6 }) + dot([0, 6], 1.1), at(x, y, s));

// ---- people. Joints in units of body height, origin between the feet, y down.

const tube = (pts, w, s) => path(`M${pts.map(([x, y]) => pt([x * s, y * s])).join("L")}`, { w: w * s + 1.1 }) +
  `<path d="M${pts.map(([x, y]) => pt([x * s, y * s])).join("L")}" fill="none" stroke="#fff" stroke-width="${f(w * s)}" stroke-linecap="round" stroke-linejoin="round"/>`;

const POSES = {
  stand: { head: [0, -0.885], neck: [0, -0.77], hip: [0, -0.47],
           armB: [[0.03, -0.75], [0.1, -0.6], [0.12, -0.46]], armF: [[-0.03, -0.75], [-0.1, -0.6], [-0.12, -0.46]],
           legB: [[0.035, -0.47], [0.05, -0.23], [0.06, 0]], legF: [[-0.035, -0.47], [-0.05, -0.23], [-0.06, 0]] },
  scratch: { head: [0, -0.885], neck: [0, -0.77], hip: [0, -0.47],
           armB: [[0.03, -0.75], [0.1, -0.6], [0.12, -0.46]], armF: [[-0.03, -0.75], [-0.17, -0.8], [-0.08, -0.93]],
           legB: [[0.035, -0.47], [0.05, -0.23], [0.06, 0]], legF: [[-0.035, -0.47], [-0.05, -0.23], [-0.06, 0]] },
  cheer: { head: [0, -0.885], neck: [0, -0.77], hip: [0, -0.47],
           armB: [[0.03, -0.75], [0.17, -0.88], [0.2, -1.04]], armF: [[-0.03, -0.75], [-0.17, -0.88], [-0.2, -1.04]],
           legB: [[0.035, -0.47], [0.08, -0.23], [0.12, 0]], legF: [[-0.035, -0.47], [-0.08, -0.23], [-0.12, 0]] },
  walk:  { head: [0.035, -0.885], neck: [0.02, -0.77], hip: [0, -0.47],
           armB: [[0.0, -0.74], [-0.05, -0.57], [-0.035, -0.37]], armF: [[0.03, -0.74], [0.08, -0.57], [0.075, -0.37]],
           legB: [[-0.01, -0.47], [-0.07, -0.25], [-0.17, -0.04]], legF: [[0.01, -0.47], [0.1, -0.25], [0.15, 0]] },
  hunch: { head: [0.21, -0.63], neck: [0.13, -0.6], hip: [0, -0.3],
           armB: [[0.12, -0.58], [0.2, -0.43], [0.36, -0.44]], armF: [[0.14, -0.58], [0.23, -0.44], [0.38, -0.45]],
           legB: [[0, -0.3], [0.24, -0.3], [0.25, 0]], legF: [[0.02, -0.3], [0.26, -0.31], [0.28, 0]] },
};

// mood: "happy", "sad", "meh"; extra(s, P) adds things drawn between the torso and the front limbs, in mm.
export function person(x, y, height, pose = "stand", { mood = "happy", flip = false, extra } = {}) {
  const P = POSES[pose], s = height, lw = 0.065;
  const face = { happy: "M-0.03,0.02Q0,0.05 0.03,0.02", sad: "M-0.03,0.04Q0,0.01 0.03,0.04", meh: "M-0.025,0.035L0.025,0.03" }[mood];
  const [hx, hy] = P.head;
  const head = circle([hx * s, hy * s], 0.1 * s, { w: 0.55 }) +
    dot([(hx - 0.033) * s, (hy - 0.012) * s], 0.012 * s) + dot([(hx + 0.033) * s, (hy - 0.012) * s], 0.012 * s) +
    path(face.replace(/(-?[\d.]+),(-?[\d.]+)/g, (_, a, b) => `${f((+a + hx) * s)},${f((+b + hy) * s)}`), { w: 0.5 }) +
    path(`M${pt([(hx - 0.085) * s, (hy - 0.05) * s])}Q${pt([(hx - 0.02) * s, (hy - 0.15) * s])} ${pt([(hx + 0.08) * s, (hy - 0.06) * s])}`, { w: 0.5 });
  const body = tube(P.legB, lw, s) + tube(P.armB, lw * 0.9, s) + tube([P.neck, P.hip], 0.15, s) +
    (extra ? extra(s, P) : "") + tube(P.legF, lw, s) + tube(P.armF, lw * 0.9, s) + head;
  return g(body, `${at(x, y)}${flip ? " scale(-1 1)" : ""}`);
}

// Two halves on the thighs: a band of modules wrapped around each upper leg, with a strap at each end.
export const wornHalves = (s, P) => [P.legB, P.legF].map(([h, k]) => {
  const ang = Math.atan2(k[1] - h[1], k[0] - h[0]) * 180 / Math.PI;
  const len = 0.14 * s, wid = 0.095 * s;
  const stripes = [0.3, 0.55, 0.8].map(t => line([0, -wid / 2 + wid * t], [len, -wid / 2 + wid * t], { w: 0.3 })).join("");
  const straps = [0.08, 0.92].map(t => rect(len * t - 0.9, -wid / 2 - 1.2, 1.8, wid + 2.4, { r: 0.6, w: 0.35 })).join("");
  return g(rect(0, -wid / 2, len, wid, { r: 1.2, w: 0.45 }) + stripes + straps,
    `translate(${f(h[0] * s + (k[0] - h[0]) * s * 0.18)} ${f(h[1] * s + (k[1] - h[1]) * s * 0.18)}) rotate(${f(ang)})`);
}).join("");

// ---- tools and things, each drawn around its own origin, about 30 mm long at s = 1

export const solderingIron = (x, y, s = 1, rot = 0) => g(
  path("M-26,-1C-34,-1 -36,8 -44,10", { w: 0.5 }) +
  rect(-26, -3.2, 20, 6.4, { r: 3 }) + [-20, -17, -14].map(u => line([u, -3.1], [u, 3.1], { w: 0.3 })).join("") +
  poly([[-6, -2.2], [-2, -1], [-2, 1], [-6, 2.2]]) + rect(-2, -0.7, 14, 1.4) + poly([[12, -0.7], [17, 0], [12, 0.7]]),
  at(x, y, s, rot));

export const solderSpool = (x, y, s = 1) => g(
  circle([0, 0], 8) + circle([0, 0], 6.3, { w: 0.3 }) + circle([0, 0], 2.2) +
  path("M6.6,3C11,6 12,11 16,12", { w: 0.6 }), at(x, y, s));

export const tweezers = (x, y, s = 1, rot = 0) => g(
  poly([[-14, -1.4], [12, -0.3], [13, -0.1], [-14, -0.4]]) + poly([[-14, 1.4], [12, 0.3], [13, 0.1], [-14, 0.4]]) +
  path("M-14,-1.4Q-17,0 -14,1.4", { w: LW }), at(x, y, s, rot));

export const screwdriver = (x, y, s = 1, rot = 0) => g(
  rect(-22, -3.4, 15, 6.8, { r: 2.5 }) + [-18.5, -15, -11.5].map(u => line([u, -2.4], [u, 2.4], { w: 0.3 })).join("") +
  poly([[-7, -2.4], [-4, -1], [-4, 1], [-7, 2.4]]) + rect(-4, -0.7, 16, 1.4) + poly([[12, -0.7], [15, -0.25], [15, 0.25], [12, 0.7]]),
  at(x, y, s, rot));

export const syringe = (x, y, s = 1, rot = 0) => g(
  rect(-14, -3.2, 18, 3, { r: 0.6 }) + rect(-14, 0.2, 18, 3, { r: 0.6 }) + rect(-16, -3.6, 2, 7.2) +
  rect(-24, -2.4, 8, 1.6) + rect(-24, 0.9, 8, 1.6) + rect(-26, -3.2, 2, 6.4) +
  poly([[4, -3.2], [7, -1.2], [7, 1.2], [4, 3.2]]) + poly([[7, -1], [17, -0.4], [17, 0.4], [7, 1]]) +
  [9, 11, 13, 15].map(u => line([u, -0.8], [u, 0.8], { w: 0.25 })).join(""),
  at(x, y, s, rot));

export const printer = (x, y, s = 1, inner = "") => g(
  rect(-18, -40, 36, 40, { r: 1 }) + rect(-15, -36, 30, 30, { w: 0.3 }) +
  line([-15, -31], [15, -31]) + rect(-4, -34, 8, 6, { r: 0.5 }) + poly([[-1.2, -28], [1.2, -28], [0, -26.2]]) +
  rect(-12, -10, 24, 1.6) + inner + rect(-18, -4, 36, 4, { r: 0.5 }) + rect(10, -3.2, 5, 2.4, { r: 0.4, w: 0.3 }),
  at(x, y, s));

export const multimeter = (x, y, s = 1) => g(
  rect(-9, -16, 18, 30, { r: 2.5 }) + rect(-6.5, -13, 13, 7, { r: 0.8 }) + circle([0, 2.5], 4.5) + line([0, 2.5], [2.2, -0.8], { w: 0.7 }) +
  circle([-4.5, 10.5], 1.1) + circle([4.5, 10.5], 1.1) +
  path("M-4.5,11.6C-6,20 -14,16 -16,22", { w: 0.6 }) + path("M4.5,11.6C6,20 14,16 16,22", { w: 0.6 }) +
  g(rect(-1, 0, 2, 7, { r: 0.6 }) + line([0, 7], [0, 10.5], { w: 0.5 }), "translate(-16 22) rotate(-160)") +
  g(rect(-1, 0, 2, 7, { r: 0.6 }) + line([0, 7], [0, 10.5], { w: 0.5 }), "translate(16 22) rotate(160)"),
  at(x, y, s));

export const hammer = (x, y, s = 1, rot = 0) => g(
  rect(-2, -6, 4, 30, { r: 1.5 }) + poly([[-10, -11], [8, -11], [12, -9], [12, -5], [8, -3], [-10, -3]]) +
  path("M-10,-11Q-14,-7 -10,-3", { w: LW }), at(x, y, s, rot));

export const laptop = (x, y, s = 1, screen = "") => g(
  poly([[-16, -22], [16, -22], [16, -1], [-16, -1]]) + rect(-14, -20, 28, 17, { w: 0.3 }) + g(screen, "translate(0 -11.5)") +
  poly([[-19, 0], [19, 0], [21, 3], [-21, 3]]) + line([-4, 1.5], [4, 1.5], { w: 0.3 }),
  at(x, y, s));

export const clock = (x, y, s = 1) => g(
  circle([0, 0], 9, { w: 0.7 }) + Array.from({ length: 12 }, (_, i) => g(line([0, -7.6], [0, -6.4], { w: 0.4 }), `rotate(${i * 30})`)).join("") +
  line([0, 0], [0, -5.4], { w: 0.7 }) + line([0, 0], [3.6, 1.4], { w: 0.7 }) + dot([0, 0], 0.7) +
  turn(0, 0, 12, 12, -80, 200, { w: 0.5, head: 2.4 }), at(x, y, s));

export const envelope = (x, y, s = 1) => g(
  rect(-12, -8, 24, 16, { r: 0.8 }) + path("M-12,-8L0,2L12,-8") + path("M-12,8L-3,-0.5M12,8L3,-0.5", { w: 0.3 }),
  at(x, y, s));

export const usbC = (x, y, s = 1, rot = 0) => g(
  path("M-30,0C-22,0 -24,0 -12,0", { w: 0.9 }) + rect(-12, -2.4, 9, 4.8, { r: 1 }) + rect(-3, -1.4, 4, 2.8, { r: 1.2 }),
  at(x, y, s, rot));

export const heart = (x, y, s = 1) => g(path("M0,3C-6,-1 -5,-6 -2.5,-6C-1.2,-6 0,-5 0,-3.6C0,-5 1.2,-6 2.5,-6C5,-6 6,-1 0,3Z", { w: 0.5, fill: "#fff" }), at(x, y, s));

// A pointing finger coming in from the left, tip at the origin.
export const finger = (x, y, s = 1, rot = 0) => g(
  path("M-30,-4.5L-3,-3.6C1,-3.4 1.6,3.4 -3,3.6L-30,4.5", { w: 0.55, fill: "#fff" }) +
  path("M-5.6,-2.4C-3.4,-2.4 -2.2,-1.6 -2,0C-2.2,1.6 -3.4,2.4 -5.6,2.4Z", { w: 0.35, fill: "#fff" }) +
  path("M-14,-3.9Q-15,0 -14,3.9", { w: 0.3 }),
  at(x, y, s, rot));

export const taps = (x, y, s = 1) => g([4, 6.5].map(r => path(`M${-r},${-r * 0.5}A${r},${r} 0 0 1 ${r},${-r * 0.5}`, { w: 0.4 })).join(""), at(x, y, s));

// ---- loose parts, drawn small and roughly isometric

export const resistor = (x, y, s = 1, mark = true) => g(
  poly([[-3, 0], [1, -1.6], [4, -0.2], [0, 1.4]]) + poly([[-3, 0], [0, 1.4], [0, 2.2], [-3, 0.8]]) + poly([[0, 1.4], [4, -0.2], [4, 0.6], [0, 2.2]]) +
  (mark ? line([-1.6, -0.1], [2, -1.1], { w: 0.25 }) + line([-0.7, 0.3], [2.9, -0.7], { w: 0.25 }) : ""),
  at(x, y, s));

export const soic = (x, y, s = 1) => {
  const pins = [];
  for (let i = 0; i < 8; i++) {
    const t = i * 1.27;
    pins.push(poly([[-5 + t * 0.8 + 0.2, 2.6 + t * 0.4 - 0.2], [-5 + t * 0.8 + 0.7, 2.6 + t * 0.4 + 0.05], [-5 + t * 0.8 - 0.3, 3.6 + t * 0.4], [-5 + t * 0.8 - 0.8, 3.35 + t * 0.4]], { w: 0.2 }));
  }
  return g(pins.join("") +
    poly([[-6, 2], [2, 6], [6, 3], [-2, -1]]) + poly([[-6, 2], [2, 6], [2, 7], [-6, 3]]) + poly([[2, 6], [6, 3], [6, 4], [2, 7]]) +
    circle([-3.5, 1.4], 0.5, { w: 0.25 }), at(x, y, s));
};

// A three-pin 2 mm header or socket: `female` draws the socket.
export const connector = (x, y, s = 1, female = false) => {
  const face = [0.2, 0.5, 0.8].map(t => [-1 + t * 8, 3.5 - t * 4]);
  const pins = female ? face.map(p => circle(p, 0.6, { w: 0.25, fill: "#000" })).join("")
                      : face.map(([px, py]) => line([px, py], [px + 3.2, py + 2.6], { w: 0.9 })).join("");
  return g(poly([[-5, 0], [3, -4], [7, -2], [-1, 2]]) + poly([[-5, 0], [-1, 2], [-1, 5], [-5, 3]]) +
           poly([[-1, 2], [7, -2], [7, 1], [-1, 5]]) + pins, at(x, y, s));
};

export const hotswap = (x, y, s = 1) => g(
  poly([[-5, 0], [1, -3], [6, -0.5], [0, 2.5]]) + poly([[-5, 0], [0, 2.5], [0, 4], [-5, 1.5]]) + poly([[0, 2.5], [6, -0.5], [6, 1], [0, 4]]) +
  poly([[-7, 0.3], [-5, -0.7], [-5, 0.3], [-7, 1.3]], { w: 0.25 }) + poly([[6, -0.5], [8, -1.5], [8, -0.5], [6, 0.5]], { w: 0.25 }) +
  circle([-1.5, 0], 0.7, { w: 0.25 }) + circle([2.5, -1.4], 0.7, { w: 0.25 }), at(x, y, s));

// An M2 x 5 screw at 1:1, point down.
export const screwLife = (x, y) => g(
  rect(-1.9, 0, 3.8, 1.3, { r: 0.5, w: 0.25 }) + poly([[-1, 1.3], [1, 1.3], [1, 5.4], [0, 6.3], [-1, 5.4]], { w: 0.25 }) +
  [2, 2.8, 3.6, 4.4, 5.2].map(u => line([-1, u], [1, u - 0.4], { w: 0.15 })).join(""),
  at(x, y));

// A 1:1 ruler mark beside it: a bracket from the head's top to the tip.
export const lengthMark = (x, y, len) => line([x, y], [x, y + len], { w: 0.2 }) + line([x - 0.8, y], [x + 0.8, y], { w: 0.2 }) + line([x - 0.8, y + len], [x + 0.8, y + len], { w: 0.2 });

export const battery = (x, y, s = 1) => g(
  poly([[-12, -3], [6, -9], [16, -4], [-2, 2]]) + poly([[-12, -3], [-2, 2], [-2, 6], [-12, 1]]) + poly([[-2, 2], [16, -4], [16, 0], [-2, 6]]) +
  path("M-11,0C-16,2 -15,6 -20,7M-10.5,1C-15,4 -13,8 -19,9", { w: 0.4 }) + rect(-23, 6, 4, 3.6, { r: 0.4 }),
  at(x, y, s));

// A drive icon: what the nice!nano shows up as.
export const drive = (x, y, s = 1) => g(
  poly([[-10, -3], [10, -3], [12, 3], [-12, 3]]) + rect(-12, 3, 24, 5, { r: 0.8 }) + dot([8.5, 5.5], 0.7),
  at(x, y, s));
export const file = (x, y, s = 1) => g(
  path("M-5,-7L2,-7L5,-4L5,7L-5,7Z", { fill: "#fff" }) + path("M2,-7L2,-4L5,-4", { w: 0.3 }) +
  [-1, 1.5, 4].map(u => line([-2.8, u], [2.8, u], { w: 0.3 })).join(""), at(x, y, s));

// The nice!view status screen, drawn small.
export const statusScreen = (x, y, s = 1) => g(
  rect(-11, -6, 22, 12, { r: 1, w: 0.6 }) +
  rect(-9, -4.2, 5, 2.4, { w: 0.3 }) + rect(-4, -3.6, 0.6, 1.2, { w: 0.2, fill: "#000" }) + rect(-8.6, -3.8, 3, 1.6, { w: 0, fill: "#000" }) +
  path("M1,-4.4L1,-1.6L3,-3.2L1,-4.2M1,-1.6L3,-0.2L1,1.2", { w: 0.3 }) +
  [-8, -5, -2].map((u, i) => circle([u, 2.6], 1, { w: 0.25, fill: i ? "#fff" : "#000" })).join("") +
  rect(4, 0.8, 5, 3, { w: 0.3 }),
  at(x, y, s));
