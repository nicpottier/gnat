// gnat case for the LilyGO T-Display S3 AMOLED touch
//
// three parts: a shallow tray the board rests in on corner risers, a snap
// on face whose full height skirt forms the case sides, and an angled foot
// that clamps the DE1's horizontal frame lip. dimensions marked MEASURE
// should be verified with calipers.

// ---------------------------------------------------------------- board
// measured with calipers on the actual unit
pcb_l = 54.6;        // board length
pcb_w = 25.5;        // board width
fit = 0.4;           // clearance around the board

// heights measured from the pcb underside. the board rests on the case
// itself: the tall chips sit on the floor and shallow pads under the
// molex jacks make up the difference so the board sits level
board_h = 7.5;       // pcb underside to glass top
under_molex = 3.1;   // molex jacks hang below the pcb (10.6 overall)
under_chips = 3.6;   // tallest chips at the other end (11.1 overall)
molex_pad_w = 4;     // molex pad reach inward from the end wall

// the glass covers nearly the whole face and pokes through the face window
// sitting flush with the case front
glass_l = 53.29;
glass_w = 24.39;
glass_t = 1.09;
glass_r = 2.2;       // corner radius, tuned against test prints
glass_clr = 0.15;    // window clearance around the glass, per side
glass_off_x = 0;     // glass offset from board center, + toward the usb end

// the molex plugs sit on the end edge flanking the usb port and protrude
// a touch, the end wall gets relief either side of the usb cutout
molex_depth = 0.9;   // relief depth into the end wall

// the two side buttons protrude slightly from the long edges
btn_from = 9.5;      // button zone start, from the pcb's usb end edge
btn_w = 2;           // button width
btn_clr = 1;         // relief margin either side of the buttons
btn_depth = 0.9;     // relief depth into the long walls

// usb-c cutout on the right wall, sized for the cable plug housing. the
// port is centered across the board and mid mounted: its bottom sits
// level with the pcb underside (7.5mm below the glass top) and it spans
// 8.5mm (17mm measured from either pcb edge to the port's far side)
usb_w = 8.5;               // port shell width (2 x 17 measured - pcb width)
usb_h = 3.2;               // port shell height
usb_center_h = usb_h / 2;  // port center above the pcb underside
usb_plug_w = 13;           // cable plug housing clearance width
usb_plug_h = 7;            // cable plug housing clearance height

// pcb underside height above the floor, set by the chips resting on it
under_h = under_chips;

// ---------------------------------------------------------------- tray
// a shallow box, the face skirt forms most of the case sides
wall = 1.6;          // tray wall thickness
floor_t = 3.4;       // tray floor thickness, houses the dovetail channel
rim_h = 2.0;         // wall rise above the riser tops, captures the pcb edge
tray_il = pcb_l + fit;
tray_iw = pcb_w + fit;
tray_l = tray_il + 2 * wall;
tray_w = tray_iw + 2 * wall;
tray_h = floor_t + under_h + rim_h;

// ---------------------------------------------------------------- face
face_t = 1.2;        // face plate thickness, the glass sits flush with its top
skirt_t = 1.2;
snap_d = 0.6;        // snap ridge depth
snap_below_rim = 1.8;
face_fit = 0.15;     // skirt to tray clearance
skirt_stop = 2.0;    // skirt ends short of the back so the foot rail and
                     // neck plate pass underneath

// total front to back thickness, back of the floor to the glass top
case_d = floor_t + under_h + board_h;
skirt_h = case_d - face_t - skirt_stop;

// ---------------------------------------------------------------- mount
// two foot variants share the same clamp: the standard 30 degree lean and
// a laid back 45 for easier viewing from above. the steeper lean drops
// the case back further, so that variant needs a taller neck to keep the
// case clear of the clamp and the machine lip
tilt = 30;           // degrees the face leans back from vertical
tilt45 = 45;         // the laid back variant
lip_t = 1.7;         // de1 horizontal lip, measured
clamp_tol = 0;       // the bare lip size tested as a perfect fit
clamp_gap = lip_t + clamp_tol;
clamp_jaw = 8;       // how far the jaws reach back over the lip
clamp_wall = 1.4;    // jaw wall thickness
clamp_w = 34;        // clamp width along the case
neck_h = 8.5;        // rise between clamp and the case bottom edge, tall
                     // enough that the neck stays clear of the lip path
