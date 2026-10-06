// Page behaviour for the assembly guide: contents and progress, the build planner and parts list, pinout
// highlighting, lightbox and keyboard navigation. Progress stays in localStorage; the plan is in the URL.
(() => {
  "use strict";
  const $ = (s, el = document) => el.querySelector(s), $$ = (s, el = document) => [...el.querySelectorAll(s)];
  const KEY = "roamy-guide:";
  const store = {
    get: (k, d) => { try { return JSON.parse(localStorage.getItem(KEY + k)) ?? d; } catch { return d; } },
    set: (k, v) => localStorage.setItem(KEY + k, JSON.stringify(v)),
  };
  const DATA = window.ROAMY_DATA || {};

  // ---- build plan, kept in the query string so a link keeps it
  const plan = (() => {
    const q = new URLSearchParams(location.search);
    const layout = q.get("layout") === "unibody" ? "unibody" : "split";
    const keys = Math.min(30, Math.max(1, parseInt(q.get("keys"), 10) || (layout === "unibody" ? 14 : 7)));
    return { layout, keys };
  })();
  const halves = () => plan.layout === "split" ? 2 : 1;
  const counts = () => ({ key: plan.keys * halves(), mcu: halves(), term: halves() });

  const { PARTS } = window.RoamyParts;
  const ROWS = { built: 5 };   // the key module the guide builds has five key positions

  function renderPlan() {
    const c = counts(), have = store.get("bom", {});
    $$("[data-planner=layout] button").forEach(b => b.setAttribute("aria-pressed", String(b.dataset.v === plan.layout)));
    $("[data-planner=keys]").textContent = plan.keys;
    const strip = $(".half-strip");
    strip.innerHTML = Array.from({ length: halves() }, () => `<span class="half"><i class="t" title="terminator module"></i>` +
      `<i class="k" title="key module"></i>`.repeat(plan.keys) + `<i class="m" title="MCU module"></i></span>`).join("");
    $("[data-summary]").textContent = `${halves() === 2 ? "Two halves" : "One half"}: ${c.key} key modules (${c.key * 5} keys), ` +
      `${c.mcu} MCU module${c.mcu > 1 ? "s" : ""} and ${c.term} terminator module${c.term > 1 ? "s" : ""}.`;
    $("[data-bom]").innerHTML = PARTS.map(({ name, source, per, note, link, group }) => {
      if (group) return `<tr class="group"><td colspan="4">${group}</td></tr>`;
      const qty = per.text ?? (per.key || 0) * c.key + (per.keyRow || 0) * c.key * ROWS.built +
        (per.mcu || 0) * c.mcu + (per.term || 0) * c.term + (per.build || 0);
      const got = have[name];
      return `<tr class="${got ? "got" : ""}" data-item="${name}"><td class="have"><input type="checkbox" ${got ? "checked" : ""} aria-label="Have it"></td>
        <td>${link ? `<a href="${link}">${name}</a>` : name}${note ? `<br><span class="summary">${note}</span>` : ""}</td>
        <td>${source || ""}</td><td class="n">${qty}</td></tr>`;
    }).join("");
    $$("[data-bom] input").forEach(i => i.onchange = () => {
      const h = store.get("bom", {}), name = i.closest("tr").dataset.item;
      i.checked ? h[name] = true : delete h[name];
      store.set("bom", h);
      i.closest("tr").classList.toggle("got", i.checked);
    });
    $$("[data-count]").forEach(el => el.textContent = c[el.dataset.count]);
    $$("[data-layout-row]").forEach(tr => tr.style.opacity = tr.dataset.layoutRow === plan.layout ? 1 : 0.45);
    const q = new URLSearchParams(location.search);
    q.set("layout", plan.layout); q.set("keys", plan.keys);
    history.replaceState(null, "", `?${q}${location.hash}`);
  }

  function setupPlanner() {
    $$("[data-planner=layout] button").forEach(b => b.onclick = () => {
      const was = plan.layout;
      plan.layout = b.dataset.v;
      if (was !== plan.layout && plan.keys === (was === "split" ? 7 : 14)) plan.keys = plan.layout === "split" ? 7 : 14;
      renderPlan();
    });
    $$(".stepper button").forEach(b => b.onclick = () => { plan.keys = Math.min(30, Math.max(1, plan.keys + +b.dataset.d)); renderPlan(); });
    renderPlan();
  }

  // ---- contents and progress
  const chapters = $$("section.chapter");
  const steps = $$("article.step:not(.info)");
  function buildToc() {
    $(".toc").innerHTML = chapters.map((ch, i) => `
      <li data-ch="${ch.id}"><a href="#${ch.id}">
        <svg class="ring" viewBox="0 0 22 22"><circle class="bg" cx="11" cy="11" r="9"/><circle class="fg" cx="11" cy="11" r="9" stroke-dasharray="56.5" stroke-dashoffset="56.5"/>
        <text class="num" x="11" y="14.5" text-anchor="middle">${i + 1}</text></svg>${ch.dataset.title}</a>
        <ul>${$$("article.step", ch).map(s => `<li><a href="#${s.id}" data-step-link="${s.id}">${s.dataset.title}</a></li>`).join("")}</ul></li>`).join("");
  }

  function renderProgress() {
    const done = new Set(store.get("done", []));
    steps.forEach(s => s.classList.toggle("is-done", done.has(s.id)));
    $$("[data-step-link]").forEach(a => a.classList.toggle("checked", done.has(a.dataset.stepLink)));
    let all = 0, total = 0;
    chapters.forEach(ch => {
      const own = $$("article.step:not(.info)", ch), n = own.filter(s => done.has(s.id)).length;
      all += n; total += own.length;
      const li = $(`.toc li[data-ch="${ch.id}"]`), frac = own.length ? n / own.length : 0;
      $(".fg", li).style.strokeDashoffset = 56.5 * (1 - frac);
      li.classList.toggle("done", own.length > 0 && n === own.length);
    });
    const pct = total ? Math.round(100 * all / total) : 0;
    $(".overall .bar i").style.width = `${pct}%`;
    $("[data-overall]").textContent = `${all} of ${total} steps · ${pct} %`;
  }

  function toggleDone(step, force) {
    const done = new Set(store.get("done", []));
    const on = force ?? !done.has(step.id);
    on ? done.add(step.id) : done.delete(step.id);
    store.set("done", [...done]);
    renderProgress();
  }

  function setupChecks() {
    const checks = store.get("checks", {});
    $$("ul.check").forEach((ul, u) => {
      const scope = ul.closest("[id]").id;
      $$(":scope > li", ul).forEach((li, i) => {
        const key = `${scope}:${u}:${i}`, box = document.createElement("input");
        box.type = "checkbox"; box.checked = !!checks[key];
        li.prepend(box); li.classList.toggle("ticked", box.checked);
        box.onchange = () => {
          const c = store.get("checks", {});
          box.checked ? c[key] = true : delete c[key];
          store.set("checks", c);
          li.classList.toggle("ticked", box.checked);
          const step = li.closest("article.step:not(.info)"), all = $$("ul.check input", step);
          if (step && all.every(b => b.checked)) toggleDone(step, true);
        };
      });
    });
  }

  // ---- scrollspy: highlight the chapter in view and open its step list
  function setupSpy() {
    let current;
    const io = new IntersectionObserver(entries => {
      entries.forEach(e => { if (e.isIntersecting) current = e.target; });
      if (!current) return;
      const ch = current.closest("section.chapter");
      $$(".toc > li").forEach(li => {
        const on = ch && li.dataset.ch === ch.id;
        li.classList.toggle("open", on);
        $(":scope > a", li).classList.toggle("active", on);
      });
    }, { rootMargin: "-20% 0px -70% 0px" });
    $$("section.chapter, article.step").forEach(el => io.observe(el));
  }

  // ---- step that the reader is on: the first whose bottom is below the top fifth of the screen
  const currentStep = () => $$("article.step").find(s => s.getBoundingClientRect().bottom > innerHeight * 0.2);
  function go(step) {
    if (!step) return;
    step.scrollIntoView({ behavior: matchMedia("(prefers-reduced-motion: reduce)").matches ? "auto" : "smooth", block: "start" });
    history.replaceState(null, "", `${location.search}#${step.id}`);
    flash(step);
  }
  function flash(el) { el.classList.remove("flash"); void el.offsetWidth; el.classList.add("flash"); }

  function setupKeys() {
    addEventListener("keydown", e => {
      if (e.target.closest("input, textarea, select") || e.metaKey || e.ctrlKey || e.altKey) return;
      const all = $$("article.step"), cur = currentStep(), i = all.indexOf(cur);
      if (e.key === "j") go(all[Math.min(all.length - 1, i + 1)]);
      else if (e.key === "k") go(all[Math.max(0, i - 1)]);
      else if (e.key === "x" && cur && !cur.classList.contains("info")) toggleDone(cur);
      else if (e.key === "Escape") $(".lightbox").classList.remove("open");
    });
  }

  // ---- pinout: hovering a signal marks it everywhere
  function setupPinout() {
    $$("[data-signal]").forEach(el => {
      const s = el.dataset.signal;
      el.addEventListener("mouseenter", () => $$(`[data-signal="${s}"]`).forEach(x => x.classList.add("hot")));
      el.addEventListener("mouseleave", () => $$(`[data-signal="${s}"]`).forEach(x => x.classList.remove("hot")));
    });
  }

  function setupLightbox() {
    const box = $(".lightbox"), img = $("img", box);
    $$("img.diagram, .photo img").forEach(i => i.addEventListener("click", () => { img.src = i.src; img.alt = i.alt; box.classList.add("open"); }));
    box.addEventListener("click", () => box.classList.remove("open"));
  }

  function setupChrome() {
    $("[data-action=reset-progress]").onclick = () => {
      if (!confirm("Clear every check mark and ticked part?")) return;
      ["done", "checks", "bom"].forEach(k => localStorage.removeItem(KEY + k));
      $$("ul.check input").forEach(b => { b.checked = false; b.closest("li").classList.remove("ticked"); });
      renderPlan(); renderProgress();
    };
    $(".menu-toggle").onclick = () => document.body.classList.toggle("menu-open");
    $(".sidebar").addEventListener("click", e => { if (e.target.closest("a")) document.body.classList.remove("menu-open"); });
    $$(".done-toggle").forEach(b => b.onclick = () => toggleDone(b.closest("article.step")));
    $$("[data-rev]").forEach(td => td.textContent = DATA.revisions ? `r${DATA.revisions[td.dataset.rev]}` : "?");
    $$("[data-pause]").forEach(el => el.textContent = DATA.terminatorPause?.toFixed(2) ?? "?");
    addEventListener("hashchange", () => { const t = document.getElementById(location.hash.slice(1)); if (t?.matches("article.step")) flash(t); });
  }

  function init() {
    buildToc();
    setupChrome();
    setupPlanner();
    setupChecks();
    renderProgress();
    setupSpy();
    setupKeys();
    setupPinout();
    setupLightbox();
    const t = location.hash && document.getElementById(location.hash.slice(1));
    if (t) setTimeout(() => { t.scrollIntoView(); if (t.matches("article.step")) flash(t); }, 50);
  }
  document.readyState === "loading" ? addEventListener("DOMContentLoaded", init) : init();
})();
