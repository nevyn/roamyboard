# Docs

- [Glossary](glossary.md): canonical terms for modules, the case, the joint and the firmware, with a labelled diagram.
- [Firmware](firmware.md): the ZMK module and config in `zmk/`: how the chain is read, key module count and anchors, the builds, the socket board pins, building and flashing.
- [Assembly guide](assembly/index.html): printing, soldering, assembling, flashing and testing a half, with 3D views from the Cadova model. Published at https://nevyn.github.io/roamyboard/docs/assembly/ by `.github/workflows/pages.yml` whenever the guide changes on main. To read a local copy, `git lfs pull` (the models are in Git LFS), run `python3 -m http.server` in the repo root and open `/docs/assembly/`.
- [Keyboard layout](layout/index.html): the default layout, layer by layer, drawn as two halves of modules with a key module count per half. The page fetches and parses `zmk/config/roamyboard_split.keymap` when it loads, so it has no copy of the layout. Published at https://nevyn.github.io/roamyboard/docs/layout/ by the same workflow. `node --test docs/layout/keymap.test.js` checks the parser and that every binding in both keymaps has a label.
