import Foundation
import Cadova

/// Plate-thickness strip with Choc openings of several sizes, labelled, to find the snuggest click on a given
/// printer and filament. 13.7 won on the 2026-09-25 print; reprint after a printer or filament change.
struct ChocCutoutCoupon: Geometry3D {
    var body: any Geometry3D {
        let sizes = [13.5, 13.6, 13.7, 13.8, 13.9, 14.0, 14.1, 14.2]
        let pitch = 19.05
        let margin = 5.0                      // room for the switch flange (15 mm) and a label below each hole
        Box(x: pitch * Double(sizes.count) + 4, y: pitch + 2 * margin, z: P.plateThickness)
            .subtracting {
                Text("coupon \(Revision.label(Revision.coupon))").withFontSize(3.0)
                    .extruded(height: 0.5).aligned(at: .centerXY)
                    .translated(x: pitch * Double(sizes.count) / 2 + 2, y: pitch + 2 * margin - 1.8, z: P.plateThickness - 0.4)
                for (i, s) in sizes.enumerated() {
                    let cx = 2 + pitch * (Double(i) + 0.5)
                    Box(x: s, y: s, z: P.plateThickness + 2).aligned(at: .centerXY)
                        .translated(x: cx, y: margin + pitch / 2, z: -1)
                    Text(String(format: "%.1f", s)).withFontSize(3.0)
                        .extruded(height: 0.5).aligned(at: .centerXY)
                        .translated(x: cx, y: 1.8, z: P.plateThickness - 0.4)
                }
            }
    }
}
