// SPDX-License-Identifier: GPL-2.0-or-later
// Maiskolben THT (board rev. 1.3c) - 3D printable desk enclosure
//
// Screwless: two shells, six button caps and an iron holder.
//   * base   - tilted wedge (default 15 deg); standoffs with locating pins
//              hold the PCB, a dovetail channel underneath takes the holder
//   * lid    - frames the display, guides the caps and snaps onto the base
//              with four hooks; its bosses clamp the PCB onto the pins
//   * holder - one part for both sides: slides into the dovetail channel from
//              the right, or turned by 180 degrees from the left
//
// Render a single part: openscad -D 'part="lid"' -o lid.stl maiskolben_tht_case.scad
// Parts: "assembly", "exploded", "bottom", "plate", "lid", "base", "caps",
//        "fitcheck", "holder"

part = "assembly";

/* [Board, from the Eagle layout] */
pcb_w = 72.24;
pcb_d = 80.77;
pcb_t = 1.6;
pcb_holes = [[3.175, 4.445], [69.064, 4.445], [3.175, 76.325], [69.064, 76.325]];

/* [Component heights above the PCB top - measure your unit] */
display_center = [33.6, 37.4];  // active area center (board coordinates)
display_active = [35.04, 28.03];
display_top_z = 12.5;           // display glass surface
button_top_z = 13.0;            // tactile plunger top (measured: 13 mm switches)
inner_h = 15.0;                 // free height under the lid (Nano on headers)

/* [Buttons and LED] */
buttons = [
	// x, y, symbol: 0 = plain, 1 = power, 2 = up, 3 = down
	[23.625, 9.88, 0],   // T1
	[33.625, 9.88, 0],   // T2
	[43.625, 9.88, 0],   // T3
	[63.984, 9.88, 1],   // power
	[63.984, 37.439, 2], // up
	[63.984, 27.279, 3]  // down
];
led_pos = [63.959, 18.58];

/* [USB access to the Nano (mini-B socket faces the right wall)] */
usb = true;
usb_pos = [60.3, 69.3];       // socket mouth X, socket center Y (board coordinates)
usb_z = 11.5;                 // socket center above the PCB top (Nano top measured at 13-14 mm)
usb_plug = [11.0, 7.5];       // plug overmold width and height

/* [Jacks on the left edge: y center, notch width, notch top z] */
dc_jack = [68.35, 11.0, 12.5];
tip_jack = [49.8, 9.0, 8.0];

/* [Shell] */
wall = 2.4;
top_t = 2.0;
clr = 0.6;            // PCB to wall clearance
left_clr = 4.0;       // jacks protrude 3.6 mm beyond the PCB edge
corner_r = 5.0;
edge_r = 1.6;         // rounded top edge of the lid
joint_z = -4.6;       // lid / base split plane (board coordinates)
tilt = 15;            // display tilt towards the user
front_h = 10;         // base height at the front edge
floor_t = 3.0;

/* [PCB pins and snap hooks] */
boss_d = 5.5;
standoff_d = 7.0;
pin_d = 2.7;          // PCB mounting holes are 3.0 mm
pin_len = 4.0;        // reaches into the lid bosses
pin_socket_d = 3.2;
hooks = [[-1, 20], [-1, 38], [1, 20], [1, 60]];  // side (-1 left, +1 right), board Y
hook_w = 8;
hook_t = 1.8;
hook_len = 8;         // arm length below the split plane
hook_barb = 1.0;
foot_d = 10.5;        // self-adhesive rubber feet
foot_depth = 1.0;

/* [Iron holder, slides in from the left or right] */
holder = true;              // adds the dovetail channel to the base
// Weller RT tip with a 3.5 mm cable coupler on its plug. One saddle sits
// under the black sleeve, the other under the coupler; cone and tip point
// backwards into free air behind the station. The holder is symmetric about
// the channel, so the same part works on both sides.
grip_d = 15.0;              // black sleeve of the RT tip (13.5-16 mm, tapered)
holder_offset = 26;         // case wall to iron axis
holder_axis_z = 42;         // iron axis height above the table
saddle_spacing = 44;        // coupler center to sleeve center
holder_plate_t = 4;
holder_post_t = 8;
dove_y = 60;                // channel center (world Y)
dove_w = [16, 22];          // width at the bottom face / at the channel ceiling
dove_h = 4;
dove_clr = 0.25;
tongue_len = 46;            // how far the tongue reaches under the base
holder_saddles = [dove_y - saddle_spacing / 2, dove_y + saddle_spacing / 2];

