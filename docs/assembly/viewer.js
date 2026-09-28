// Interactive 3D figures for the assembly guide.
//
// A <div class="viewer" data-scene="<id>"> gets the scene SCENES[id]. Models are models/<name>.glb (Git LFS, written by
// case/roamy-v4/tools/guide_models.py), one mesh per part and colour, named "<part>#<rrggbb[aa]>".
// Scene coordinates are the case's module frame in mm: x across the column from the socket face, y along it from
// the SW1 end face, z up from the board's back. Viewers only hold a WebGL context while on screen.
(() => {
  "use strict";

  // Browsers refuse fetch() from file://, so the guide has to be served; the fallback says how.
  async function loadModel(name) {
    const url = `models/${name}.glb`;
    if (location.protocol === "file:") throw new Error("Opened from disk: serve the repo to see the 3D views: python3 -m http.server in the repo root, then open /docs/assembly/");
    const res = await fetch(url);
    if (!res.ok) throw new Error(`${url}: HTTP ${res.status}`);
    const buf = await res.arrayBuffer();
    if (new TextDecoder().decode(buf.slice(0, 4)) !== "glTF") throw new Error(`${url} is not a GLB; is it a Git LFS pointer? Run git lfs pull`);
    return buf;
  }

  const DATA = window.ROAMY_DATA;

  // ---- palette: group id -> [label, colour, opacity]
  const PLA = "#e9dfc7", FLOOR = "#9db4cc", GHOST = "#cfc8ba";
  const G = {
    shell:    ["Shell", PLA], floor: ["Floor", FLOOR],
    pcb:      ["Board", "#6a43a0"], ics: ["74HC165 and hotswap sockets", "#2a2730"],
    switches: ["Choc switches", "#f4f1ea", 0.72], legs: ["Switch legs", "#f4f1ea"],
    sockets:  ["Sockets", "#77727f"], headers: ["Headers", "#e8871e"], tails: ["Connector tails", "#c9a227"],
    ghost:    ["Neighbour", GHOST, 0.38], ghostBoard: ["Neighbour's board", "#8f7bb0", 0.38],
    support:  ["Painted support", "#ff3b30", 0.8], jig: ["Solder jig", "#b7cfa4"],
    terminator: ["Terminator module", PLA], termHeaders: ["Embedded headers", "#e8871e"],
    mcuShell: ["MCU shell", PLA], mcuFloor: ["MCU floor", FLOOR], socketBoard: ["Socket board", "#6a43a0"],
    view: ["nice!view", "#17161c"], nano: ["nice!nano", "#2f2d5c"], battery: ["Battery", "#b9bec5"],
    switch: ["Power switch", "#55545c"], reset: ["Reset button", "#55545c"], jack: ["Battery jack", "#efece2"],
    coupon: ["Coupon", PLA], print: ["Print", PLA],
  };

  // mesh name -> group id, first match wins; `ghostRest` sends key module parts to the ghost groups
  function classify(name, opts = {}) {
    const [part, hex] = name.split("#");
    const board = { "000000": "ics", "008000": "pcb", "808080": "sockets", "ffa500": "headers", "ffff00": "tails",
                    "ffffff": "legs", "ffffff80": "switches" };
    switch (part) {
      case "Shell": return opts.mcu ? "mcuShell" : "shell";
      case "Floor": return opts.mcu ? "mcuFloor" : "floor";
      case "Board": return board[hex];
      case "Key module shell": return "ghost";
      case "Key module board": return hex === "008000" ? "ghostBoard" : "ghost";
      case "Socket board": return hex === "008000" ? "socketBoard" : board[hex];
      case "Painted support": return "support";
      case "Jig": case "solder-jig-print": return "jig";
      case "Terminator": case "terminator-print": return "terminator";
      case "Headers": return "termHeaders";
      case "choc-cutout-coupon": return "coupon";
      default:
        if (part.startsWith("MCU ")) return part.slice(4);
        return "print";
    }
  }

  // ---- scenes. view: camera direction toward the model's centre from outside, in the module frame.
  const ISO = [0.9, -0.75, 1.1];
  const SCENES = {
    half: {
      badge: "ONE HALF", view: [0.25, -1.1, 0.75],
      count: { label: "Key modules", min: 1, max: 10, value: 5 },
      layers: n => [
        { model: "terminator", only: ["terminator", "termHeaders"] },
        ...Array.from({ length: n }, (_, i) => ({ model: "key-module", power: i + 1 })),
        { model: "mcu-module", power: n, mcu: true, hide: ["ghost", "ghostBoard"] },
      ],
      legend: ["shell", "floor", "switches", "mcuShell", "terminator"],
    },
    coupon: { badge: "PRINT POSE", model: "choc-cutout-coupon", view: [0.6, -1, 1.3], bed: true },
    jigPrint: { badge: "PRINT POSE", model: "solder-jig-print", view: [0.7, -1, 1.2], bed: true },
    keyPrint: {
      badge: "PRINT POSE", model: "key-module-print", view: [0.8, -1.1, 1.1], bed: true,
      legend: ["shell", "floor", "support"],
      note: "Red: the guide pins, the only parts that get painted support.",
    },
    mcuPrint: { badge: "PRINT POSE", model: "mcu-module-print", view: [0.8, -1.1, 1.1], bed: true },
    termPrint: {
      badge: "PRINT POSE", model: "terminator-print", view: [1.1, -0.9, 1.2], bed: true,
      clip: { label: "Print height", group: ["terminator", "support"], marker: DATA.terminatorPause },
      legend: ["terminator", "termHeaders", "support"],
    },
    jig: {
      badge: "SOLDER JIG", model: "solder-jig", view: [0.9, -0.8, 1.3],
      explode: { pcb: [0, 0, 14], ics: [0, 0, 14], sockets: [0, 0, 14], headers: [0, 0, 14], tails: [0, 0, 14] },
      sequence: [
        { caption: "The empty jig. The engraved SW5 and SW1 mark the board's ends; the header side faces the labels.", add: ["jig"] },
        { caption: "Board front down in the pocket, back up. Solder U1 and the five hotswap sockets.", add: ["pcb", "ics"] },
        { caption: "Sockets: each nests on the board against its stop at the mouth, 2.0 mm past the board edge.", add: ["sockets", "tails"] },
        { caption: "Headers: in the 8° cradle, pushed outward against the stop.", add: ["headers"] },
      ],
      legend: ["jig", "pcb", "ics", "sockets", "headers"],
    },
    keyModule: {
      badge: "KEY MODULE", model: "key-module", view: [0.9, -0.9, -0.9],
      explode: { pcb: [0, 0, -16], ics: [0, 0, -16], sockets: [0, 0, -16], headers: [0, 0, -16], tails: [0, 0, -16],
                 floor: [0, 0, -34], switches: [0, 0, 16], legs: [0, 0, 16] },
      sequence: [
        { caption: "The shell, seen from below: it is open at the bottom.", add: ["shell"] },
        { caption: "The board drops in along its normal, back (connectors) toward you.", add: ["pcb", "ics", "sockets", "headers", "tails"] },
        { caption: "The floor goes on and takes four M2 corner screws. Its pillars press the board against the ledges.", add: ["floor"] },
        { caption: "Turn it over: the switches click into the plate and their legs into the hotswap sockets.", add: ["switches", "legs"], view: [0.9, -0.9, 1.1] },
      ],
      legend: ["shell", "floor", "pcb", "switches", "headers", "sockets"],
    },
    join: {
      badge: "SLIDE-ON", view: [0.35, -1.2, 0.9],
      layers: () => [{ model: "key-module" }, { model: "key-module", power: 1, slide: true }],
      slide: { label: "Slide-on", max: 24, value: 24 },
      legend: ["shell", "floor", "headers", "sockets", "switches"],
    },
    terminator: {
      badge: "TERMINATOR", model: "terminator", view: [0.3, -1.2, -0.6],
      legend: ["terminator", "termHeaders", "ghost", "ghostBoard"], hidden: ["ghostBoard"],
    },
    mcu: {
      badge: "MCU MODULE · UPTURNED", model: "mcu-module", mcu: true, flip: true, view: [0.4, -0.9, -1.2],
      explode: { view: [0, 0, -14], socketBoard: [0, 0, -22], sockets: [0, 0, -22], tails: [0, 0, -22],
                 battery: [0, 0, -30], nano: [0, 0, -30], switch: [0, 0, -30], reset: [0, 0, -30], jack: [0, 0, -44], mcuFloor: [0, 0, -44] },
      sequence: [
        { caption: "The MCU shell, upturned: parts go in from what will be the bottom.", add: ["mcuShell"] },
        { caption: "The nice!view drops into its pocket under the window, header edge toward the nano's end.", add: ["view"] },
        { caption: "The socket board sits exactly where a key module's board would, over the view.", add: ["socketBoard", "sockets", "tails"] },
        { caption: "The battery, at the SW1 end, between its stops.", add: ["battery"] },
        { caption: "The nice!nano at the SW5 end: port into the end wall slot, onto the post and under the prop.", add: ["nano"] },
        { caption: "The power switch and the reset button into their chambers on the free face.", add: ["switch", "reset"] },
        { caption: "The floor, with the battery jack on its seat, closes it all. Six M2 screws.", add: ["jack", "mcuFloor"] },
      ],
      legend: ["mcuShell", "mcuFloor", "socketBoard", "view", "nano", "battery", "switch", "reset", "jack", "ghost"],
      hidden: ["ghost", "ghostBoard", "legs", "switches", "pcb", "ics", "headers", "shell", "floor"],
    },
  };

  // ---- three.js, loaded once on demand through the page's import map
  let three;
  function loadThree() {
    three ||= Promise.all([
      import("three"),
      import("three/addons/controls/OrbitControls.js"),
      import("three/addons/loaders/GLTFLoader.js"),
      import("three/addons/utils/BufferGeometryUtils.js"),
    ]).then(([THREE, { OrbitControls }, { GLTFLoader }, BGU]) => ({ THREE, OrbitControls, GLTFLoader, BGU }));
    return three;
  }

  // parsed models: name -> [{ name, geometry, edges }], geometry in the module frame
  const parsed = {};
  function model(name) {
    parsed[name] ||= Promise.all([loadThree(), loadModel(name)]).then(async ([{ THREE, GLTFLoader, BGU }, buf]) => {
      const gltf = await new GLTFLoader().parseAsync(buf, "");
      const out = [];
      gltf.scene.updateMatrixWorld(true);
      gltf.scene.traverse(o => {
        if (!o.isMesh) return;
        const raw = (o.name || o.parent?.name || "").replace(/_/g, " ");
        let g = o.geometry.clone().applyMatrix4(o.matrixWorld);
        g.deleteAttribute("normal");
        g = BGU.toCreasedNormals(g, THREE.MathUtils.degToRad(32));
        out.push({ name: raw.replace(/ (\d)$/, ""), geometry: g, edges: new THREE.EdgesGeometry(g, 28) });
      });
      return out;
    });
    return parsed[name];
  }

  const ease = t => t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;

  class Viewer {
    constructor(el) {
      this.el = el;
      this.id = el.dataset.scene;
      this.spec = SCENES[this.id];
      this.state = {
        explode: 0, seq: this.spec.sequence ? 0 : -1,
        count: this.spec.count?.value, slide: this.spec.slide?.value ?? 0, clip: null,
        hidden: new Set(this.spec.hidden || []),
      };
      this.buildDom();
    }

    buildDom() {
      const s = this.spec, el = this.el;
      el.innerHTML = "";
      this.stage = Object.assign(document.createElement("div"), { className: "stage" });
      this.stage.innerHTML = `<div class="badge3d">${s.badge || "3D"}</div>
        <div class="hud"><button class="btn" data-a="reset" title="Reset the view">⟲ View</button>
        <button class="btn" data-a="full" title="Full screen">⤢</button></div>
        <div class="tip3d"></div><div class="loading">Loading model…</div>`;
      el.appendChild(this.stage);
      this.tip = this.stage.querySelector(".tip3d");
      this.stage.querySelector("[data-a=reset]").onclick = () => this.frame(true);
      this.stage.querySelector("[data-a=full]").onclick = () => {
        el.classList.toggle("fullscreen");
        document.body.style.overflow = el.classList.contains("fullscreen") ? "hidden" : "";
        requestAnimationFrame(() => this.resize());
      };
      const c = document.createElement("div");
      c.className = "controls3d";
      const row = (label, html) => `<div class="row3d"><label>${label}</label>${html}</div>`;
      let h = "";
      if (s.sequence) h += `<div class="seq">${s.sequence.map((_, i) => `<button data-i="${i}">${i + 1}</button>`).join("")}
          <button data-i="prev" title="Previous">‹</button><button data-i="next" title="Next">›</button></div>
          <div class="seq-caption"></div>`;
      if (s.count) h += row(s.count.label, `<input type="range" data-c="count" min="${s.count.min}" max="${s.count.max}" step="1" value="${this.state.count}"><output></output>`);
      if (s.slide) h += row(s.slide.label, `<input type="range" data-c="slide" min="0" max="${s.slide.max}" step="0.1" value="${this.state.slide}"><output></output>`);
      if (s.explode) h += row("Explode", `<input type="range" data-c="explode" min="0" max="1" step="0.01" value="${this.state.explode}"><output></output>`);
      if (s.clip) h += row(s.clip.label, `<input type="range" data-c="clip" min="0" max="1" step="0.01" value="1"><output></output>`) +
          `<div class="seq-caption" data-clipnote></div>`;
      if (s.legend) h += `<div class="legend">${s.legend.map(g => `<button data-g="${g}" aria-pressed="${!this.state.hidden.has(g)}"><span class="swatch" style="background:${G[g][1]}"></span>${G[g][0]}</button>`).join("")}</div>`;
      if (s.note) h += `<div class="seq-caption" style="color:var(--muted)">${s.note}</div>`;
      c.innerHTML = h;
      if (h) el.appendChild(c);
      this.controlsEl = c;
      c.querySelectorAll(".seq button").forEach(b => b.onclick = () => {
        const n = s.sequence.length, i = b.dataset.i;
        this.setSeq(i === "prev" ? Math.max(0, this.state.seq - 1) : i === "next" ? Math.min(n - 1, this.state.seq + 1) : +i);
      });
      c.querySelectorAll("input[type=range]").forEach(r => r.oninput = () => this.onRange(r.dataset.c, +r.value));
      c.querySelectorAll(".legend button").forEach(b => b.onclick = () => {
        const g = b.dataset.g, hide = !this.state.hidden.has(g);
        hide ? this.state.hidden.add(g) : this.state.hidden.delete(g);
        b.setAttribute("aria-pressed", String(!hide));
        this.applyVisibility();
      });
      this.updateOutputs();
    }

    updateOutputs() {
      const out = (k, v) => { const o = this.controlsEl.querySelector(`[data-c=${k}] + output`); if (o) o.textContent = v; };
      out("count", this.state.count);
      out("slide", `${this.state.slide.toFixed(1)} mm`);
      out("explode", `${Math.round(this.state.explode * 100)} %`);
      if (this.spec.sequence) {
        this.controlsEl.querySelectorAll(".seq button[data-i]").forEach(b => b.classList.toggle("on", +b.dataset.i === this.state.seq));
        this.controlsEl.querySelector(".seq-caption").textContent = this.spec.sequence[this.state.seq].caption;
      }
      if (this.spec.clip && this.clipMax) {
        const h = this.state.clip ?? this.clipMax, m = this.spec.clip.marker;
        out("clip", `${h.toFixed(2)} mm`);
        this.controlsEl.querySelector("[data-clipnote]").innerHTML = h < m - 0.05
          ? `Printing… the pause comes at <b>${m.toFixed(2)} mm</b>.`
          : Math.abs(h - m) <= 0.2 ? `<b>Paused at ${m.toFixed(2)} mm:</b> drop the headers into their pockets, lay the wire in its groove, resume.`
          : `Past the pause: ${(h - m).toFixed(2)} mm printed over the pockets.`;
      }
    }

    onRange(k, v) {
      if (k === "count") { this.state.count = v; this.rebuild(); }
      if (k === "slide") { this.state.slide = v; this.applySlide(); }
      if (k === "explode") { this.state.explode = v; this.animateTo(); }
      if (k === "clip") { this.state.clip = this.clipMax * v; this.applyClip(); }
      this.updateOutputs();
      this.invalidate();
    }

    setSeq(i) {
      this.state.seq = i;
      this.updateOutputs();
      const view = this.spec.sequence[i].view || this.spec.view;
      if (this.live) { this.animateTo(); this.tweenView(view); }
    }

    // groups visible at the current sequence step; null when the scene has no sequence
    seqGroups() {
      if (!this.spec.sequence) return null;
      const on = new Set();
      this.spec.sequence.slice(0, this.state.seq + 1).forEach(st => st.add.forEach(g => on.add(g)));
      return on;
    }

    async activate() {
      if (this.live || this.starting) return;
      this.starting = true;
      try {
        const { THREE, OrbitControls } = await loadThree();
        this.T = THREE;
        const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: "low-power" });
        renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
        renderer.localClippingEnabled = true;
        renderer.outputColorSpace = THREE.SRGBColorSpace;
        this.renderer = renderer;
        this.stage.prepend(renderer.domElement);
        this.scene = new THREE.Scene();
        this.scene.add(new THREE.HemisphereLight(0xffffff, 0xb8b0a2, 2.4));
        const key = new THREE.DirectionalLight(0xffffff, 2.0); key.position.set(1, 2, 1.5); this.scene.add(key);
        const rim = new THREE.DirectionalLight(0xffffff, 0.7); rim.position.set(-1.5, 0.5, -1); this.scene.add(rim);
        const under = new THREE.DirectionalLight(0xffffff, 0.9); under.position.set(0.5, -2, 1); this.scene.add(under);
        this.camera = new THREE.PerspectiveCamera(28, 1, 1, 5000);
        this.controls = new OrbitControls(this.camera, renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.addEventListener("change", () => this.invalidate());
        this.controls.addEventListener("start", () => { this.controls.autoRotate = false; });
        this.root = new THREE.Group();
        this.root.rotation.x = -Math.PI / 2;   // module frame is z-up
        this.scene.add(this.root);
        this.content = new THREE.Group();
        if (this.spec.flip) this.content.rotation.y = Math.PI;
        this.root.add(this.content);
        this.ro = new ResizeObserver(() => this.resize());
        this.ro.observe(this.stage);
        this.stage.addEventListener("pointermove", this.onHover = e => this.hover(e));
        this.stage.addEventListener("pointerleave", () => { this.tip.style.opacity = 0; });
        this.live = true;
        await this.rebuild(true);
        this.stage.querySelector(".loading")?.remove();
        this.controls.autoRotate = !!this.spec.spin && !matchMedia("(prefers-reduced-motion: reduce)").matches;
        this.controls.autoRotateSpeed = 0.8;
        this.invalidate();
      } catch (err) {
        console.error(`viewer ${this.id}:`, err);
        this.stage.querySelector(".loading")?.remove();
        this.stage.insertAdjacentHTML("beforeend", `<div class="fallback">${err.message.startsWith("Opened from disk") ? "" : "The 3D view needs WebGL and a network connection for three.js.<br>"}${err.message}</div>`);
      } finally { this.starting = false; }
    }

    deactivate() {
      if (!this.live) return;
      this.saved = { pos: this.camera.position.clone(), target: this.controls.target.clone() };
      this.live = false;
      cancelAnimationFrame(this.raf);
      this.ro.disconnect();
      this.controls.dispose();
      this.renderer.dispose();
      this.renderer.forceContextLoss();
      this.renderer.domElement.remove();
      this.stage.removeEventListener("pointermove", this.onHover);
      this.meshes?.forEach(m => { m.material.dispose(); m.children[0]?.material.dispose(); });
      this.renderer = this.scene = null;
      this.stage.insertAdjacentHTML("beforeend", `<div class="loading">Loading model…</div>`);
    }

    async rebuild(first) {
      const THREE = this.T, s = this.spec;
      const layers = s.layers ? s.layers(this.state.count) : [{ model: s.model, mcu: s.mcu }];
      const loaded = await Promise.all(layers.map(l => model(l.model)));
      if (!this.live) return;
      this.content.clear();
      this.meshes = [];
      this.layers = [];
      const N = new THREE.Matrix4().fromArray(DATA.neighbour);
      layers.forEach((l, li) => {
        const g = new THREE.Group();
        g.matrixAutoUpdate = false;
        g.userData.base = new THREE.Matrix4();
        for (let k = 0; k < (l.power || 0); k++) g.userData.base.multiply(N);
        g.userData.slide = !!l.slide;
        this.layers.push(g);
        this.content.add(g);
        loaded[li].forEach(p => {
          const group = classify(p.name, { mcu: l.mcu });
          if (!group || !G[group]) return;
          if (l.only && !l.only.includes(group)) return;
          if (l.hide && l.hide.includes(group)) return;
          const [, colour, opacity = 1] = G[group];
          const mat = new THREE.MeshStandardMaterial({
            color: colour, roughness: 0.62, metalness: 0.04, side: THREE.DoubleSide,
            transparent: opacity < 1, opacity, depthWrite: opacity >= 1,
            polygonOffset: true, polygonOffsetFactor: 1, polygonOffsetUnits: 1,
          });
          const mesh = new THREE.Mesh(p.geometry, mat);
          mesh.userData = { group, t: 1, opacity };
          const edges = new THREE.LineSegments(p.edges, new THREE.LineBasicMaterial({
            color: 0x1d1a24, transparent: true, opacity: opacity < 1 ? 0.12 : 0.28 }));
          mesh.add(edges);
          g.add(mesh);
          this.meshes.push(mesh);
        });
      });
      this.scene.updateMatrixWorld(true);   // boxes below read world matrices
      if (s.bed) this.addBed();
      this.applySlide();
      this.applyVisibility(true);
      if (s.clip) this.setupClip();
      if (first) this.frame(false);
      this.invalidate();
    }

    addBed() {
      const THREE = this.T;
      const box = new THREE.Box3().setFromObject(this.content);
      const size = box.getSize(new THREE.Vector3()), c = box.getCenter(new THREE.Vector3());
      const w = Math.max(size.x, size.z) * 1.5;   // world after the z-up rotation: bed spans x and z
      const grid = new THREE.GridHelper(Math.ceil(w / 10) * 10, Math.ceil(w / 10), 0x8a8070, 0xb8b0a0);
      grid.material.transparent = true; grid.material.opacity = 0.35;
      grid.position.set(c.x, box.min.y - 0.05, c.z);
      this.scene.add(grid);
      this.bed = grid;
    }

    setupClip() {
      const THREE = this.T;
      const box = new THREE.Box3().setFromObject(this.content);
      this.clipMin = box.min.y;
      this.clipMax = box.max.y - box.min.y;
      this.plane = new THREE.Plane(new THREE.Vector3(0, -1, 0), box.max.y);
      this.meshes.forEach(m => {
        if (this.spec.clip.group.includes(m.userData.group)) {
          m.material.clippingPlanes = [this.plane];
          m.children[0].material.clippingPlanes = [this.plane];
        }
      });
      const marker = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), new THREE.MeshBasicMaterial({
        color: 0xe8871e, transparent: true, opacity: 0.16, side: THREE.DoubleSide, depthWrite: false }));
      const size = box.getSize(new THREE.Vector3()), c = box.getCenter(new THREE.Vector3());
      marker.scale.set(size.x * 1.25, size.z * 1.1, 1);
      marker.rotation.x = -Math.PI / 2;
      marker.position.set(c.x, this.clipMin + this.spec.clip.marker, c.z);
      this.scene.add(marker);
      this.marker = marker;
      this.applyClip();
    }

    applyClip() {
      if (!this.plane) return;
      const h = this.state.clip ?? this.clipMax;
      this.plane.constant = this.clipMin + h;
      this.marker.visible = h < this.clipMax - 0.01;
      this.updateOutputs();
    }

    applySlide() {
      const THREE = this.T;
      this.layers.forEach(g => {
        g.matrix.copy(g.userData.base);
        if (g.userData.slide) g.matrix.multiply(new THREE.Matrix4().makeTranslation(this.state.slide, 0, 0));
        g.matrixWorldNeedsUpdate = true;
      });
    }

    applyVisibility(instant) {
      const seq = this.seqGroups();
      this.meshes.forEach(m => {
        const g = m.userData.group;
        m.userData.wanted = !this.state.hidden.has(g) && (!seq || seq.has(g) || !this.inSequence(g));
        if (instant) { m.userData.t = m.userData.wanted ? 1 : 0; }
      });
      this.animateTo(instant);
    }

    inSequence(g) { return this.spec.sequence.some(st => st.add.includes(g)); }

    // moves every mesh toward its wanted visibility and explode offset
    animateTo(instant) {
      if (!this.live) return;
      const seq = this.seqGroups();
      if (seq) this.meshes.forEach(m => {
        const g = m.userData.group;
        m.userData.wanted = !this.state.hidden.has(g) && (seq.has(g) || !this.inSequence(g));
      });
      this.anim = !instant;
      this.step(instant ? 1 : 0);
      this.invalidate();
    }

    // one animation tick; returns true while anything is still moving
    step(dt) {
      const ex = this.spec.explode || {};
      let moving = false;
      this.meshes.forEach(m => {
        const u = m.userData, target = u.wanted ? 1 : 0;
        if (u.t !== target) {
          u.t = dt >= 1 ? target : Math.min(1, Math.max(0, u.t + Math.sign(target - u.t) * dt * 2.2));
          moving = true;
        }
        const e = ease(u.t), v = ex[u.group];
        const amount = Math.max(this.state.explode, 1 - e);
        if (v) m.position.set(v[0] * amount, v[1] * amount, v[2] * amount);
        m.visible = u.t > 0.001;
        m.material.opacity = u.opacity * e;
        m.material.transparent = u.opacity < 1 || e < 1;
        m.material.depthWrite = !m.material.transparent;
        m.children[0].material.opacity = (u.opacity < 1 ? 0.12 : 0.28) * e;
      });
      return moving;
    }

    frame(animated) {
      const THREE = this.T;
      if (!this.live) return;
      if (!animated && this.saved) {
        this.camera.position.copy(this.saved.pos); this.controls.target.copy(this.saved.target);
        this.saved = null; this.resize(); return;
      }
      this.tweenView(this.spec.view || ISO, animated);
    }

    tweenView(view, animated = true) {
      const THREE = this.T;
      this.scene.updateMatrixWorld(true);
      const box = new THREE.Box3();
      this.meshes.forEach(m => { if (m.userData.wanted !== false) box.expandByObject(m); });
      if (box.isEmpty()) box.setFromObject(this.content);
      const sphere = box.getBoundingSphere(new THREE.Sphere());
      const [vx, vy, vz] = this.spec.flip ? [-view[0], view[1], -view[2]] : view;   // flip: turned about the column
      const d = new THREE.Vector3(vx, vz, -vy).normalize();   // module frame -> world
      const dist = sphere.radius / Math.sin(THREE.MathUtils.degToRad(this.camera.fov / 2)) * 0.92;
      const to = sphere.center.clone().add(d.multiplyScalar(dist)), target = sphere.center.clone();
      this.resize();
      if (!animated) { this.camera.position.copy(to); this.controls.target.copy(target); this.controls.update(); this.invalidate(); return; }
      const from = this.camera.position.clone(), fromT = this.controls.target.clone(), t0 = performance.now();
      this.tween = now => {
        const k = ease(Math.min(1, (now - t0) / 700));
        this.camera.position.lerpVectors(from, to, k);
        this.controls.target.lerpVectors(fromT, target, k);
        return k < 1;
      };
      this.invalidate();
    }

    resize() {
      if (!this.live) return;
      const w = this.stage.clientWidth, h = this.stage.clientHeight;
      if (!w || !h) return;
      this.renderer.setSize(w, h, false);
      this.camera.aspect = w / h;
      this.camera.updateProjectionMatrix();
      this.invalidate();
    }

    hover(e) {
      if (!this.live || !this.meshes) return;
      const THREE = this.T, r = this.stage.getBoundingClientRect();
      const p = new THREE.Vector2(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
      const ray = new THREE.Raycaster();
      ray.setFromCamera(p, this.camera);
      const hit = ray.intersectObjects(this.meshes.filter(m => m.visible && m.userData.t > 0.5), false)
        .find(h => !this.plane || !h.object.material.clippingPlanes || this.plane.distanceToPoint(h.point) >= 0);
      if (hit) {
        this.tip.textContent = G[hit.object.userData.group][0];
        this.tip.style.left = `${e.clientX - r.left}px`;
        this.tip.style.top = `${e.clientY - r.top}px`;
        this.tip.style.opacity = 1;
      } else this.tip.style.opacity = 0;
    }

    invalidate() {
      if (!this.live || this.raf) return;
      let last = performance.now();
      const loop = now => {
        this.raf = 0;
        if (!this.live) return;
        const dt = Math.min(0.05, (now - last) / 1000); last = now;
        let more = this.controls.update();
        if (this.anim) { this.anim = this.step(dt); more ||= this.anim; }
        if (this.tween) { if (!this.tween(now)) this.tween = null; else more = true; }
        if (this.controls.autoRotate) more = true;
        this.renderer.render(this.scene, this.camera);
        if (more) this.raf = requestAnimationFrame(loop);
      };
      this.raf = requestAnimationFrame(loop);
    }
  }

  const viewers = window.RoamyViewers = new Map();   // exposed for debugging from the console
  const io = new IntersectionObserver(entries => entries.forEach(e => {
    const v = viewers.get(e.target);
    e.isIntersecting ? v.activate() : v.deactivate();
  }), { rootMargin: "400px 0px" });

  function init() {
    document.querySelectorAll(".viewer[data-scene]").forEach(el => {
      if (!SCENES[el.dataset.scene]) { console.error(`viewer: no scene "${el.dataset.scene}"`); return; }
      const v = new Viewer(el);
      viewers.set(el, v);
      io.observe(el);
    });
    addEventListener("keydown", e => {
      if (e.key === "Escape") document.querySelectorAll(".viewer.fullscreen").forEach(el => {
        el.classList.remove("fullscreen"); document.body.style.overflow = ""; viewers.get(el).resize();
      });
    });
  }
  document.readyState === "loading" ? addEventListener("DOMContentLoaded", init) : init();
})();
