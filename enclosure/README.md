# Maiskolben THT – desk enclosure

![Enclosure preview](preview.png)

Parametric, screwless OpenSCAD design for the Maiskolben THT board
(rev. 1.3c), dimensions taken from the original Eagle layout.

* **Base** – 15° wedge so the display faces the user, vent slots at the back,
  hollow floor for ballast (a steel plate or washers keep the station from
  sliding when the cable pulls). Standoffs with locating pins hold the PCB.
* **Lid** – frames the display with a 45° bezel, guides the button caps and
  snaps onto the base with four hooks. Its bosses clamp the PCB onto the pins.
* **Caps** – T1–T3 plain, power, ▲ and ▼ with debossed symbols. The symbol
  caps have a D-flat that matches the lid holes, so they cannot rotate.
* **Tip holder** – slides into a dovetail channel under the base from the
  left or the right and locks with a spring detent. It carries a Weller RT
  tip by its black grip sleeve and the cable coupler; cone and tip point
  backwards into free air behind the station, so no hot part touches plastic.

Outside dimensions: 82 × 93 mm footprint, about 55 mm high at the back.

## Parts

| File | Orientation | Notes |
| --- | --- | --- |
| `stl/maiskolben_tht_lid.stl` | top face down | no supports |
| `stl/maiskolben_tht_base.stl` | as exported | no supports |
| `stl/maiskolben_tht_caps.stl` | flange down | 6 caps |
| `stl/maiskolben_tht_holder_right.stl` | as exported | holder for the right side |
| `stl/maiskolben_tht_holder_left.stl` | as exported | mirrored, for the left side |
| `stl/maiskolben_tht_fitcheck.stl` | flat | 1.2 mm slice of the lid top |

PETG or ASA recommended, 0.2 mm layers, 3 perimeters, 20 % infill.
No screws needed; optional: 6 × self-adhesive rubber feet up to 10 mm
(4 under the base, 2 under the holder so both stand at the same height).

Preview images for model sites are in [`images/`](images).

## Before printing the lid

Print the **fit check** first (10 minutes): lay it on the assembled board and
verify that the display window and the six button holes line up. The values
to adjust are at the top of the `.scad` file:

* `display_center` – active area center in board coordinates
* `inner_h` – free height under the lid (default 15 mm, enough for a Nano on
  female headers including the USB socket)
* `button_top_z` – tactile plunger height (13 mm switches, measured)
* `dc_jack`, `tip_jack` – notch position, width and height for the jacks
* `grip_d` – grip sleeve of the RT tip; the coupler saddle uses the same
  opening as the power jack notch (`coupler_d`), the flared saddles also take
  slightly thinner parts

The design assumes the LM7805 is not fitted (12 V supply, SJ1 bridged).

## Assembly

1. Insert the caps into the lid from below.
2. Put the board onto the base so the four pins go through its mounting holes.
3. Press the lid down until all four hooks click into the side windows.
4. Slide the holder into the channel under the base from either side until
   the detent clicks.

To open the case, push the hooks inwards through the side windows with a
small screwdriver. USB is only reachable with the lid removed.

## Export

```sh
openscad -D 'part="lid"' -o lid.stl maiskolben_tht_case.scad
```

Parts: `assembly`, `exploded`, `bottom`, `plate`, `lid`, `base`, `caps`,
`fitcheck`, `holder_right`, `holder_left`.
