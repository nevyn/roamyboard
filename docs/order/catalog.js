// What a Roamyboard can be ordered as: filament colours, switches, keycaps, curves, and the rates that turn a
// configuration into a price. Every vendor figure here is quoted from the vendor's own page; `source` names it.
//
// Keycap and switch prices are pack prices, because that is how splitkb sells them: a build of 70 keys buys
// seven packs of ten and keeps the change. The order page rounds up to whole packs and shows the leftovers.
(() => {
  "use strict";

  // Bambu Lab's own colour table, BambuStudio resources/profiles/BBL/filament/filaments_color_codes.json,
  // cross-checked against the hex PDFs linked from each store page. Both materials print the case equally
  // well; PETG HF is tougher and has no purples or pinks, PLA Basic has the wider palette.
  // [name, hex, Bambu filament code]
  const FILAMENTS = {
    "PLA Basic": [
      ["Jade White", "#FFFFFF", "10100"], ["Black", "#000000", "10101"], ["Silver", "#A6A9AA", "10102"],
      ["Gray", "#8E9089", "10103"], ["Light Gray", "#D1D3D5", "10104"], ["Dark Gray", "#545454", "10105"],
      ["Red", "#C12E1F", "10200"], ["Beige", "#F7E6DE", "10201"], ["Magenta", "#EC008C", "10202"],
      ["Pink", "#F55A74", "10203"], ["Hot Pink", "#F5547C", "10204"], ["Maroon Red", "#9D2235", "10205"],
      ["Orange", "#FF6A13", "10300"], ["Pumpkin Orange", "#FF9016", "10301"], ["Yellow", "#F4EE2A", "10400"],
      ["Gold", "#E4BD68", "10401"], ["Sunflower Yellow", "#FEC600", "10402"], ["Bambu Green", "#00AE42", "10501"],
      ["Mistletoe Green", "#3F8E43", "10502"], ["Bright Green", "#BECF00", "10503"], ["Blue", "#0A2989", "10601"],
      ["Blue Grey", "#5B6579", "10602"], ["Cyan", "#0086D6", "10603"], ["Cobalt Blue", "#0056B8", "10604"],
      ["Turquoise", "#00B1B7", "10605"], ["Purple", "#5E43B7", "10700"], ["Indigo Purple", "#482960", "10701"],
      ["Brown", "#9D432C", "10800"], ["Bronze", "#847D48", "10801"], ["Cocoa Brown", "#6F5034", "10802"],
    ],
    "PETG HF": [
      ["White", "#FFFFFF", "33100"], ["Gray", "#ADB1B2", "33101"], ["Black", "#000000", "33102"],
      ["Dark Gray", "#515151", "33103"], ["Red", "#EB3A3A", "33200"], ["Orange", "#F75403", "33300"],
      ["Yellow", "#FFD00B", "33400"], ["Cream", "#F9DFB9", "33401"], ["Green", "#00AE42", "33500"],
      ["Lime Green", "#6EE53C", "33501"], ["Forest Green", "#39541A", "33502"], ["Blue", "#002E96", "33600"],
      ["Lake Blue", "#1F79E5", "33601"], ["Peanut Brown", "#875718", "33801"],
    ],
  };
  const FILAMENT_SOURCE = {
    label: "Bambu Lab",
    url: "https://eu.store.bambulab.com/products/pla-basic-filament",
    note: "Colour names and hex values from Bambu Lab's own filament colour table in Bambu Studio.",
  };

  // Kailh Choc v1 only, since that is the switch the key module's hotswap sockets take. These are the ones
  // worth offering: a light-to-heavy spread of each feel, plus the two silent Ambients. splitkb stocks more.
  // Prices incl. 21% Dutch VAT, per pack of ten.
  const SWITCHES = [
    { name: "Choc Pink", feel: "linear", force: 20, pack: 10, price: 5.99, handle: "kailh-low-profile-choc-switches" },
    { name: "Choc Purple", feel: "linear", force: 25, pack: 10, price: 5.99, handle: "kailh-low-profile-choc-switches" },
    { name: "Choc Pro Red", feel: "linear", force: 35, pack: 10, price: 5.99, handle: "kailh-low-profile-choc-switches" },
    { name: "Choc Red", feel: "linear", force: 50, pack: 10, price: 5.59, handle: "kailh-low-profile-choc-switches" },
    { name: "Choc Black", feel: "linear", force: 60, pack: 10, price: 5.59, handle: "kailh-low-profile-choc-switches" },
    { name: "Choc Brown", feel: "tactile", force: 50, pack: 10, price: 5.59, handle: "kailh-low-profile-choc-switches" },
    { name: "Choc Burnt Orange", feel: "tactile", force: 70, pack: 10, price: 5.99, handle: "kailh-low-profile-choc-switches" },
    { name: "Choc White", feel: "clicky", force: 50, pack: 10, price: 5.59, handle: "kailh-low-profile-choc-switches" },
    { name: "Choc Navy", feel: "clicky", force: 60, pack: 10, price: 5.99, handle: "kailh-low-profile-choc-switches" },
    { name: "Ambients Nocturnal", feel: "silent linear", force: 20, pack: 10, price: 10.25, handle: "ambients-kailh-low-profile-choc-switches" },
    { name: "Ambients Twilight", feel: "silent linear", force: 35, pack: 10, price: 10.25, handle: "ambients-kailh-low-profile-choc-switches" },
  ];

  // MBK low-profile blanks, the Choc-spacing cap that fits a 1u column. Every colour sells in the same three
  // packs, so a key's profile decides which pack it comes out of. Prices incl. 21% Dutch VAT.
  const CAP_PACKS = {
    flat:   { label: "flat", pack: 10, price: 4.45 },
    homing: { label: "homing", pack: 2, price: 1.79, note: "a tactile ridge, for finding the home row by touch" },
    convex: { label: "convex", pack: 2, price: 0.99, note: "dished the other way; the usual choice for thumb keys" },
  };
  // [name, hex, splitkb product handle]. The twelve MBK PBT colours carry splitkb's own storefront swatch
  // values. White and black are eyeballed: splitkb's swatch for those two is the CSS keyword, and no photo
  // of the blank caps shows their material colour.
  const CAPS = [
    ["White", "#E9E8E4", "blank-mbk-choc-low-profile-keycaps"],
    ["Black", "#1F1F1F", "blank-mbk-choc-low-profile-keycaps"],
    ["Steel Grey", "#818586", "mbk-pbt-coloured-blank-keycaps"],
    ["Shadow Grey", "#484647", "mbk-pbt-coloured-blank-keycaps"],
    ["Chili Red", "#E5272A", "mbk-pbt-coloured-blank-keycaps"],
    ["Tiger Orange", "#FD7B35", "mbk-pbt-coloured-blank-keycaps"],
    ["Cream Yellow", "#FEF486", "mbk-pbt-coloured-blank-keycaps"],
    ["Mint Green", "#53DFBF", "mbk-pbt-coloured-blank-keycaps"],
    ["Arctic Blue", "#44C0E5", "mbk-pbt-coloured-blank-keycaps"],
    ["Ocean Blue", "#0B5790", "mbk-pbt-coloured-blank-keycaps"],
    ["Violet Purple", "#6942AB", "mbk-pbt-coloured-blank-keycaps"],
    ["Lavender Purple", "#A170CF", "mbk-pbt-coloured-blank-keycaps"],
    ["Fuchsia Pink", "#F1449E", "mbk-pbt-coloured-blank-keycaps"],
    ["Rose Pink", "#FD95A8", "mbk-pbt-coloured-blank-keycaps"],
  ];
  const CAP_SOURCE = {
    label: "splitkb",
    url: "https://splitkb.com/products/mbk-pbt-coloured-blank-keycaps",
    note: "Swatch colours are splitkb's own, from the colour chips on the product page; white and black are eyeballed.",
  };

  // The joint angle between neighbouring boards. It is the only thing that sets the curve, and it costs
  // nothing to change: the connectors fix the 20.85 mm board spacing, so the angle only reshapes the printed
  // keystone. A different angle does mean a different solder jig, since the header is soldered at that angle.
  const CURVES = [
    { angle: 0, name: "Flat", fits: "a desk, or a very wide leg" },
    { angle: 6, name: "Gentle", fits: "a heavier thigh" },
    { angle: 8, name: "Standard", fits: "most legs; the angle the design ships at", default: true },
    { angle: 10, name: "Curved", fits: "a slimmer thigh" },
    { angle: 12, name: "Tight", fits: "a slim thigh, or sitting high on the leg" },
  ];

  // What the page starts on. Named here so a renamed colour or switch breaks a test rather than silently
  // falling back to a grey swatch.
  const DEFAULTS = {
    material: "PLA Basic", caseColour: "Indigo Purple", capColour: "Lavender Purple", switch: "Choc Brown",
  };

  const ROWS = { min: 3, max: 8, built: 5 };
  // A half takes as many key modules as you like; these are the counts worth offering. The firmware reads
  // the chain's length, so the only limit past the keymap's columns is the keymap itself.
  const COLUMNS = { min: 1, max: { split: 10, unibody: 20 }, default: 7 };

  // Nevyn's own pace, soldering included. Hours per module; the terminator is mostly a print with two
  // headers and a wire.
  const ASSEMBLY = { rate: 1000, currency: "SEK", hours: { key: 0.25, mcu: 2, term: 0.5 } };
  const MARKUP = 0.15;   // on parts, to cover the odds and ends that no line item catches

  const api = { FILAMENTS, FILAMENT_SOURCE, SWITCHES, CAPS, CAP_PACKS, CAP_SOURCE, CURVES, DEFAULTS, ROWS, COLUMNS, ASSEMBLY, MARKUP };
  if (typeof module === "object" && module.exports) module.exports = api;
  if (typeof window === "object") window.RoamyCatalog = api;
})();