/* [Button caps] */
cap_d = 7.0;
cap_hole_clr = 0.3;
cap_flange_d = 8.8;  // keeps clear of the display header pins
cap_protrude = 1.6;
cap_gap = 0.5;        // play between flange and lid underside
plunger_d = 3.5;
socket_depth = 1.5;
cap_flat = 0.9;       // D-flat on symbol caps, keeps them from rotating

$fn = 64;
eps = 0.01;

// Derived inner/outer extents (board coordinates)
in_x0 = -left_clr;
in_x1 = pcb_w + clr;
in_y0 = -clr;
in_y1 = pcb_d + clr;
out_x0 = in_x0 - wall;
out_x1 = in_x1 + wall;
out_y0 = in_y0 - wall;
out_y1 = in_y1 + wall;
lid_top = inner_h + top_t;

// Board frame -> world frame: rotate about X, then drop onto the table.
s = sin(tilt);
c = cos(tilt);
ground = out_y0 * s + joint_z * c - front_h;

module to_world() {
	translate([0, 0, -ground]) rotate([tilt, 0, 0]) children();
}

// ---------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------
module rrect(x0, y0, x1, y1, r) {
	translate([x0 + r, y0 + r]) offset(r = r) square([x1 - x0 - 2 * r, y1 - y0 - 2 * r]);
}

// Cap cross-section: round, or D-shaped (flat towards +X) for keyed caps.
module cap_profile(d, keyed) {
	if (keyed) intersection() {
		circle(d = d);
		translate([-d, -d]) square([d + d / 2 - cap_flat, 2 * d]);
	} else circle(d = d);
}

module rounded_block(x0, y0, x1, y1, z0, z1, r, er) {
	hull() {
		translate([0, 0, z0]) linear_extrude(z1 - z0 - er) rrect(x0, y0, x1, y1, r);
		for (p = [[x0 + r, y0 + r], [x1 - r, y0 + r], [x0 + r, y1 - r], [x1 - r, y1 - r]])
			translate([p[0], p[1], z1 - er])
				rotate_extrude() translate([r - er, 0]) circle(er);
	}
}

// ---------------------------------------------------------------------------
// Lid
// ---------------------------------------------------------------------------
module display_window() {
	w = display_active[0] + 1.0;
	h = display_active[1] + 1.0;
	hull() {
		translate([display_center[0], display_center[1], inner_h - eps])
			linear_extrude(eps) offset(r = 0.8) square([w - 1.6, h - 1.6], center = true);
		translate([display_center[0], display_center[1], lid_top])
			linear_extrude(eps * 2) offset(r = 0.8 + top_t) square([w - 1.6, h - 1.6], center = true);
	}
}

module jack_notch(j) {
	translate([out_x0 - 1, j[0] - j[1] / 2, joint_z - 1])
		cube([wall + 2, j[1], j[2] - joint_z + 1 - j[1] / 2]);
	translate([out_x0 - 1, j[0], j[2] - j[1] / 2])
		rotate([0, 90, 0]) cylinder(d = j[1], h = wall + 2);
}

// Snap hooks. Local frame: X = 0 is the inner wall face, +X points into the
// cavity, Z is board Z. place_hook() mirrors the right-hand side.
hook_catch_z = joint_z - hook_len + 1.6;

module place_hook(k) {
	if (k[0] < 0) translate([in_x0, k[1], 0]) children();
	else translate([in_x1, k[1], 0]) mirror([1, 0, 0]) children();
}

