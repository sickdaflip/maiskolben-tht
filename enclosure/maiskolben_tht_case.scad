// SPDX-License-Identifier: GPL-2.0-or-later
// Maiskolben THT (board rev. 1.3c) - 3D printable desk enclosure
//
// Two shells plus six button caps:
//   * lid  - frames the display, carries the button caps, screwed to the PCB
//   * base - tilted wedge (default 15 deg) with standoffs and ballast space
// Four M3x16 screws go from below through the base standoffs and the PCB
// mounting holes into pilot holes in the lid bosses.
//
// Render a single part: openscad -D 'part="lid"' -o lid.stl maiskolben_tht_case.scad
// Parts: "assembly", "exploded", "lid", "base", "caps", "fitcheck"

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
button_top_z = 12.0;            // tactile plunger top (12 mm switches)
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

/* [Fasteners] */
boss_d = 5.5;
pilot_d = 2.6;        // M3 self-tapping into PLA/PETG
pilot_depth = 10;
standoff_d = 7.0;
screw_d = 3.4;
head_d = 6.4;
seat_below_pcb = 6.4; // screw head seat, gives M3x16
foot_d = 10.5;        // self-adhesive rubber feet
foot_depth = 1.0;

/* [Button caps] */
cap_d = 7.0;
cap_hole_clr = 0.3;
cap_flange_d = 8.8;  // keeps clear of the display header pins
cap_protrude = 1.6;
cap_gap = 0.5;        // play between flange and lid underside
plunger_d = 3.5;
socket_depth = 1.5;

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

module lid() {
	difference() {
		union() {
			difference() {
				rounded_block(out_x0, out_y0, out_x1, out_y1, joint_z, lid_top, corner_r, edge_r);
				translate([0, 0, joint_z - 1])
					linear_extrude(inner_h - joint_z + 1)
						rrect(in_x0, in_y0, in_x1, in_y1, corner_r - wall);
			}
			for (h = pcb_holes) translate([h[0], h[1], 0]) cylinder(d = boss_d, h = inner_h + eps);
		}
		for (h = pcb_holes) translate([h[0], h[1], -1]) cylinder(d = pilot_d, h = pilot_depth + 1);
		display_window();
		for (b = buttons)
			translate([b[0], b[1], inner_h - 1]) {
				cylinder(d = cap_d + 2 * cap_hole_clr, h = top_t + 2);
				translate([0, 0, top_t + 1 - 0.6]) cylinder(d1 = cap_d + 2 * cap_hole_clr, d2 = cap_d + 2 * cap_hole_clr + 1.6, h = 0.6 + eps);
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
		}
		to_world() for (h = pcb_holes) {
			translate([h[0], h[1], -200]) cylinder(d = screw_d, h = 200);
			translate([h[0], h[1], -200]) cylinder(d = head_d, h = 200 - pcb_t - seat_below_pcb);
		}
		// Rubber feet
		for (x = [20, pcb_w - 20], y = [ground_y(out_y0 + 12), ground_y(out_y1 - 12)])
			translate([x, y, -1]) cylinder(d = foot_d, h = foot_depth + 1);
		// Vent slots in the back wall
		to_world() for (i = [0 : 6])
			translate([12 + i * 8, in_y1 - 1, joint_z - 22]) cube([3.2, wall + 2, 16]);
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
			cylinder(d = cap_d, h = cap_len - 0.6);
			translate([0, 0, cap_len - 0.6]) cylinder(d1 = cap_d, d2 = cap_d - 1.2, h = 0.6);
		}
		translate([0, 0, -eps]) cylinder(d = plunger_d + 0.3, h = socket_depth + eps);
		if (kind > 0)
			translate([0, 0, cap_len - 0.6]) linear_extrude(1) symbol(kind);
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
	color("#1565c0") translate([15.85, 60.41, 6]) cube([44.45, 17.78, 1.6]);
	for (b = buttons) color("#222") translate([b[0] - 3, b[1] - 3, 0]) cube([6, 6, 3.5]);
	color("#333") translate([-2.9, 63.85, 0]) cube([13.6, 9, 11]);
	color("#333") translate([-3.6, 46.8, 0]) cube([17.9, 6, 6]);
}

module assembly(explode = 0) {
	color("#e2e8f0") base();
	to_world() {
		translate([0, 0, explode * 0.5]) board_dummy();
		translate([0, 0, explode * 1.4]) {
			color("#334155") lid();
			for (b = buttons)
				color("#94a3b8") translate([b[0], b[1], cap_bottom + explode * 0.4]) cap(b[2]);
		}
	}
}

// ---------------------------------------------------------------------------
if (part == "assembly") assembly();
else if (part == "exploded") assembly(40);
else if (part == "lid") translate([0, 0, lid_top]) rotate([180, 0, 0]) lid();  // print top down
else if (part == "base") base();
else if (part == "caps") caps();
else if (part == "fitcheck") fitcheck();
