// The order page: a configuration in the URL, a drawing of the keyboard it describes, a bill built from the
// vendor prices in ../parts.js and catalog.js, and a mailto: that sends the lot to Nevyn.
(() => {
  "use strict";
  const { PARTS, RATES, RATES_DATE, FILAMENT } = window.RoamyParts;
  const { WIDTH, THICK, arcRadius, radiusFromSagitta } = window.RoamyGeometry;
  const C = window.RoamyCatalog;
  const $ = (s, el = document) => el.querySelector(s), $$ = (s, el = document) => [...el.querySelectorAll(s)];
  const el = (tag, cls, attrs = {}) => Object.assign(document.createElement(tag), cls ? { className: cls } : {}, attrs);
  const ORDER_TO = "hello@nevyn.dev";
  const PROFILES = ["flat", "homing", "convex"];

  // ---- state

  const curveDefault = C.CURVES.find(c => c.default).angle;
  const state = {
    halves: 2, cols: C.COLUMNS.default, rows: C.ROWS.built, curve: curveDefault,
    material: C.DEFAULTS.material, caseColour: C.DEFAULTS.caseColour, accent: null,
    switch: C.DEFAULTS.switch, brush: C.DEFAULTS.capColour, profile: "flat", build: "kit",
    caps: {},   // "side:col:row" -> { colour, profile }; absent means the default colour, flat
  };
  const DEFAULTS = { ...state };
  const BASE_CAP = C.DEFAULTS.capColour;
  const capKey = (side, col, row) => `${side}:${col}:${row}`;
  const capAt = (side, col, row) => state.caps[capKey(side, col, row)] || { colour: BASE_CAP, profile: "flat" };
  const keyCount = () => state.halves * state.cols * state.rows;
  const counts = () => ({ key: state.cols * state.halves, mcu: state.halves, term: state.halves, build: 1 });
  const palette = () => C.FILAMENTS[state.material];
  const hexOf = (list, name) => (list.find(e => e[0] === name) || [])[1] || "#888888";
  const caseHex = () => hexOf(palette(), state.caseColour);
  const accentHex = () => state.accent ? hexOf(palette(), state.accent) : caseHex();
  const capHex = name => hexOf(C.CAPS, name);
  // A key module's three inner columns carry the thumb keys on their last row, as the firmware's keymap does.
  const isThumb = (col, row) => row === state.rows - 1 && col >= state.cols - 3;

  // ---- the configuration in the query string

  // Painted keys as runs over the canonical order, so a mostly-uniform board stays a short link:
  // "35*0f-6*3c" is 35 flat keys of palette colour 0, then 6 convex of colour 3.
  function encodeCaps() {
    const tokens = [];
    for (let s = 0; s < state.halves; s++)
      for (let c = 0; c < state.cols; c++)
        for (let r = 0; r < state.rows; r++) {
          const cap = capAt(s, c, r);
          tokens.push(`${Math.max(0, C.CAPS.findIndex(e => e[0] === cap.colour)).toString(36)}${cap.profile[0]}`);
        }
    const runs = [];
    for (const t of tokens) {
      const last = runs[runs.length - 1];
      if (last && last.t === t) last.n++; else runs.push({ t, n: 1 });
    }
    return runs.map(r => (r.n > 1 ? `${r.n}*` : "") + r.t).join("-");
  }
  function decodeCaps(str) {
    const caps = {};
    const flat = [];
    for (const run of str.split("-")) {
      const m = /^(?:(\d+)\*)?([0-9a-z])([fhc])$/.exec(run);
      if (!m) return null;
      const colour = (C.CAPS[parseInt(m[2], 36)] || [])[0];
      const profile = PROFILES.find(p => p[0] === m[3]);
      if (!colour || !profile) return null;
      for (let i = 0; i < (m[1] ? +m[1] : 1); i++) flat.push({ colour, profile });
    }
    let i = 0;
    for (let s = 0; s < state.halves; s++)
      for (let c = 0; c < state.cols; c++)
        for (let r = 0; r < state.rows; r++) {
          const cap = flat[i++];
          if (cap && !(cap.colour === BASE_CAP && cap.profile === "flat")) caps[capKey(s, c, r)] = cap;
        }
    return caps;
  }

  function readUrl() {
    const q = new URLSearchParams(location.search);
    const int = (k, lo, hi, d) => { const n = parseInt(q.get(k), 10); return Number.isFinite(n) ? Math.min(hi, Math.max(lo, n)) : d; };
    const one = (k, list, d) => list.includes(q.get(k)) ? q.get(k) : d;
    state.halves = int("halves", 1, 2, 2);
    state.cols = int("cols", C.COLUMNS.min, C.COLUMNS.max, C.COLUMNS.default);
    state.rows = int("rows", C.ROWS.min, C.ROWS.max, C.ROWS.built);
    const curve = int("curve", 0, 90, curveDefault);
    state.curve = C.CURVES.some(c => c.angle === curve) ? curve : curveDefault;
    state.material = one("material", Object.keys(C.FILAMENTS), DEFAULTS.material);
    const names = palette().map(e => e[0]);
    state.caseColour = one("case", names, names.includes(DEFAULTS.caseColour) ? DEFAULTS.caseColour : names[0]);
    state.accent = names.includes(q.get("accent")) ? q.get("accent") : null;
    state.switch = one("switch", C.SWITCHES.map(s => s.name), state.switch);
    state.build = one("build", ["kit", "assembled"], "kit");
    state.caps = (q.get("caps") && decodeCaps(q.get("caps"))) || {};
  }
  function writeUrl() {
    const q = new URLSearchParams();
    const put = (k, v, d) => { if (v !== d && v != null) q.set(k, v); };
    put("halves", state.halves, 2);
    put("cols", state.cols, C.COLUMNS.default);
    put("rows", state.rows, C.ROWS.built);
    put("curve", state.curve, curveDefault);
    put("material", state.material, DEFAULTS.material);
    put("case", state.caseColour, DEFAULTS.caseColour);
    put("accent", state.accent, null);
    put("switch", state.switch, DEFAULTS.switch);
    put("build", state.build, "kit");
    const caps = encodeCaps();
    if (Object.keys(state.caps).length) q.set("caps", caps);
    history.replaceState(null, "", q.toString() ? `?${q}` : location.pathname);
  }
  const permalink = () => location.href;

  // ---- the bill

  const toEur = (amount, currency) => amount / RATES[currency];
  const money = eur => `€${eur.toFixed(2)} / ${(eur * RATES.SEK).toFixed(0)} kr`;
  // Unit prices stay in euros alone: a line like "70 × €0.00 / 0 kr" reads as free when it is not.
  const unitPrice = eur => `€${eur.toFixed(eur < 0.1 ? 3 : 2)}`;

  function bill() {
    const c = counts(), lines = [];
    const qtyOf = per => (per.key || 0) * c.key + (per.keyRow || 0) * c.key * state.rows +
      (per.mcu || 0) * c.mcu + (per.term || 0) * c.term + (per.build || 0);

    let grams = 0;
    for (const row of PARTS) {
      if (row.group || !row.per || row.per.text) continue;
      const qty = qtyOf(row.per);
      if (!qty) continue;
      if (row.grams) { grams += row.grams * qty; continue; }
      if (!row.price) continue;
      const unit = toEur(row.price.amount, row.price.currency) / row.price.each;
      lines.push({ what: row.name, detail: `${qty} × ${unitPrice(unit)}`, eur: unit * qty, vendor: row.price.vendor, url: row.price.url });
    }

    const sw = C.SWITCHES.find(s => s.name === state.switch);
    const swPacks = Math.ceil(keyCount() / sw.pack);
    lines.push({
      what: `${sw.name} switches`, eur: swPacks * sw.price, vendor: "splitkb",
      url: `https://splitkb.com/products/${sw.handle}`,
      detail: `${keyCount()} keys → ${swPacks} × pack of ${sw.pack}` + (swPacks * sw.pack > keyCount() ? ` (${swPacks * sw.pack - keyCount()} spare)` : ""),
    });

    for (const { colour, profile, n } of capTally()) {
      const pack = C.CAP_PACKS[profile];
      const packs = Math.ceil(n / pack.pack);
      lines.push({
        what: `${colour} keycaps, ${pack.label}`, eur: packs * pack.price, vendor: "splitkb", swatch: capHex(colour),
        url: `https://splitkb.com/products/${(C.CAPS.find(e => e[0] === colour) || [])[2]}`,
        detail: `${n} cap${n > 1 ? "s" : ""} → ${packs} × pack of ${pack.pack}` + (packs * pack.pack > n ? ` (${packs * pack.pack - n} spare)` : ""),
      });
    }

    lines.push({
      what: `Filament, ${state.material}`, eur: grams / 1000 * toEur(FILAMENT.amount, FILAMENT.currency),
      detail: `${Math.round(grams)} g at ${unitPrice(toEur(FILAMENT.amount, FILAMENT.currency))} a kilo, measured off the print models`,
      vendor: FILAMENT.vendor, url: FILAMENT.url,
    });

    const parts = lines.reduce((a, l) => a + l.eur, 0);
    const markup = parts * C.MARKUP;
    const out = { lines, parts, markup, partsTotal: parts + markup, grams, hours: 0, assembly: 0 };

    if (state.build === "assembled") {
      const h = C.ASSEMBLY.hours;
      out.hours = state.halves * (h.key * state.cols + h.mcu + h.term);
      out.assembly = toEur(out.hours * C.ASSEMBLY.rate, C.ASSEMBLY.currency);
    }
    out.total = out.partsTotal + out.assembly;
    return out;
  }

  // How many caps of each colour and profile the build needs.
  function capTally() {
    const seen = new Map();
    for (let s = 0; s < state.halves; s++)
      for (let c = 0; c < state.cols; c++)
        for (let r = 0; r < state.rows; r++) {
          const { colour, profile } = capAt(s, c, r);
          const k = `${colour}|${profile}`;
          seen.set(k, (seen.get(k) || 0) + 1);
        }
    return [...seen].map(([k, n]) => ({ colour: k.split("|")[0], profile: k.split("|")[1], n }))
      .sort((a, b) => b.n - a.n);
  }

  // ---- building the page

  function buildPickers() {
    $("[data-pick=material]").append(...Object.keys(C.FILAMENTS).map(m => {
      const b = el("button", null, { type: "button" }); b.dataset.v = m; b.textContent = m;
      b.onclick = () => {
        state.material = m;
        const names = palette().map(e => e[0]);
        if (!names.includes(state.caseColour)) state.caseColour = names[0];
        if (state.accent && !names.includes(state.accent)) state.accent = names[0];
        commit();
      };
      return b;
    }));

    $("[data-pick=curve]").append(...C.CURVES.map(cv => {
      const b = el("button", "curve-opt", { type: "button" });
      b.dataset.v = cv.angle;
      b.setAttribute("role", "radio");
      const r = arcRadius(cv.angle);
      b.innerHTML = `<b>${cv.name}</b><span class="deg">${cv.angle}°</span>` +
        `<small>${Number.isFinite(r) ? `bends around ${Math.round(r)} mm` : "no bend"}<br>${cv.fits}</small>`;
      b.onclick = () => { state.curve = cv.angle; commit(); };
      return b;
    }));

    $("[data-pick=switch]").append(...C.SWITCHES.map(s => {
      const b = el("button", "switch-opt", { type: "button" });
      b.dataset.v = s.name;
      b.setAttribute("role", "radio");
      b.innerHTML = `<b>${s.name}</b><span class="pill ${s.feel.includes("silent") ? "ok" : s.feel === "tactile" ? "orange" : ""}">${s.feel}</span>` +
        `<small>${s.force} gf · €${s.price.toFixed(2)} per ${s.pack}</small>`;
      b.onclick = () => { state.switch = s.name; commit(); };
      return b;
    }));

    for (const [name, max] of [["cols", C.COLUMNS], ["rows", C.ROWS]]) {
      const input = $(`[data-input=${name}]`);
      input.min = max.min; input.max = max.max;
      input.oninput = () => { state[name] = +input.value; commit(); };
    }
    $$("[data-pick=halves] button").forEach(b => b.onclick = () => { state.halves = +b.dataset.v; commit(); });
    $$("[data-pick=build] button").forEach(b => b.onclick = () => { state.build = b.dataset.v; commit(); });
    $$("[data-pick=profile] button").forEach(b => b.onclick = () => { state.profile = b.dataset.v; commit(); });
    $("[data-pick=accent-on]").onchange = e => {
      state.accent = e.target.checked ? (state.accent || palette()[0][0]) : null;
      commit();
    };
    $$("[data-preset]").forEach(b => b.onclick = () => paintPreset(b.dataset.preset));
    for (const which of ["span", "drop"]) $(`[data-measure=${which}]`).oninput = suggestCurve;
  }

  function swatchList(host, names, hexes, current, pick) {
    host.innerHTML = "";
    names.forEach((name, i) => {
      const b = el("button", "swatch-btn", { type: "button", title: name });
      b.style.setProperty("--c", hexes[i]);
      b.setAttribute("aria-pressed", String(name === current));
      b.setAttribute("aria-label", name);
      b.onclick = () => pick(name);
      host.append(b);
    });
  }

  function buildBoard() {
    const board = $("[data-board]");
    board.innerHTML = "";
    for (let s = 0; s < state.halves; s++) {
      const half = el("div", "half", { role: "group" });
      half.setAttribute("aria-label", state.halves === 2 ? `${s ? "Right" : "Left"} half` : "The half");
      half.append(el("div", "terminator", { title: "Terminator module" }));
      for (let c = 0; c < state.cols; c++) {
        const mod = el("div", "key-module");
        for (let r = 0; r < state.rows; r++) {
          const k = el("button", "key", { type: "button", tabIndex: -1 });
          k.dataset.k = capKey(s, c, r);
          if (isThumb(c, r)) k.classList.add("thumb");
          k.onclick = () => { paintAt(s, c, r); commit(); };
          mod.append(k);
        }
        half.append(mod);
      }
      const mcu = el("div", "mcu", { title: "MCU module" });
      mcu.append(el("div", "screen"));
      half.append(mcu);
      board.append(half);
    }
    const keys = $$(".key", board);
    if (keys[0]) keys[0].tabIndex = 0;
    board.onkeydown = e => {
      const i = keys.indexOf(document.activeElement);
      if (i < 0) return;
      const step = { ArrowUp: -1, ArrowDown: 1, ArrowLeft: -state.rows, ArrowRight: state.rows }[e.key];
      if (!step) return;
      const next = keys[Math.min(keys.length - 1, Math.max(0, i + step))];
      e.preventDefault();
      keys.forEach(k => k.tabIndex = -1);
      next.tabIndex = 0; next.focus();
    };
  }

  function paintPreset(which) {
    if (which === "reset") { state.caps = {}; return commit(); }
    for (let s = 0; s < state.halves; s++)
      for (let c = 0; c < state.cols; c++)
        for (let r = 0; r < state.rows; r++) {
          const hit = which === "all" || (which === "thumbs" && isThumb(c, r)) ||
            (which === "outer" && (c === 0 || c === state.cols - 1));
          if (hit) paintAt(s, c, r);
        }
    commit();
  }
  function paintAt(s, c, r) {
    const k = capKey(s, c, r);
    if (state.brush === BASE_CAP && state.profile === "flat") delete state.caps[k];
    else state.caps[k] = { colour: state.brush, profile: state.profile };
  }

  // ---- the curve drawing: the half's modules laid along the arc, over the leg they bend around

  function drawCurve() {
    const svg = $("[data-curve-svg]");
    const R = arcRadius(state.curve);
    const mods = [{ w: WIDTH.term, t: THICK.term, kind: "term" },
      ...Array.from({ length: state.cols }, (_, col) => ({ w: WIDTH.key, t: THICK.key, kind: "key", col })),
      { w: WIDTH.mcu, t: THICK.mcu, kind: "mcu" }];
    const span = mods.reduce((a, m) => a + m.w, 0);

    // Place each module by arc length from the middle of the half, then scale the lot to fit the viewBox.
    const parts = [];
    let at = -span / 2;
    for (const m of mods) {
      const mid = at + m.w / 2;
      const angle = Number.isFinite(R) ? mid / R : 0;
      parts.push({ ...m, angle, x: Number.isFinite(R) ? R * Math.sin(angle) : mid, y: Number.isFinite(R) ? R * (1 - Math.cos(angle)) : 0 });
      at += m.w;
    }
    const maxX = Math.max(...parts.map(p => Math.abs(p.x))) + WIDTH.mcu / 2;
    const maxY = Math.max(...parts.map(p => p.y)) + 20;
    const k = Math.min(190 / maxX, 150 / Math.max(maxY, 40));
    const cx = 210, cy = 48;

    const leg = Number.isFinite(R)
      ? `<circle cx="${cx}" cy="${cy + R * k}" r="${R * k}" class="leg"/>`
      : `<rect x="${cx - 190}" y="${cy}" width="380" height="140" class="leg"/>`;

    const body = parts.map(p => {
      const x = cx + p.x * k, y = cy + p.y * k, deg = p.angle * 180 / Math.PI;
      const w = p.w * k, t = p.t * k;
      const fill = p.kind === "key" ? caseHex() : accentHex();
      const cap = p.kind === "key"
        ? `<rect x="${-w / 2 + w * .1}" y="${-t - 3.2}" width="${w * .8}" height="3.2" rx="1" fill="${capHex(columnCapColour(p.col))}"/>`
        : "";
      return `<g transform="translate(${x} ${y}) rotate(${deg})">` +
        `<rect x="${-w / 2}" y="${-t}" width="${w}" height="${t}" rx="${Math.min(2, w / 4)}" fill="${fill}" class="mod"/>${cap}</g>`;
    }).join("");

    svg.innerHTML = `${leg}${body}`;
    const cv = C.CURVES.find(c => c.angle === state.curve);
    $("[data-curve-caption]").textContent = Number.isFinite(R)
      ? `${cv.name}: ${state.curve}° between columns, an arc of ${Math.round(R)} mm radius, ${Math.round(span)} mm of keyboard around it.`
      : `${cv.name}: the columns sit in a line, ${Math.round(span)} mm wide.`;
  }
  // The cross-section shows one key per module, so colour it with that column's commonest cap.
  function columnCapColour(col) {
    const seen = new Map();
    for (let s = 0; s < state.halves; s++)
      for (let r = 0; r < state.rows; r++) {
        const { colour } = capAt(s, col, r);
        seen.set(colour, (seen.get(colour) || 0) + 1);
      }
    return [...seen].sort((a, b) => b[1] - a[1])[0][0];
  }

  function suggestCurve() {
    const w = parseFloat($("[data-measure=span]").value), d = parseFloat($("[data-measure=drop]").value);
    const out = $("[data-measure-out]");
    if (!(w > 0 && d > 0)) { out.textContent = "Fill both in and this will suggest a curve."; return; }
    const R = radiusFromSagitta(w, d);
    const best = C.CURVES.filter(c => c.angle).reduce((a, b) =>
      Math.abs(arcRadius(b.angle) - R) < Math.abs(arcRadius(a.angle) - R) ? b : a);
    out.innerHTML = `Your leg curves at about <b>${Math.round(R)} mm</b> radius there, which is closest to ` +
      `<b>${best.name}, ${best.angle}°</b> (${Math.round(arcRadius(best.angle))} mm). ` +
      `A keyboard flatter than your leg rests on its ends; one more curved rests in the middle.`;
  }

  // ---- render

  function render() {
    const names = palette().map(e => e[0]), hexes = palette().map(e => e[1]);
    $("[data-out=cols]").textContent = state.cols;
    $("[data-out=rows]").textContent = state.rows;
    $("[data-input=cols]").value = state.cols;
    $("[data-input=rows]").value = state.rows;
    $("[data-note=cols]").textContent =
      `${state.cols * state.halves} key modules, ${keyCount()} keys. Columns click together, so you can start narrow and add more later.`;
    $("[data-note=material]").textContent = state.material === "PETG HF"
      ? "Tougher and less brittle than PLA, and it minds a hot car less. Fewer colours."
      : "Prints beautifully and comes in every colour. Goes soft in a car on a summer day.";

    const rowWarn = $("[data-warn=rows]");
    rowWarn.hidden = state.rows === C.ROWS.built;
    if (!rowWarn.hidden) rowWarn.querySelector("p").innerHTML =
      `Only <b>${C.ROWS.built} keys per column</b> exists today. ${state.rows} would need a new PCB, a new case and a new keymap, ` +
      `so it carries a design fee, quoted on request. Ask Nevyn before you count on it.`;

    $$("[data-pick=halves] button").forEach(b => b.setAttribute("aria-pressed", String(+b.dataset.v === state.halves)));
    $$("[data-pick=build] button").forEach(b => b.setAttribute("aria-pressed", String(b.dataset.v === state.build)));
    $$("[data-pick=profile] button").forEach(b => b.setAttribute("aria-pressed", String(b.dataset.v === state.profile)));
    $$("[data-pick=material] button").forEach(b => b.setAttribute("aria-pressed", String(b.dataset.v === state.material)));
    $$("[data-pick=curve] button").forEach(b => b.setAttribute("aria-checked", String(+b.dataset.v === state.curve)));
    $$("[data-pick=switch] button").forEach(b => b.setAttribute("aria-checked", String(b.dataset.v === state.switch)));

    swatchList($("[data-pick=case]"), names, hexes, state.caseColour, n => { state.caseColour = n; commit(); });
    $("[data-out=case]").textContent = `${state.caseColour}`;
    const accentBox = $("[data-pick=accent]");
    accentBox.hidden = !state.accent;
    $("[data-pick=accent-on]").checked = !!state.accent;
    if (state.accent) swatchList(accentBox, names, hexes, state.accent, n => { state.accent = n; commit(); });
    swatchList($("[data-pick=brush]"), C.CAPS.map(e => e[0]), C.CAPS.map(e => e[1]), state.brush, n => { state.brush = n; commit(); });
    $("[data-out=brush]").textContent = `${state.brush}, ${state.profile}`;

    $("[data-board-tag]").textContent = `${state.cols * state.halves} modules, ${keyCount()} keys`;
    // Key size on a narrow screen follows one half's width: key modules, their gaps, terminator and MCU.
    document.documentElement.style.setProperty("--half-units", (state.cols * 1.28 + 2.31).toFixed(2));
    document.documentElement.style.setProperty("--board-units", (state.halves * (state.rows * 1.1 + 0.2) + 0.5).toFixed(2));
    document.documentElement.style.setProperty("--case", caseHex());
    document.documentElement.style.setProperty("--accent-case", accentHex());
    $$("[data-board] .key").forEach(k => {
      const [s, c, r] = k.dataset.k.split(":").map(Number);
      const cap = capAt(s, c, r);
      k.style.setProperty("--cap", capHex(cap.colour));
      k.className = `key profile-${cap.profile}` + (isThumb(c, r) ? " thumb" : "");
      k.title = `${cap.colour}, ${cap.profile}`;
    });

    $("[data-cap-legend]").innerHTML = capTally().map(({ colour, profile, n }) =>
      `<li><i class="sw" style="background:${capHex(colour)}"></i>${colour}${profile === "flat" ? "" : ` ${profile}`} × ${n}</li>`).join("");

    drawCurve();
    renderBill();
  }

  function renderBill() {
    const b = bill();
    const row = (what, detail, eur, cls = "") =>
      `<tr class="${cls}"><td>${what}${detail ? `<br><span class="summary">${detail}</span>` : ""}</td>` +
      `<td class="n">${eur == null ? "—" : money(eur)}</td></tr>`;

    let html = b.lines.map(l => row(
      (l.swatch ? `<i class="sw" style="background:${l.swatch}"></i>` : "") +
      (l.url ? `<a href="${l.url}">${l.what}</a>` : l.what), l.detail, l.eur)).join("");
    html += row(`Markup`, `${Math.round(C.MARKUP * 100)} % on parts, for the odds and ends no line catches`, b.markup);
    html += row(`<b>Parts</b>`, "", b.partsTotal, "sum");
    if (state.build === "assembled")
      html += row(`<b>Assembly by Nevyn</b>`, `${b.hours} hours at ${C.ASSEMBLY.rate} ${C.ASSEMBLY.currency}/h, soldering included`, b.assembly, "sum");
    if (state.rows !== C.ROWS.built)
      html += row(`<b>Design fee</b>`, `for ${state.rows} keys per column — quoted on request`, null, "sum");
    html += row(`Shipping`, "quoted separately, once you say where to", null);
    html += row(`<b>Total</b>`, state.rows !== C.ROWS.built ? "before the design fee" : "", b.total, "total");
    $("[data-bill]").innerHTML = html;

    $("[data-running-what]").textContent = state.build === "assembled" ? "Assembled " : "As a kit ";
    $("[data-running-total]").textContent = money(b.total);

    $("[data-rates]").textContent =
      `Vendor prices as published on ${RATES_DATE}, converted at the European Central Bank's rates of that day ` +
      `(€1 = ${RATES.SEK} kr = $${RATES.USD}). Both drift; treat the bill as an estimate.`;

    const order = $("[data-order]");
    order.href = `mailto:${ORDER_TO}?subject=${encodeURIComponent("Roamyboard order")}&body=${encodeURIComponent(orderBody(b))}`;
    $("[data-permalink]").innerHTML = `Opens your mail app. The link in it carries this exact configuration, so nothing is lost in the retelling.`;
  }

  function orderBody(b) {
    const cv = C.CURVES.find(c => c.angle === state.curve);
    const R = arcRadius(state.curve);
    const lines = [
      `Hi Nevyn, I would like to order a Roamyboard.`, ``,
      `  Shape       ${state.halves === 2 ? "split, two halves" : "unibody, one half"}, ${state.cols} key modules per half, ${state.rows} keys per column`,
      `  Keys        ${keyCount()}`,
      `  Curve       ${cv.name}, ${state.curve}°${Number.isFinite(R) ? `, around ${Math.round(R)} mm` : ""}`,
      `  Case        ${state.material}, ${state.caseColour}${state.accent ? `, ${state.accent} for the MCU and terminator modules` : ""}`,
      `  Switches    ${state.switch}`,
      `  Keycaps     ${capTally().map(c => `${c.colour}${c.profile === "flat" ? "" : ` ${c.profile}`} × ${c.n}`).join(", ")}`,
      `  Build       ${state.build === "assembled" ? `assembled by you (${b.hours} h)` : "kit, I will build it"}`,
      ``,
      `  Parts       ${money(b.partsTotal)}`,
    ];
    if (state.build === "assembled") lines.push(`  Assembly    ${money(b.assembly)}`);
    if (state.rows !== C.ROWS.built) lines.push(`  Design fee  for ${state.rows} keys per column, to be quoted`);
    lines.push(`  Total       ${money(b.total)}, before shipping`, ``,
      `The same configuration as a link:`, permalink(), ``, `Thanks!`);
    return lines.join("\n");
  }

  // ---- go

  let built = { halves: null, cols: null, rows: null };
  function commit() {
    writeUrl();
    if (built.halves !== state.halves || built.cols !== state.cols || built.rows !== state.rows) {
      buildBoard();
      built = { halves: state.halves, cols: state.cols, rows: state.rows };
    }
    render();
  }

  readUrl();
  buildPickers();
  buildBoard();
  built = { halves: state.halves, cols: state.cols, rows: state.rows };
  render();
})();