module hook_arm() {
	zb = joint_z - hook_len;
	// Root bonded to the lid wall, stops 0.2 mm below the PCB
	translate([-eps, -hook_w / 2, joint_z]) cube([hook_t + 1.2, hook_w, -pcb_t - 0.2 - joint_z]);
	// Flexible arm, 0.2 mm clear of the base wall
	translate([0.2, -hook_w / 2, zb]) cube([hook_t, hook_w, hook_len + 1]);
	// Barb: ramp on the way in, flat catch face on top
	rotate([90, 0, 0]) linear_extrude(hook_w, center = true)
		polygon([[0.2 + hook_t, zb], [0.2, zb], [0.2 - hook_barb, hook_catch_z], [0.2 + hook_t, hook_catch_z]]);
}

module hook_window() {
	translate([-wall - 1, -(hook_w + 1) / 2, hook_catch_z - 2.8]) cube([wall + 1 + eps, hook_w + 1, 2.8]);
}

// The rear right boss would block the USB plug: with USB access enabled it
// moves 3 mm towards the back wall and presses the PCB next to the hole.
function usb_moved(i) = usb && i == 3;
function boss_pos(i) = usb_moved(i) ? [pcb_holes[i][0], pcb_holes[i][1] + 3, 0] : [pcb_holes[i][0], pcb_holes[i][1], 0];

module usb_channel() {
	translate([usb_pos[0] - 0.5, usb_pos[1], usb_z]) rotate([0, 90, 0])
		linear_extrude(out_x1 - usb_pos[0] + 2)
			offset(r = 1.5) square([usb_plug[1] - 3, usb_plug[0] - 3], center = true);
}

module lid() {
	difference() {
		union() {
			difference() {
				rounded_block(out_x0, out_y0, out_x1, out_y1, joint_z, lid_top, corner_r, edge_r);
				translate([0, 0, joint_z - 1])
					linear_extrude(inner_h - joint_z + 1)
						rrect(in_x0, in_y0, in_x1, in_y1, corner_r - wall);
			}
			for (i = [0 : 3]) translate(boss_pos(i)) cylinder(d = boss_d, h = inner_h + eps);
			for (k = hooks) place_hook(k) hook_arm();
		}
		for (i = [0 : 3]) if (!usb_moved(i))
			translate([pcb_holes[i][0], pcb_holes[i][1], -1]) cylinder(d = pin_socket_d, h = pin_len + 1.5);
		if (usb) usb_channel();
		display_window();
		for (b = buttons)
			translate([b[0], b[1], inner_h - 1]) {
				linear_extrude(top_t + 2) offset(r = cap_hole_clr) cap_profile(cap_d, b[2] > 0);
				// Chamfered rim on the top face
				hull() {
					translate([0, 0, top_t + 1 - 0.6]) linear_extrude(eps) offset(r = cap_hole_clr) cap_profile(cap_d, b[2] > 0);
					translate([0, 0, top_t + 1]) linear_extrude(eps * 2) offset(r = cap_hole_clr + 0.8) cap_profile(cap_d, b[2] > 0);
				}
			}
		// LED: small light hole with a wide counterbore underneath
		translate([led_pos[0], led_pos[1], inner_h - 1]) cylinder(d = 3.0, h = top_t + 2);
		translate([led_pos[0], led_pos[1], inner_h - 1]) cylinder(d = 5.6, h = 1 + top_t - 0.8);
		jack_notch(dc_jack);
		jack_notch(tip_jack);
	}
}

// ---------------------------------------------------------------------------
// Base
// ---------------------------------------------------------------------------
module ground_slab(z0 = 0) {
	translate([-200, -200, z0]) cube([400, 400, 400]);
}

module base_outer() {
	intersection() {
		to_world() translate([0, 0, -200]) linear_extrude(200 + joint_z) rrect(out_x0, out_y0, out_x1, out_y1, corner_r);
		ground_slab();
	}
}

module base_cavity() {
	intersection() {
		to_world() translate([0, 0, -200]) linear_extrude(200 + joint_z + 1) rrect(in_x0, in_y0, in_x1, in_y1, corner_r - wall);
		ground_slab(floor_t);
	}
}