neck_h45 = 12;       // taller neck for the 45 degree foot
neck_t = 5;          // neck thickness

// dovetail joining the foot to the tray back, channel opens at the bottom
// edge so gravity seats the case onto the rail
dove_w = 11;         // channel width at the surface
dove_uc = 1.8;       // undercut each side at depth
dove_d = 2.0;        // channel depth into the floor
dove_len = 12.5;     // how far up the back the channel runs
dove_fit = 0.25;     // rail to channel clearance per side

explode = 0;         // set to ~15 to see the face lifted off
layout = "print"; // "assembly" or "print" for the plated parts

$fn = 48;

// dovetail cross sections shared by the tray channel and the foot rail
module dove_channel_2d() {
  polygon([[-dove_w / 2, 0.01], [dove_w / 2, 0.01],
           [dove_w / 2 + dove_uc, -dove_d], [-dove_w / 2 - dove_uc, -dove_d]]);
}

module dove_rail_2d() {
  polygon([[-dove_w / 2 + dove_fit, 0.2], [dove_w / 2 - dove_fit, 0.2],
           [dove_w / 2 + dove_uc - dove_fit, -dove_d + 0.15],
           [-dove_w / 2 - dove_uc + dove_fit, -dove_d + 0.15]]);
}

// box with rounded vertical corners, footprint l x w with the same corner
// radius as the glass so the face outline echoes the window
module rounded_box(l, w, h, r) {
  hull()
    for (x = [r, l - r])
      for (y = [r, w - r])
        translate([x, y, 0])
          cylinder(r = r, h = h);
}

// the screen window, a rounded rect matching the glass outline so the
// glass pokes through and sits flush, shared by the face and the gauge
module glass_window(h) {
  win_l = glass_l + 2 * glass_clr;
  win_w = glass_w + 2 * glass_clr;
  r = glass_r + glass_clr;
  translate([wall + tray_il / 2 + glass_off_x, wall + tray_iw / 2, 0])
    hull()
      for (x = [-win_l / 2 + r, win_l / 2 - r])
        for (y = [-win_w / 2 + r, win_w / 2 - r])
          translate([x, y, 0])
            cylinder(r = r, h = h);
}

// shallow tray, origin back-bottom-left, face up
module tray() {
  // the molex plugs run right up to the usb port, so the end wall relief
  // reaches from the side walls all the way to the port's own edges
  usb_flank = tray_iw / 2 - usb_w / 2;

  difference() {
    cube([tray_l, tray_w, tray_h]);

    // cavity
    translate([wall, wall, floor_t])
      cube([tray_il, tray_iw, tray_h]);

    // usb notch where the plug bottom dips below the wall top
    translate([tray_l - wall - 1,
               wall + tray_iw / 2 - usb_plug_w / 2,
               floor_t + under_h + usb_center_h - usb_plug_h / 2])
      cube([wall + 2, usb_plug_w, tray_h]);

    // relief for the molex plugs flanking the usb port on the end edge
    for (y = [wall, tray_w - wall - usb_flank])
      translate([tray_l - wall, y, floor_t])
        cube([molex_depth, usb_flank, tray_h]);

    // relief for the side buttons protruding slightly from both long edges
    for (y = [wall - btn_depth, tray_w - wall])
      translate([tray_l - wall - fit / 2 - btn_from - btn_w - btn_clr, y, floor_t])
        cube([btn_w + 2 * btn_clr, btn_depth, tray_h]);

    // dovetail channels recessed in the back, open at both long edges so
    // the case can mount on the foot either way up for a flipped screen
    for (m = [0, 1])
      translate([0, m * tray_w, 0])
        mirror([0, m, 0])
          translate([tray_l / 2, -1, 0])
            rotate([-90, 0, 0])
              linear_extrude(dove_len + 1)
                dove_channel_2d();

    // snap grooves along the outer long walls
    for (y = [0, tray_w])
      translate([tray_l / 2, y, tray_h - snap_below_rim])
        rotate([45, 0, 0])
          cube([tray_l * 0.7, snap_d * 1.4, snap_d * 1.4], center = true);
  }

