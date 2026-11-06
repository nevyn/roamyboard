// The assembly guide as a wordless, black-and-white flat-pack manual.
//
// Every sheet is an A4 SVG in mm. 3D figures come from the guide's Cadova models (../models/*.glb), drawn as line art:
// a pass writes each pixel's face normal and part id, and an edge pass blackens pixels whose neighbours differ in
// part, in normal or in depth. Annotations are SVG drawn over the figure, placed by projecting points in the model frame.
import * as THREE from "three";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { SHEETS } from "./sheets.js";

const MODELS = new URL("../models/", import.meta.url);
const NEIGHBOUR = new THREE.Matrix4().fromArray(window.ROAMY_DATA.neighbour);
const RES = 12;   // render pixels per sheet mm

// ---- models: name -> [{ group, geometry }] in the module frame (mm: x across from the socket face, y along from
// the SW1 end, z up from the board's back)

function classify(raw) {
  const [whole, hex] = raw.split("#"), [part, sub] = whole.split("/");
  if (sub === "header tails") return part.startsWith("Key module") ? "ghost" : "headerTails";
  if (sub === "socket tails") return part.startsWith("Key module") ? "ghost" : "tails";
  const board = { "000000": "ics", "008000": "pcb", "808080": "sockets", "ffa500": "headers", "ffff00": "tails",
                  "ffffff": "legs", "ffffff80": "switches" };
  switch (part) {
    case "Shell": return "shell";
    case "Floor": return "floor";
    case "Board": return board[hex];
    case "Key module shell": case "Key module board": return "ghost";
    case "Socket board": return hex === "008000" ? "socketBoard" : board[hex];
    case "Jig": return "jig";
    case "Painted support": return "support";
    case "Terminator": return "terminator";
    case "Headers": return "termHeaders";
    case "Trackpad": return "pad";
    case "Switch": return "vSwitch";
    case "Sockets": return "looseSockets";
    case "Socket tails": return "looseTails";
    default: return part.startsWith("MCU ") ? part.slice(4) : "print";
  }
}

const models = {};
function load(name) {
  return models[name] ||= (async () => {
    const url = new URL(`${name}.glb`, MODELS);
    const res = await fetch(url);
    if (!res.ok) throw new Error(`${url}: HTTP ${res.status}`);
    const buf = await res.arrayBuffer();
    if (new TextDecoder().decode(buf.slice(0, 4)) !== "glTF") throw new Error(`${url} is not a GLB; run git lfs pull`);
    const gltf = await new GLTFLoader().parseAsync(buf, "");
    gltf.scene.updateMatrixWorld(true);
    const parts = [];
    gltf.scene.traverse(o => {
      if (!o.isMesh) return;
      const raw = (o.userData.name || o.name || o.parent?.name || "").replace(/_/g, " ").replace(/ (\d)$/, "");
      parts.push({ group: classify(raw), geometry: o.geometry.clone().applyMatrix4(o.matrixWorld) });
    });
    return parts;
  })();
}

// ---- procedural parts, in the module frame like the models

// Choc keycaps on the five switches, which are as wide along y as across, at equal pitch.
function keycaps(parts) {
  const box = new THREE.Box3();
  for (const p of parts) if (p.group === "switches") { p.geometry.computeBoundingBox(); box.union(p.geometry.boundingBox); }
  const { min, max } = box, w = max.x - min.x, pitch = (max.y - min.y - w) / 4;
  const x0 = min.x, x1 = max.x, top = max.z;
  return [0, 1, 2, 3, 4].map(i => keycap((x0 + x1) / 2, min.y + i * pitch + w / 2, top));
}

// A Choc keycap whose underside centre sits at (x, y, z).
function keycap(x = 0, y = 0, z = 0) {
  const g = new THREE.CylinderGeometry(Math.SQRT1_2 * 15.5, Math.SQRT1_2 * 17.5, 3, 4, 1).toNonIndexed();
  g.rotateY(Math.PI / 4); g.rotateX(Math.PI / 2);
  g.scale(1, 16.5 / 17.5, 1);
  g.translate(x, y, z + 1.6);
  return g;
}

// An M2 x 5 pan head screw, point down, head top at z = 0.
function screw() {
  const head = new THREE.CylinderGeometry(1.9, 1.9, 1.3, 24).toNonIndexed();
  head.rotateX(Math.PI / 2); head.translate(0, 0, -0.65);
  const shaft = new THREE.CylinderGeometry(1, 0.6, 5, 16).toNonIndexed();
  shaft.rotateX(Math.PI / 2); shaft.translate(0, 0, -3.8);
  return [head, shaft];
}

// ---- line-art renderer

