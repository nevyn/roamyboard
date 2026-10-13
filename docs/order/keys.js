// Which key of the firmware's keymap each module position types, and what kind of key it is.
//
// The order page draws modules; the keymap is addressed by column. The two meet at the half's anchor, the end
// that keymap columns are counted from: every half counts from its MCU module (zmk/boards/shields/*/*.overlay).
// The left half's and a unibody's MCU module sit at the right end; the right half is turned by 180 degrees, so
// its MCU module sits at its left end. A half with fewer key modules keeps the columns nearest the middle of
// the keyboard. docs/layout/ draws the same mapping; the two must agree.
(() => {
  "use strict";

  /// Keymap columns the shipped keymaps bind, per half, as [first, last] in the 24-column keymap.
  const BOUND = {
    split: { left: [5, 11], right: [12, 18] },
    unibody: { left: [10, 23] },
  };

  /**
   * The keymap column that a module position addresses.
   * @param layout "split" or "unibody".
   * @param side 0 for the left half or a unibody, 1 for the right half.
   * @param modules How many key modules that half carries.
   * @param index The module's position in the half as drawn, counting from 0 at the half's left end.
   * @returns A keymap column, which may fall outside the bound range when the half has more key modules
   *          than the keymap binds. Callers treat such a column as a key with no binding.
   */
  function keymapColumn(layout, side, modules, index) {
    const bound = BOUND[layout][side ? "right" : "left"];
    // With the MCU module at the right end, the rightmost module keeps the half's last bound column; with it at
    // the left end, the leftmost module keeps the first.
    const mcuAtRightEnd = layout === "unibody" || side === 0;
    return mcuAtRightEnd ? bound[1] - (modules - 1 - index) : bound[0] + index;
  }

  /// How many key modules per half the shipped keymap has bindings for.
  const boundModules = (layout, side) => {
    const b = BOUND[layout][side ? "right" : "left"];
    return b[1] - b[0] + 1;
  };

  // Groups the keycap painter offers, matched on the name describeBinding gives a binding. Layer keys count
  // as modifiers: they are held like one, and they sit under the pinkies with the other modifiers.
  const GROUPS = {
    modifiers: {
      label: "modifiers",
      // Names carry the other names a key goes by, as in "Left Alt (Option)", so match the head of the name.
      test: (d) => d.kind === "layer" || /^(Left|Right) (Shift|Control|Alt|GUI)\b/.test(d.name) || /^Shift \+ /.test(d.name),
    },
    numerics: { label: "numbers", test: (d) => /^[0-9]$/.test(d.name) },
    whitespace: {
      label: "whitespace",
      test: (d) => ["Space", "Backspace", "Return", "Tab", "Forward delete", "Escape"].includes(d.name),
    },
  };

  /// The group a described binding belongs to, or null. A binding belongs to at most one group.
  function groupOf(description) {
    if (!description) return null;
    for (const [name, group] of Object.entries(GROUPS)) if (group.test(description)) return name;
    return null;
  }

  const api = { BOUND, GROUPS, keymapColumn, boundModules, groupOf };
  if (typeof module === "object" && module.exports) module.exports = api;
  if (typeof window === "object") window.RoamyKeys = api;
})();