module standoffs() {
	intersection() {
		to_world() for (h = pcb_holes) translate([h[0], h[1], -200]) cylinder(d = standoff_d, h = 200 - pcb_t);
		ground_slab();
	}
	// Locating pins through the PCB holes into the lid bosses
	to_world() for (i = [0 : 3]) translate([pcb_holes[i][0], pcb_holes[i][1], -pcb_t - eps]) {
		// The pin under the moved boss ends flush with the PCB top
		len = usb_moved(i) ? -0.2 : pin_len;
		cylinder(d = pin_d, h = pcb_t + len - 0.6);
		translate([0, 0, pcb_t + len - 0.6]) cylinder(d1 = pin_d, d2 = pin_d - 1, h = 0.6);
	}
}

// Dovetail channel across the whole base, open on both sides.
module dovetail_profile(w0, w1, h) {
	polygon([[-w0 / 2, -1], [w0 / 2, -1], [w0 / 2, 0], [w1 / 2, h], [-w1 / 2, h], [-w0 / 2, 0]]);
}

module dove_channel() {
	translate([out_x0 - 1, dove_y, 0]) rotate([90, 0, 90])
		linear_extrude(out_x1 - out_x0 + 2) dovetail_profile(dove_w[0], dove_w[1], dove_h);
	// Detent dimples for a holder inserted from either side
	for (x = [out_x0 + tongue_len - 4, out_x1 - tongue_len + 4])
		translate([x, dove_y, dove_h]) sphere(r = 1.6, $fn = 24);
}

// World Y of a board-frame Y on the table surface.
function ground_y(y) = (y - ground * s) / c;

module base() {
	difference() {
		union() {
			difference() {
				base_outer();
				base_cavity();
			}
			standoffs();
			// Thicker floor above the dovetail channel
			if (holder) intersection() {
				base_outer();
				translate([-100, dove_y - 16, 0]) cube([300, 32, dove_h + 4]);
			}
		}
		if (holder) dove_channel();
		// Snap hook windows
		to_world() for (k = hooks) place_hook(k) hook_window();
		// Rubber feet
		for (x = [20, pcb_w - 20], y = [ground_y(out_y0 + 12), ground_y(out_y1 - 12)])
			translate([x, y, -1]) cylinder(d = foot_d, h = foot_depth + 1);
		// Vent slots in the back wall
		to_world() for (i = [0 : 6])
			translate([12 + i * 8, in_y1 - 1, joint_z - 22]) cube([3.2, wall + 2, 16]);
	}
}

// ---------------------------------------------------------------------------
// Iron holder: a dovetail tongue slides into the channel under the base,
// the grip rests in two saddles and the hot tip points backwards into free
// air behind the station. Symmetric about the channel: turn it by 180
// degrees to use it on the left side.
// ---------------------------------------------------------------------------
module saddle(y, d) {
	cx = out_x1 + holder_offset;
	w = grip_d + 16;
	top = holder_axis_z + d / 2 + 3;
	difference() {
		union() {
			translate([cx - w / 2, y - holder_post_t / 2, 0]) cube([w, holder_post_t, top]);
			hull() {
				translate([out_x1 + 0.5, y - holder_post_t / 2, 0]) cube([eps, holder_post_t, holder_plate_t + 6]);
				translate([cx - w / 2, y - holder_post_t / 2, 0]) cube([eps, holder_post_t, top * 0.6]);
			}
		}
		// U-shaped seat with a flared entry
		hull() {
			translate([cx, y, holder_axis_z]) rotate([90, 0, 0]) cylinder(d = d + 1, h = holder_post_t + 2, center = true);
			translate([cx - (d + 9) / 2, y - holder_post_t / 2 - 1, top + 1]) cube([d + 9, holder_post_t + 2, eps]);
		}
	}
}