const ID_VERT = `varying vec3 vPos;
void main() { vec4 mv = modelViewMatrix * vec4(position, 1.0); vPos = mv.xyz; gl_Position = projectionMatrix * mv; }`;
const ID_FRAG = `uniform float uId; varying vec3 vPos;
void main() { vec3 n = normalize(cross(dFdx(vPos), dFdy(vPos))); gl_FragColor = vec4(n * 0.5 + 0.5, uId); }`;
const EDGE_VERT = `varying vec2 vUv; void main() { vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }`;
const EDGE_FRAG = `uniform sampler2D tN; uniform sampler2D tD; uniform vec2 px; uniform float r; uniform float range;
uniform float dThresh; uniform float nThresh; varying vec2 vUv;
float edgeAt(vec2 o, vec4 c, float d) {
  vec4 c2 = texture2D(tN, vUv + o); float d2 = texture2D(tD, vUv + o).x * range;
  if (abs(c2.a - c.a) > 0.002) return 1.0;
  if (c.a == 0.0) return 0.0;
  if (dot(c.xyz * 2.0 - 1.0, c2.xyz * 2.0 - 1.0) < nThresh) return 1.0;
  return abs(d2 - d) > dThresh ? 1.0 : 0.0;
}
void main() {
  vec4 c = texture2D(tN, vUv); float d = texture2D(tD, vUv).x * range;
  float hits = 0.0;
  for (int i = 0; i < 12; i++) {
    float a = float(i) * 0.5235988;
    vec2 dir = vec2(cos(a), sin(a)) * px;
    hits = max(hits, edgeAt(dir * r, c, d));
    hits = max(hits, edgeAt(dir * r * 0.5, c, d));
  }
  gl_FragColor = vec4(vec3(1.0 - hits), 1.0);
}`;

let renderer;
function getRenderer() {
  renderer ||= new THREE.WebGLRenderer({ antialias: false, preserveDrawingBuffer: true });
  renderer.setPixelRatio(1);
  return renderer;
}

function itemMatrix(it) {
  const m = new THREE.Matrix4();
  for (let k = 0; k < (it.pow || 0); k++) m.multiply(NEIGHBOUR);
  if (it.at) m.multiply(new THREE.Matrix4().makeTranslation(...it.at));
  if (it.rot) {
    const p = it.pivot || [16, 56.8, 0];
    m.multiply(new THREE.Matrix4().makeTranslation(...p));
    m.multiply(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(...it.rot.map(THREE.MathUtils.degToRad), "ZYX")));
    m.multiply(new THREE.Matrix4().makeTranslation(-p[0], -p[1], -p[2]));
  }
  return m;
}

// The triangles whose centroid lies within y0..y1, in the module frame.
function clipY(geometry, [y0, y1]) {
  const src = geometry.index ? geometry.toNonIndexed() : geometry, a = src.attributes.position, keep = [];
  for (let i = 0; i < a.count; i += 3) {
    const y = (a.getY(i) + a.getY(i + 1) + a.getY(i + 2)) / 3;
    if (y >= y0 && y <= y1) for (let k = 0; k < 3; k++) keep.push(a.getX(i + k), a.getY(i + k), a.getZ(i + k));
  }
  const out = new THREE.BufferGeometry();
  out.setAttribute("position", new THREE.Float32BufferAttribute(keep, 3));
  return out;
}

// Resolves an item to [{ geometry, matrix, key }]: `key` names the part for the id buffer.
async function resolve(it, index) {
  const base = itemMatrix(it);
  const out = [];
  const add = (geometry, group, extra) => {
    const m = base.clone();
    const ex = it.explode?.[group];
    if (ex) m.premultiply(new THREE.Matrix4().makeTranslation(...ex));
    out.push({ geometry: it.clip ? clipY(geometry, it.clip) : geometry, matrix: m, key: `${index}:${group}${extra ?? ""}` });
  };
  if (it.screw) { for (const g of screw()) add(g, "screw"); return out; }
  if (it.cap) { add(keycap(), "cap"); return out; }
  if (it.geometry) { add(it.geometry, "custom"); return out; }
  const parts = await load(it.model);
  const want = g => it.groups ? it.groups.includes(g) : g !== "ghost" && g !== "support";
  for (const p of parts) if (want(p.group)) add(p.geometry, p.group);
  if (it.groups?.includes("caps")) keycaps(parts).forEach((g, i) => add(g, "caps", i));
  return out;
}

