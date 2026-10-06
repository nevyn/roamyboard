// The curve, as the case model computes it. Neighbouring boards sit a fixed distance apart along the board
// plane, set by the connectors, so the joint angle alone decides the arc a half bends through: a bigger angle
// is a tighter curve, and nothing about the electronics changes.
//
// `arcRadius` is the port of `Joint.centre` from case/roamy-v4/Sources/roamy-v4/Frame.swift, which solves for
// the fixed point of the rotation that the tilted header imposes on the neighbour. At the design's 8° it
// returns the 149 mm that case/Case design.md quotes.
(() => {
  "use strict";

  // Module frame numbers behind the joint, from case/roamy-v4/Sources/roamy-v4/Parameters.swift.
  const BOARD = { wall: 1.8, clearance: 0.2, width: 16.85, padInboardEnd: 5.685, mouthPastHeaderEdge: 2.0 };
  const headerEdgeX = BOARD.wall + BOARD.clearance + BOARD.width;

  /// Distance along the board plane from one module's socket mouth to its neighbour's: 20.85 mm.
  const PITCH = headerEdgeX + BOARD.mouthPastHeaderEdge;

  /// Widths at the board plane and thicknesses perpendicular to the top, in mm, per module kind.
  const WIDTH = { key: PITCH, term: 12.0, mcu: 52.4 };
  const THICK = { key: 8.94, term: 8.94, mcu: 10.48 };

  /// How far below the board plane the arc's centre lies, for a joint angle in degrees.
  /// Returns Infinity for 0°, where the boards are coplanar and the keyboard is flat.
  function arcRadius(degrees) {
    if (!degrees) return Infinity;
    const t = degrees * Math.PI / 180, c = Math.cos(t), s = Math.sin(t);
    const rm = [c * PITCH, -s * PITCH];
    const a = 1 - c, b = -s, det = a * a + b * b;
    return -((b * rm[0] + a * rm[1]) / det);
  }

  /// The radius of a curve that sags `drop` mm below a straight edge spanning `span` mm across it: the
  /// sagitta formula. Measuring a thigh this way beats wrapping a tape around it, since only the part the
  /// keyboard rests on matters.
  function radiusFromSagitta(span, drop) {
    if (!(span > 0 && drop > 0)) return null;
    return span * span / (8 * drop) + drop / 2;
  }

  const api = { PITCH, WIDTH, THICK, arcRadius, radiusFromSagitta };
  if (typeof module === "object" && module.exports) module.exports = api;
  if (typeof window === "object") window.RoamyGeometry = api;
})();
