// The parts of a Roamyboard, shared by the assembly guide's parts list and the order page's price list.
// The guide reads name, source, per and note; the order page also reads price and grams.
//
//   per     quantity per key module (key), per key position (keyRow, so it follows the row count), MCU
//           module (mcu), terminator module (term), per build (build), or { text } for something measured
//           by eye. Rows with no `per` are group headings.
//   price   { amount, currency, each } — `each` is how many units `amount` buys, so a pack price stays a
//           pack price and the order page can round up to whole packs. Omitted where no vendor publishes
//           one; `pricedBy` names the chooser that supplies it instead.
//   grams   PLA for a printed part, from the solid volume of the published print model at 1.24 g/cm³.
//           Slicer infill makes the real figure lower, so this is an upper bound.
(() => {
  "use strict";
  const lcscUrl = id => `https://www.lcsc.com/product-detail/${id}.html`;
  const lcsc = id => `<a href="${lcscUrl(id)}">LCSC ${id}</a>`;
  const at = (id, amount, note) => ({ amount, currency: "USD", each: 1, vendor: "LCSC", url: lcscUrl(id), note });
  const splitkb = handle => ({ vendor: "splitkb", url: `https://splitkb.com/products/${handle}` });

  // Euro reference rates, European Central Bank, 2026-10-06. Hardcoded: the site is static and cannot fetch
  // a rate. Units of the currency per euro.
  const RATES = { EUR: 1, USD: 1.1269, SEK: 11.2425 };
  const RATES_DATE = "2026-10-06";

  const PARTS = [
    { group: "Key module board" },
    {
      name: "Key module PCB, fab revision v5",
      source: `<a href="https://github.com/nevyn/roamyboard/tree/main/electronics/KeyModule/fab">Gerbers and BOM</a>`,
      per: { key: 1, mcu: 1 },
      note: "One per MCU module becomes its socket board.",
      price: { amount: 4.10, currency: "USD", each: 5, vendor: "JLCPCB", url: "https://jlcpcb.com/",
               note: "5-piece price for a 17 × 100 mm board, 2 layers. Five columns panelise into one 100 × 100 mm board for the same money." },
    },
    { name: "74HC165D, SOIC-16 (U1)", source: lcsc("C5613"), per: { key: 1 },
      price: at("C5613", 0.0906, "50+ price tier") },
    { name: "100 nF, 0805 (C1)", source: lcsc("C49678"), per: { key: 1 },
      price: at("C49678", 0.0191, "20+ price tier") },
    { name: "10 kΩ, 0805 (R1 to R5)", source: lcsc("C17414"), per: { keyRow: 1 },
      price: at("C17414", 0.0034, "100+ price tier") },
    { name: "Kailh Choc hotswap socket CPG135001S30", source: lcsc("C5333465"), per: { keyRow: 1 },
      price: { amount: 6.57, currency: "EUR", each: 50, ...splitkb("kailh-hotswap-sockets"),
               note: "LCSC lists the part with no price; splitkb sells the same socket in fifties." } },
    { name: "Socket, hanxia HX PM2.54-1x3P WT", source: lcsc("C46061767") + "; Harwin M20-7910342R fits", per: { key: 3, mcu: 3 },
      price: at("C46061767", 0.1426, "50+ price tier") },
    { name: "Header, hanxia HX PZ2.54-1x3P WT", source: lcsc("C46061676") + "; Harwin M20-8890345R fits", per: { key: 3, term: 2 },
      price: at("C46061676", 0.0589, "10+ price tier") },
    { name: "Kailh Choc v1 switch, any", source: "splitkb", per: { keyRow: 1 }, pricedBy: "switch" },
    { name: "Choc keycap", source: "splitkb", per: { keyRow: 1 }, pricedBy: "keycaps" },

    { group: "MCU module" },
    { name: "nice!nano v2", source: "typeractive.xyz", per: { mcu: 1 },
      price: { amount: 25.00, currency: "USD", each: 1, vendor: "Typeractive", url: "https://typeractive.xyz/products/nice-nano" } },
    { name: "nice!view", source: "typeractive.xyz", per: { mcu: 1 },
      price: { amount: 20.00, currency: "USD", each: 1, vendor: "Typeractive", url: "https://typeractive.xyz/products/nice-view" } },
    { name: "LiPo battery, 750 mAh, 48 × 30 × 5 mm", source: "electrokit 41016063", per: { mcu: 1 },
      price: { amount: 129, currency: "SEK", each: 1, vendor: "electrokit", url: "https://www.electrokit.com/batteri-lipo-3.7v-750mah",
               note: "Swedish VAT included" } },
    { name: "Battery jack, JST S2B-PH-K-S", source: lcsc("C173752"), per: { mcu: 1 },
      price: at("C173752", 0.0399, "20+ price tier") },
    { name: "Slide switch, Alps SSSS811101", source: lcsc("C109335"), per: { mcu: 1 },
      price: at("C109335", 0.1487, "5+ price tier") },
    { name: "Reset button, Panasonic EVQPUC02K", source: lcsc("C79174"), per: { mcu: 1 },
      price: at("C79174", 0.1745, "5+ price tier") },

    { group: "Hardware" },
    { name: "M2 × 5 mm self-tapping screw", source: "splitkb", per: { key: 4, mcu: 6 },
      price: { amount: 5.74, currency: "EUR", each: 50, ...splitkb("m2-screws"),
               note: "splitkb's M2 × 5 are machine screws; no vendor was found for a self-tapping M2 × 5." } },
    { name: "Hookup wire, thin", source: "for the MCU module", per: { text: "a few colours" } },
    { name: "Insulated wire, up to 1.3 mm across", source: "for the terminator module", per: { text: "25 mm per terminator" } },
    { name: "Two-component epoxy", source: "superglue as a fallback", per: { text: "one pack" }, note: "glues the connectors to the board" },

    { group: "Printed" },
    { name: "Key module shell and floor", source: "<code>key-module-print.3mf</code>", per: { key: 1 }, link: "#key-print", grams: 11.73 },
    { name: "MCU module shell and floor", source: "<code>mcu-module-print.3mf</code>", per: { mcu: 1 }, link: "#mcu-print", grams: 35.79 },
    { name: "Terminator module", source: "<code>terminator-print.3mf</code>", per: { term: 1 }, link: "#term-print", grams: 18.15 },
    { name: "Solder jig", source: "<code>solder-jig-print.3mf</code>", per: { build: 1 }, link: "#jig-print", grams: 24.30 },
  ];

  const FILAMENT = { amount: 16.99, currency: "EUR", perKg: true, vendor: "Bambu Lab",
                     url: "https://eu.store.bambulab.com/products/pla-basic-filament" };

  const api = { PARTS, RATES, RATES_DATE, FILAMENT };
  if (typeof module === "object" && module.exports) module.exports = api;
  if (typeof window === "object") window.RoamyParts = api;
})();