module tongue() {
	xt = out_x1 - tongue_len;  // tip, under the base
	th = dove_h - dove_clr;
	difference() {
		translate([xt, dove_y, 0]) rotate([90, 0, 90])
			linear_extrude(tongue_len + 1)
				intersection() {
					offset(delta = -dove_clr) dovetail_profile(dove_w[0], dove_w[1], dove_h);
					translate([-50, 0]) square([100, th]);
				}
		// Slot around the spring finger
		translate([xt + 1, dove_y - 4, -1]) cube([18, 8, dove_h + 2]);
	}
	// Spring finger with a detent bump near its free end
	translate([xt + 2, dove_y - 3, th - 1.6]) cube([17 + eps, 6, 1.6]);
	intersection() {
		translate([xt + 4, dove_y, th]) sphere(r = 1.4, $fn = 24);
		translate([xt + 2, dove_y - 3, th]) cube([4, 6, 1]);
	}
}

module holder_part() {
	y0 = holder_saddles[0] - 10;
	y1 = holder_saddles[1] + 10;
	x1 = out_x1 + holder_offset + grip_d / 2 + 12;
	difference() {
		union() {
			linear_extrude(holder_plate_t) rrect(out_x1 + 0.5, y0, x1, y1, 4);
			tongue();
			for (y = holder_saddles) saddle(y, grip_d);
		}
		// Rubber feet, same as under the base
		for (y = [y0 + 12, y1 - 12])
			translate([(out_x1 + x1) / 2 + 6, y, -1]) cylinder(d = foot_d, h = foot_depth + 1);
	}
}

// Weller RT tip with cable coupler, for the assembly views only.
// Lengths measured from a product photo, scaled by the 3.5 mm plug.
module iron_dummy() {
	chrome = "#d4d4d8";
	nickel = "#a1a1aa";
	rubber = "#18181b";
	sleeve_front = holder_saddles[1] + 19.5;
	sleeve_rear = holder_saddles[1] - 19.5;
	translate([out_x1 + holder_offset, 0, holder_axis_z]) rotate([-90, 0, 0]) {
		// Cable and coupler
		color("#27272a") translate([0, 0, sleeve_rear - 11.6 - 25 - 60]) cylinder(d = 4, h = 62);
		color("#3f3f46") translate([0, 0, sleeve_rear - 11.6 - 25]) cylinder(d = 10, h = 25);
		// Ferrule
		color(nickel) translate([0, 0, sleeve_rear - 11.6]) cylinder(d = 9.7, h = 11.6);
		// Ribbed grip sleeve, tapered towards the plug
		color(rubber) translate([0, 0, sleeve_rear]) cylinder(d1 = 13.6, d2 = 15.9, h = 39);
		color("#27272a") for (z = [3 : 3 : 36])
			translate([0, 0, sleeve_rear + z]) cylinder(d = 13.9 + z * 0.059, h = 0.6);
		// Cone, collar, shaft and working tip
		color(chrome) translate([0, 0, sleeve_front]) cylinder(d1 = 14.5, d2 = 2.6, h = 12);
		color(chrome) translate([0, 0, sleeve_front + 12]) cylinder(d = 2.4, h = 3);
		color(chrome) translate([0, 0, sleeve_front + 15]) cylinder(d = 1.2, h = 12.4);
		color(chrome) translate([0, 0, sleeve_front + 27.4]) cylinder(d = 2.5, h = 15.6);
		color(chrome) translate([0, 0, sleeve_front + 43]) cylinder(d1 = 2.5, d2 = 0.3, h = 6.6);
	}
}

// ---------------------------------------------------------------------------
// Button caps (printed upright, flange on the bed)
// ---------------------------------------------------------------------------
cap_bottom = button_top_z - socket_depth;
cap_len = lid_top + cap_protrude - cap_bottom;
flange_h = inner_h - cap_gap - cap_bottom;

module symbol(kind) {
	if (kind == 1) {
		difference() {
			circle(d = 4.4);
			circle(d = 3.0);
			translate([-1.1, 0]) square([2.2, 3]);
		}
		translate([-0.35, 0]) square([0.7, 2.6]);
	} else if (kind == 2) {
		polygon([[-2, -1.2], [2, -1.2], [0, 1.8]]);
	} else if (kind == 3) {
		polygon([[-2, 1.2], [2, 1.2], [0, -1.8]]);
	}
}