// Renders a figure: { view, items, zoom?, line? } into a w x h mm box. Returns the image and a projector from the
// world frame to sheet mm.
async function renderFigure(fig, box) {
  const W = Math.round(box.w * RES), H = Math.round(box.h * RES);
  const meshes = (await Promise.all(fig.items.map(resolve))).flat();
  const keys = [...new Set(meshes.map(m => m.key))];
  const scene = new THREE.Scene();
  for (const m of meshes) {
    const mat = new THREE.ShaderMaterial({ uniforms: { uId: { value: (keys.indexOf(m.key) + 1) / 255 } },
      vertexShader: ID_VERT, fragmentShader: ID_FRAG, side: THREE.DoubleSide, blending: THREE.NoBlending });
    const mesh = new THREE.Mesh(m.geometry, mat);
    mesh.matrixAutoUpdate = false; mesh.matrix.copy(m.matrix); mesh.updateMatrixWorld(true);
    scene.add(mesh);
  }

  const dir = new THREE.Vector3(...fig.view).normalize();
  const bbox = new THREE.Box3();
  for (const o of scene.children) { o.geometry.computeBoundingBox(); bbox.union(o.geometry.boundingBox.clone().applyMatrix4(o.matrixWorld)); }
  const centre = bbox.getCenter(new THREE.Vector3()), radius = bbox.getSize(new THREE.Vector3()).length();
  const cam = new THREE.OrthographicCamera();
  cam.up.set(...(fig.up ?? (Math.abs(dir.z) > 0.99 ? [0, 1, 0] : [0, 0, 1])));
  cam.position.copy(centre).addScaledVector(dir, radius + 50);
  cam.lookAt(centre); cam.updateMatrixWorld(true);

  const lo = new THREE.Vector3(Infinity, Infinity, Infinity), hi = lo.clone().negate(), v = new THREE.Vector3();
  for (const o of scene.children) {
    const a = o.geometry.attributes.position;
    const step = Math.max(1, Math.floor(a.count / 4000));
    for (let i = 0; i < a.count; i += step) {
      v.fromBufferAttribute(a, i).applyMatrix4(o.matrixWorld).applyMatrix4(cam.matrixWorldInverse);
      lo.min(v); hi.max(v);
    }
  }
  const pad = fig.pad ?? 0.06;
  const s = (fig.zoom ?? 1) * Math.min(W / ((hi.x - lo.x) * (1 + 2 * pad)), H / ((hi.y - lo.y) * (1 + 2 * pad)));
  const cx = (lo.x + hi.x) / 2 + (fig.shift?.[0] ?? 0) / s * RES, cy = (lo.y + hi.y) / 2 - (fig.shift?.[1] ?? 0) / s * RES;
  Object.assign(cam, { left: cx - W / 2 / s, right: cx + W / 2 / s, top: cy + H / 2 / s, bottom: cy - H / 2 / s,
                       near: -hi.z - 5, far: -lo.z + 5 });
  cam.updateProjectionMatrix();

  const r = getRenderer();
  r.setSize(W, H, false);
  const rt = new THREE.WebGLRenderTarget(W, H, { minFilter: THREE.NearestFilter, magFilter: THREE.NearestFilter,
                                                 depthTexture: new THREE.DepthTexture(W, H) });
  r.setRenderTarget(rt); r.setClearColor(0x000000, 0); r.clear(); r.render(scene, cam);

  const quad = new THREE.Mesh(new THREE.PlaneGeometry(2, 2), new THREE.ShaderMaterial({
    vertexShader: EDGE_VERT, fragmentShader: EDGE_FRAG, depthTest: false, depthWrite: false,
    uniforms: { tN: { value: rt.texture }, tD: { value: rt.depthTexture }, px: { value: new THREE.Vector2(1 / W, 1 / H) },
                r: { value: (fig.line ?? 0.42) * RES / 2 }, range: { value: cam.far - cam.near },
                dThresh: { value: fig.depth ?? 1.0 }, nThresh: { value: Math.cos(THREE.MathUtils.degToRad(fig.crease ?? 25)) } } }));
  const post = new THREE.Scene(); post.add(quad);
  r.setRenderTarget(null); r.setClearColor(0xffffff, 1); r.clear(); r.render(post, new THREE.Camera());
  const url = r.domElement.toDataURL("image/png");
  rt.dispose(); rt.depthTexture.dispose(); quad.geometry.dispose(); quad.material.dispose();
  for (const o of scene.children) o.material.dispose();

  const project = (p, pow = 0) => {
    const q = new THREE.Vector3(...p);
    for (let k = 0; k < pow; k++) q.applyMatrix4(NEIGHBOUR);
    q.project(cam);
    return [box.x + (q.x + 1) / 2 * box.w, box.y + (1 - q.y) / 2 * box.h];
  };
  return { svg: `<image href="${url}" x="${box.x}" y="${box.y}" width="${box.w}" height="${box.h}"/>`, project };
}

// ---- book

async function build() {
  const book = document.getElementById("book");
  book.replaceChildren();
  let page = 0;
  for (const sheet of SHEETS) {
    page++;
    const el = document.createElement("section");
    el.className = "sheet";
    book.append(el);
    const layers = [];
    for (const f of sheet.figures || []) {
      const out = await renderFigure(f, f.box);
      layers.push(out.svg, f.draw ? f.draw(out.project) : "");
    }
    const draw = sheet.draw ? sheet.draw() : "";
    el.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 210 297" font-family="Verdana, 'Noto Sans', sans-serif">
      <rect width="210" height="297" fill="#fff"/>${layers.join("")}${draw}
      ${sheet.bare ? "" : `<text x="${page % 2 ? 196 : 14}" y="288" font-size="5" text-anchor="${page % 2 ? "end" : "start"}">${page}</text>`}
    </svg>`;
  }
}

build().catch(e => {
  console.error(e);
  document.getElementById("book").insertAdjacentHTML("beforeend", `<pre style="background:#fff;padding:1em">${String(e.stack || e)}</pre>`);
});