  // shallow pads under the molex plugs, they carry the usb end while the
  // tall chips rest directly on the floor at the other end, the pads run
  // into the end wall relief so the overhanging plugs sit fully supported
  for (y = [wall, tray_w - wall - usb_flank])
    translate([tray_l - wall - molex_pad_w, y, floor_t])
      cube([molex_pad_w + molex_depth, usb_flank, under_chips - under_molex]);
}

// snap on face, its skirt runs nearly the full case depth and forms the
// sides, the plate underside seats on the display surround
module face() {
  ow = tray_w + 2 * (skirt_t + face_fit);
  ol = tray_l + 2 * (skirt_t + face_fit);

  difference() {
    // plate and skirt as one solid, corners rounded like the window
    translate([-skirt_t - face_fit, -skirt_t - face_fit, 0])
      rounded_box(ol, ow, face_t + skirt_h, glass_r);

    // hollow above the plate where the board and tray nest in
    translate([-face_fit, -face_fit, face_t])
      cube([tray_l + 2 * face_fit, tray_w + 2 * face_fit, skirt_h + 1]);

    // screen window matching the glass outline
    translate([0, 0, -1])
      glass_window(face_t + 2);

    // usb opening through the skirt only, centered on the port, the plate
    // stays unbroken so the face looks complete from the front
    translate([tray_l - 1, wall + tray_iw / 2 - usb_plug_w / 2,
               case_d - (floor_t + under_h + usb_center_h + usb_plug_h / 2)])
      cube([skirt_t + face_fit + 2, usb_plug_w, usb_plug_h]);
  }

  // snap ridges aligned with the tray grooves
  for (y = [-face_fit, tray_w + face_fit])
    translate([tray_l / 2, y, case_d - tray_h + snap_below_rim])
      rotate([45, 0, 0])
        cube([tray_l * 0.7, snap_d * 1.2, snap_d * 1.2], center = true);
}

// c channel that slides forward onto the horizontal frame lip, the slot
// opens toward the machine at the back
module clamp(gap = clamp_gap) {
  h = 2 * clamp_wall + gap;
  difference() {
    translate([-clamp_jaw, -clamp_w / 2, 0])
      cube([clamp_jaw + clamp_wall, clamp_w, h]);
    translate([-clamp_jaw - 1, -clamp_w / 2 - 1, clamp_wall])
      cube([clamp_jaw + 1, clamp_w + 2, gap]);
  }
}

// everything in mounted orientation, clamp at origin, display facing +x
// and leaning back by tilt degrees
clamp_h = 2 * clamp_wall + clamp_gap;

module case_placed(part = "tray") {
  translate([0, 0, clamp_h + neck_h - 0.1])
    rotate([0, -tilt, 0])
      translate([-case_d + 1, 0, 0])
        rotate([90, 0, 90])
          translate([-tray_l / 2, 0, 0])
            if (part == "tray") tray();
            else translate([0, 0, case_d + explode]) mirror([0, 0, 1]) face();
}

// a solid wedge from the clamp top up to the tray's back plane, plus a
// backrest panel running up the case back. the backrest spans the full
// clamp width so when the foot prints on its side it is present from the
// first layer and backs the rail in every layer
module neck(t = tilt, nh = neck_h) {
  hull() {
    translate([-neck_t / 2, -clamp_w / 2, clamp_h - 2])
      cube([neck_t, clamp_w, 2]);
    translate([0, 0, clamp_h + nh - 0.1])
      rotate([0, -t, 0])
        translate([-case_d - 1, -clamp_w / 2, 0])
          cube([case_d + 1, clamp_w, 2]);
  }
  translate([0, 0, clamp_h + nh - 0.1])
    rotate([0, -t, 0])
      translate([-case_d - 1, -clamp_w / 2, 0])
        cube([2, clamp_w, dove_len]);
}