module cap(kind) {
	difference() {
		union() {
			cylinder(d = cap_flange_d, h = flange_h);
			linear_extrude(cap_len - 0.6) cap_profile(cap_d, kind > 0);
			hull() {
				translate([0, 0, cap_len - 0.6]) linear_extrude(eps) cap_profile(cap_d, kind > 0);
				translate([0, 0, cap_len - eps]) linear_extrude(eps) offset(r = -0.6) cap_profile(cap_d, kind > 0);
			}
		}
		translate([0, 0, -eps]) cylinder(d = plunger_d + 0.3, h = socket_depth + eps);
		if (kind > 0)
			translate([0, 0, cap_len - 0.6]) linear_extrude(1) scale(0.85) symbol(kind);  // stays clear of the D-flat
	}
}

module caps() {
	for (i = [0 : len(buttons) - 1])
		translate([(i % 3) * 12, floor(i / 3) * 12, 0]) cap(buttons[i][2]);
}

// ---------------------------------------------------------------------------
// Fit check: a 1.2 mm slice of the lid top to verify display and buttons
// before printing the full lid.
// ---------------------------------------------------------------------------
module fitcheck() {
	intersection() {
		translate([0, 0, -lid_top + 1.2]) lid();
		translate([-50, -50, 0]) cube([200, 200, 1.2]);
	}
}

// ---------------------------------------------------------------------------
// Board dummy for the assembly view
// ---------------------------------------------------------------------------
module board_dummy() {
	color("#c62828") translate([0, 0, -pcb_t]) difference() {
		cube([pcb_w, pcb_d, pcb_t]);
		for (h = pcb_holes) translate([h[0], h[1], -1]) cylinder(d = 3.2, h = 4);
	}
	color("#1565c0") translate([6.92, 14.9, 8.5]) cube([53.34, 45, 1.6]);
	color("#0b1220") translate([display_center[0] - 23.3, display_center[1] - 17.3, 10.1]) cube([46.6, 34.6, display_top_z - 10.1]);
	color("#1565c0") translate([15.85, 60.41, 8]) cube([44.45, 17.78, 1.6]);
	for (b = buttons) color("#222") translate([b[0] - 3, b[1] - 3, 0]) cube([6, 6, 3.5]);
	color("#333") translate([-2.9, 63.85, 0]) cube([13.6, 9, 11]);
	color("#333") translate([-3.6, 46.8, 0]) cube([17.9, 6, 6]);
}

module assembly(explode = 0) {
	color("#e2e8f0") base();
	if (holder) {
		translate([explode * 1.2, 0, 0]) color("#cbd5e1") holder_part();
		translate([explode * 1.2, 0, explode * 0.4]) iron_dummy();
	}
	to_world() {
		translate([0, 0, explode * 0.5]) board_dummy();
		translate([0, 0, explode * 1.4]) {
			color("#334155") lid();
			for (b = buttons)
				color("#94a3b8") translate([b[0], b[1], cap_bottom + explode * 0.4]) cap(b[2]);
		}
	}
}

// All printed parts in print orientation (preview image)
module plate() {
	color("#e2e8f0") translate([0, 0, 0]) base();
	color("#334155") translate([100, 0, lid_top]) rotate([180, 0, 0]) translate([0, -out_y1 - out_y0, 0]) lid();
	color("#cbd5e1") translate([200 - out_x1 + tongue_len, -10, 0]) holder_part();
	color("#94a3b8") translate([110, -40, 0]) caps();
}

// ---------------------------------------------------------------------------
if (part == "assembly") assembly();
else if (part == "exploded") assembly(40);
else if (part == "lid") translate([0, 0, lid_top]) rotate([180, 0, 0]) lid();  // print top down
else if (part == "base") base();
else if (part == "caps") caps();
else if (part == "fitcheck") fitcheck();
else if (part == "plate") plate();
else if (part == "bottom") {  // preview: holder half pulled out to show the channel
	color("#e2e8f0") base();
	to_world() color("#334155") lid();
	translate([28, 0, 0]) { color("#cbd5e1") holder_part(); iron_dummy(); }
}
else if (part == "holder") translate([-out_x1 + tongue_len, 0, 0]) holder_part();
