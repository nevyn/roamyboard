// Renders the split keymap as two halves of modules. The keymap file is the only copy of the layout: this page
// fetches and parses it (keymap.js) on every load. The layer and the key module counts live in the URL.
(() => {
  "use strict";
  const { COLUMNS, ROWS, parseKeymap, describeBinding } = window.RoamyKeymap;
  const KEYMAP_URL = "../keymaps/roamyboard_split.keymap";
  const HALF = COLUMNS / 2; // keymap columns 0 to 14 are the left half, 15 to 29 the right
  const LONG_PRESS_MS = 450;
  const $ = (s, el = document) => el.querySelector(s), $$ = (s, el = document) => [...el.querySelectorAll(s)];
  const el = (tag, cls, attrs = {}) => Object.assign(document.createElement(tag), cls ? { className: cls } : {}, attrs);

  let keymap, halves, keys = [];
  const state = { layer: 0, preview: null, count: { left: 0, right: 0 } };
  const shown = () => state.preview ?? state.layer;

  function fail(message) {
    const box = $("[data-error]");
    box.hidden = false;
    box.querySelector("p").textContent = message;
    $("[data-layer-title]").textContent = "The keymap did not load";
    $(".board").hidden = true;
    $(".controls").hidden = true;
  }

  async function load() {
    const url = new URL(KEYMAP_URL, location.href).href;
    let text;
    try {
      const res = await fetch(url, { cache: "no-cache" });
      if (!res.ok) throw new Error(`HTTP ${res.status} ${res.statusText}`.trim());
      text = await res.text();
    } catch (e) {
      const hint = location.protocol === "file:" ? " Browsers do not fetch files from a page opened as a file: serve the repo root with python3 -m http.server and open /docs/layout/." : "";
      return fail(`Could not load the keymap from ${url}: ${e.message}.${hint}`);
    }
    try {
      keymap = parseKeymap(text);
    } catch (e) {
      return fail(`Could not read the keymap at ${url}: ${e.message}.`);
    }
    halves = usedHalves();
    if (!halves.left || !halves.right) return fail(`The keymap at ${url} has no keys on the ${halves.left ? "right" : "left"} half (keymap columns ${halves.left ? `${HALF} to ${COLUMNS - 1}` : `0 to ${HALF - 1}`}).`);
    readUrl();
    build();
    render();
  }

  // The keymap columns that some layer binds, per half: { left: [first, last], right: [first, last] }.
  function usedHalves() {
    const used = new Set();
    for (const layer of keymap.layers)
      layer.bindings.forEach((b, i) => { if (b.behavior !== "none") used.add(i % COLUMNS); });
    const range = cols => cols.length ? [Math.min(...cols), Math.max(...cols)] : null;
    return { left: range([...used].filter(c => c < HALF)), right: range([...used].filter(c => c >= HALF)) };
  }
  const moduleCount = side => halves[side][1] - halves[side][0] + 1;

  // Anchors: the left half counts from its MCU module and the right half from its terminator module, so both keep
  // the keymap columns nearest the middle of the keyboard.
  const attached = col => col < HALF ? col > halves.left[1] - state.count.left : col < halves.right[0] + state.count.right;
  const isThumb = (col, row) => row === ROWS - 1 && ((col <= halves.left[1] && col > halves.left[1] - 3) || (col >= halves.right[0] && col < halves.right[0] + 3));

  function readUrl() {
    const q = new URLSearchParams(location.search);
    const clamp = (v, lo, hi, d) => { const n = parseInt(v, 10); return Number.isFinite(n) ? Math.min(hi, Math.max(lo, n)) : d; };
    state.layer = clamp(q.get("layer"), 0, keymap.layers.length - 1, 0);
    for (const side of ["left", "right"]) state.count[side] = clamp(q.get(side), 1, moduleCount(side), moduleCount(side));
  }
  function writeUrl() {
    const q = new URLSearchParams();
    if (state.layer) q.set("layer", state.layer);
    for (const side of ["left", "right"]) if (state.count[side] !== moduleCount(side)) q.set(side, state.count[side]);
    history.replaceState(null, "", q.toString() ? `?${q}` : location.pathname);
  }

  // ---- building the DOM once; render() only updates it, so a hovered key survives a layer preview

  function build() {
    const switcher = $(".layers");
    keymap.layers.forEach((layer, n) => {
      const b = el("button", "btn", { type: "button" });
      b.innerHTML = `<span class="n">${n}</span> `;
      b.append(layer.name);
      b.onclick = () => { state.preview = null; state.layer = n; writeUrl(); render(); };
      switcher.append(b);
    });

    const board = $(".board");
    for (const side of ["left", "right"]) {
      const half = el("div", "half");
      half.setAttribute("role", "group");
      half.setAttribute("aria-label", `${side === "left" ? "Left" : "Right"} half`);
      half.dataset.side = side;
      half.append(el("div", "terminator", { title: "Terminator module" }));
      for (let col = halves[side][0]; col <= halves[side][1]; col++) {
        const mod = el("div", "key-module");
        mod.dataset.col = col;
        for (let row = 0; row < ROWS; row++) {
          const k = el("button", "key", { type: "button", tabIndex: -1 });
          k.dataset.col = col; k.dataset.row = row;
          k.append(el("span", "cap"));
          if (isThumb(col, row)) k.classList.add("thumb");
          mod.append(k);
          keys.push(k);
        }
        half.append(mod);
      }
      const mcu = el("div", "mcu", { title: "MCU module" });
      mcu.append(el("div", "screen"));
      half.append(mcu);
      board.append(half);

      const slider = $(`[data-count=${side}]`);
      slider.max = moduleCount(side);
      slider.oninput = () => { state.count[side] = +slider.value; writeUrl(); render(); };
    }
    keys[0].tabIndex = 0;
    wireKeys(board);
  }

  function render() {
    const layer = keymap.layers[shown()];
    $$(".layers button").forEach((b, n) => {
      b.setAttribute("aria-pressed", n === state.layer);
      b.classList.toggle("previewing", n === state.preview);
    });
    $("[data-layer-title]").textContent = `Layer ${shown()}: ${layer.name}${state.preview !== null ? " (preview)" : ""}`;

    for (const k of keys) {
      const col = +k.dataset.col, row = +k.dataset.row;
      const binding = layer.bindings[row * COLUMNS + col], d = describeBinding(binding, keymap);
      k.binding = binding; k.desc = d;
      k.className = `key kind-${d.kind}` + (isThumb(col, row) ? " thumb" : "") + (d.label.length > 4 ? " long" : "") + (d.label.length > 6 ? " longer" : "");
      // Lets a tap/hold label such as "↩/Boot" break after the slash when the key is too narrow.
      const cap = k.querySelector(".cap"), parts = d.label.length > 4 ? d.label.split("/") : [d.label];
      cap.replaceChildren(...parts.flatMap((part, i) => i < parts.length - 1 ? [part + "/", el("wbr")] : [part]));
      const on = attached(col);
      k.setAttribute("aria-label", `${d.name}${d.kind === "none" ? "" : `, ${binding.raw}`}, keymap column ${col}, row ${row + 1}${on ? "" : ", key module not attached"}`);
    }
    $$(".key-module").forEach(m => m.classList.toggle("detached", !attached(+m.dataset.col)));

    for (const side of ["left", "right"]) {
      const n = state.count[side];
      $(`[data-count=${side}]`).value = n;
      $(`[data-count-out=${side}]`).textContent = n;
      const screen = $(`.half[data-side=${side}] .screen`), cols = `${n} col${n === 1 ? "" : "s"}`;
      // The split peripheral's status screen shows no layer; only the central (left half) does.
      screen.innerHTML = side === "left" ? `<b>${shown()}</b><small>${cols}</small>` : `<small>${cols}</small>`;
      screen.setAttribute("aria-label", side === "left" ? `Status screen: layer ${shown()}, ${layer.name}; ${cols}` : `Status screen: ${cols}`);
      screen.setAttribute("role", "img");
    }
    if (tipFor && tipFor !== previewFrom) showTip(tipFor);
  }

  // ---- tooltips, layer previews and keyboard navigation

  const tip = $(".tip");
  let tipFor = null;
  function showTip(k) {
    tipFor = k;
    const d = k.desc, col = +k.dataset.col, row = +k.dataset.row;
    let extra = d.detail ? `<p>${esc(d.detail)}</p>` : "";
    if (d.kind === "trans" && shown() > 0) {
      const below = describeBinding(keymap.layers[0].bindings[row * COLUMNS + col], keymap);
      extra += `<p>On ${esc(keymap.layers[0].name)}: ${esc(below.name)}.</p>`;
    }
    if (d.kind === "layer" && d.target !== shown()) extra += `<p>Hover or long-press to preview; click to show it.</p>`;
    if (!attached(col)) extra += `<p>This key module is not attached, so the key never fires.</p>`;
    tip.innerHTML = `<b>${esc(d.name)}</b>${extra}<small>${esc(k.binding.raw)} · keymap column ${col}, row ${row + 1}${k.classList.contains("thumb") ? " · thumb key" : ""}</small>`;
    tip.hidden = false;
    const r = k.getBoundingClientRect(), t = tip.getBoundingClientRect();
    const x = Math.min(innerWidth - t.width - 8, Math.max(8, r.left + r.width / 2 - t.width / 2));
    const above = r.top - t.height - 8;
    tip.style.left = `${x}px`;
    tip.style.top = `${above > 8 ? above : r.bottom + 8}px`;
  }
  function hideTip() { tipFor = null; tip.hidden = true; }
  const esc = s => String(s).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);

  let previewFrom = null;
  function startPreview(k) {
    if (k.desc.kind !== "layer" || k.desc.target === undefined || !keymap.layers[k.desc.target] || k.desc.target === shown()) return;
    previewFrom = k; state.preview = k.desc.target; render();
  }
  function endPreview() {
    if (previewFrom === null) return;
    previewFrom = null; state.preview = null; render();
  }

  function wireKeys(board) {
    let pressTimer = null, longPressed = false;
    board.addEventListener("pointerover", e => {
      const k = e.target.closest(".key");
      if (!k || e.pointerType !== "mouse" || k === tipFor) return;
      showTip(k);
      if (!previewFrom) startPreview(k);
    });
    board.addEventListener("pointerout", e => {
      const k = e.target.closest(".key");
      if (!k || e.pointerType !== "mouse" || k.contains(e.relatedTarget)) return;
      if (k === previewFrom) endPreview();
      if (k === tipFor && document.activeElement !== k) hideTip();
    });
    board.addEventListener("pointerdown", e => {
      const k = e.target.closest(".key");
      if (!k || e.pointerType === "mouse") return;
      longPressed = false;
      pressTimer = setTimeout(() => { longPressed = true; startPreview(k); showTip(k); }, LONG_PRESS_MS);
    });
    const release = () => { clearTimeout(pressTimer); if (longPressed) endPreview(); };
    board.addEventListener("pointerup", release);
    board.addEventListener("pointercancel", release);
    board.addEventListener("contextmenu", e => { if (e.target.closest(".key")) e.preventDefault(); });
    board.addEventListener("click", e => {
      const k = e.target.closest(".key");
      if (!k) return;
      if (longPressed) { longPressed = false; return; }
      const target = k.desc.target;
      if (k.desc.kind === "layer" && keymap.layers[target] && (target !== state.layer || state.preview !== null)) {
        previewFrom = null; state.preview = null; state.layer = target; writeUrl(); render();
      }
      showTip(k);
    });
    board.addEventListener("focusin", e => { const k = e.target.closest(".key"); if (k) showTip(k); });
    board.addEventListener("focusout", e => { if (!board.contains(e.relatedTarget)) hideTip(); });
    board.addEventListener("keydown", e => {
      const k = e.target.closest(".key");
      const step = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] }[e.key];
      if (!k || !step) { if (e.key === "Escape") hideTip(); return; }
      e.preventDefault();
      const cols = [...new Set(keys.map(x => +x.dataset.col))];
      const ci = Math.min(cols.length - 1, Math.max(0, cols.indexOf(+k.dataset.col) + step[0]));
      const row = Math.min(ROWS - 1, Math.max(0, +k.dataset.row + step[1]));
      const next = keys.find(x => +x.dataset.col === cols[ci] && +x.dataset.row === row);
      k.tabIndex = -1; next.tabIndex = 0; next.focus();
    });
    document.addEventListener("pointerdown", e => { if (!e.target.closest(".key")) hideTip(); });
    addEventListener("scroll", () => { if (tipFor) showTip(tipFor); }, { passive: true });
  }

  load();
})();
