// The landing page's hero gallery: thumbnails pick a slide, ?slide=<name> keeps the choice across a refresh or a
// shared link, and a sideways swipe moves between photo slides. The 3D slide takes no swipe, since dragging orbits
// the model there; its viewer only starts when the slide is shown (assembly/viewer.js starts viewers on screen).
(() => {
  "use strict";

  function init() {
    const gallery = document.querySelector("[data-gallery]");
    if (!gallery) return;
    const slides = [...gallery.querySelectorAll(".slide")];
    const tabs = [...gallery.querySelectorAll("[role=tab]")];
    const caption = gallery.querySelector("[data-caption]:not(.slide)");
    const names = slides.map(s => s.dataset.slide);
    let current = 0;

    function show(i, { focus = false, remember = true } = {}) {
      current = (i + slides.length) % slides.length;
      slides.forEach((s, n) => { s.hidden = n !== current; });
      tabs.forEach((t, n) => {
        t.setAttribute("aria-selected", String(n === current));
        t.tabIndex = n === current ? 0 : -1;
      });
      caption.textContent = slides[current].dataset.caption;
      if (focus) tabs[current].focus();
      if (remember) {
        const q = new URLSearchParams(location.search);
        current ? q.set("slide", names[current]) : q.delete("slide");
        history.replaceState(null, "", `${location.pathname}${q.toString() ? `?${q}` : ""}${location.hash}`);
      }
    }

    tabs.forEach((t, n) => {
      t.onclick = () => show(n);
      t.onkeydown = e => {
        const step = { ArrowLeft: -1, ArrowRight: 1, Home: -current, End: slides.length - 1 - current }[e.key];
        if (step === undefined) return;
        e.preventDefault();
        show(current + step, { focus: true });
      };
    });

    let start = null;
    const stage = gallery.querySelector(".hero-stage");
    stage.addEventListener("pointerdown", e => {
      start = slides[current].dataset.swipe === "off" ? null : { x: e.clientX, y: e.clientY };
    });
    stage.addEventListener("pointerup", e => {
      if (!start) return;
      const dx = e.clientX - start.x, dy = e.clientY - start.y;
      start = null;
      if (Math.abs(dx) > 40 && Math.abs(dx) > 2 * Math.abs(dy)) show(current + (dx < 0 ? 1 : -1));
    });

    const wanted = names.indexOf(new URLSearchParams(location.search).get("slide"));
    show(wanted > 0 ? wanted : 0, { remember: false });
  }
  document.readyState === "loading" ? addEventListener("DOMContentLoaded", init) : init();
})();