// the dovetail rail in tray model space, sized to slide into the channel.
// its width ends are clipped at 45 degrees so the rail grows out of the
// backrest without overhangs when the foot prints on its side
module rail_tray_space() {
  half = dove_w / 2 + dove_uc;
  translate([tray_l / 2, 1, 0])
    rotate([-90, 0, 0])
      linear_extrude(dove_len - 3)
        intersection() {
          dove_rail_2d();
          polygon([[-half, 1], [-half, 0], [0, -half], [half, 0], [half, 1]]);
        }
}

// a filler that slides into whichever dovetail channel the foot doesn't
// use, sitting flush with the case back. modeled in print orientation:
// wide face on the bed so the flanks taper inward with no overhangs
module dove_plug() {
  difference() {
    rotate([-90, 0, 0])
      linear_extrude(dove_len - 0.3)
        polygon([[-dove_w / 2 - dove_uc + dove_fit, 0],
                 [dove_w / 2 + dove_uc - dove_fit, 0],
                 [dove_w / 2 - dove_fit, -dove_d + 0.15],
                 [-dove_w / 2 + dove_fit, -dove_d + 0.15]]);

    // hollowed slot at the outer end, leaving a foot on each side, so a
    // coin can hook the plug and drag it back out of the channel
    translate([-3, -1, -1])
      cube([6, 3, dove_d + 2]);
  }
}

// the complete foot, clamp, neck and the rail that carries the case. the
// neck hull can sag toward the clamp, so the lip's path is carved back
// out at the end: the slot plane runs clear all the way back and nothing
// sits below the jaw top beyond the jaw tip
module foot(t = tilt, nh = neck_h) {
  difference() {
    union() {
      clamp();
      neck(t, nh);
      translate([0, 0, clamp_h + nh - 0.1])
        rotate([0, -t, 0])
          translate([-case_d + 1, 0, 0])
            rotate([90, 0, 90])
              translate([-tray_l / 2, 0, 0])
                rail_tray_space();
    }

    // the slot plane, extended endlessly toward the machine
    translate([-100, -clamp_w / 2 - 1, clamp_wall])
      cube([100, clamp_w + 2, clamp_gap]);

    // the approach beyond the jaw tip, up past the jaw top
    translate([-100, -clamp_w / 2 - 1, clamp_wall])
      cube([100 - clamp_jaw, clamp_w + 2, clamp_h + 1 - clamp_wall]);
  }
}

// the laid back variant, same clamp with a taller neck
module foot45() {
  foot(tilt45, neck_h45);
}

module assembly() {
  foot();
  case_placed("tray");
  color("steelblue") case_placed("face");
}

// all three parts flat on the bed in their printing orientations
module print_layout() {
  // tray on its back
  tray();

  // face on its front
  translate([0, tray_w + 15, 0])
    face();

  // foot on its side, profile flat on the bed
  translate([-18, 32, clamp_w / 2])
    rotate([90, 0, 0])
      foot();
}

if (layout == "print") {
  print_layout();
} else if (layout == "foot") {
  // just the standard foot in print orientation
  translate([0, 0, clamp_w / 2])
    rotate([90, 0, 0])
      foot();
} else if (layout == "foot45") {
  // the laid back foot in print orientation
  translate([0, 0, clamp_w / 2])
    rotate([90, 0, 0])
      foot45();
} else if (layout == "tray") {
  // just the tray, for reprinting the bottom alone
  tray();
} else if (layout == "face") {
  // just the face in print orientation
  face();
} else if (layout == "plug") {
  // filler for the unused dovetail channel
  dove_plug();
} else {
  assembly();
}
