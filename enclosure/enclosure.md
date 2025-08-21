# Printalyzer UV/VIS Densitometer Enclosure

## Introduction

This directory contains all the assets necessary to produce the physical
enclosures for the Printalyzer Densitometer. This document explains what
component each file represents, and also provides a bill-of-materials
for additional hardware needed for assembly.

The actual assembly instructions are located elsewhere.

## 3D Models

Models designed for 3D printing are provided as both STL and 3MF files,
and are listed without their file extension.

Original CAD files are provided in the native format of the CAD application.

The models that make up the construction of the device must be printed
in a material that is minimally reflective and opaque to both visible and
infrared light.

_**Note: Many of the models for components beyond the main enclosure were
created by deriving from either the main assembly CAD project or related
projects that might include non-distributable models of 3rd party
components. As such, original CAD files may not be provided for everything
listed here.**_

These files are in the `models_3d` subdirectory.

### CAD Projects

* `dens-enclosure.f3z` - Main CAD model for the enclosure, designed in Fusion 360.  
  (_Note: Does not include models of the PCBs or mounting hardware, to avoid
  accidentally redistributing component models that are under restrictive
  license terms._)

### Main Enclosure Components

* `dens-main-housing` - Main housing for the circuit board and major components
* `dens-main-cover` - Top cover for the main housing
* `dens-main-cover-dev` - Alternative top cover with a clearance hole for a debug connector
* `dens-base` - Base housing that contains the transmission LED components
* `dens-base-cover` - Bottom cover for the base housing

### Internal Components

* `filter-stack-holder` - Tray that sits on top of the lens assembly and holds the sensor diffuser
* `filter-stack-lower-ring` - Lower ring that acts as a spacer, aperture, and helps to hold the sensor
  diffuser in place.
* `filter-stack-upper-ring` - Upper ring that sits between the filter stack and the main circuit
  board to act as a light seal. Should be made from an opaque flexible material, such as rubber or foam.
* `sensor-board-cover` - Internal cover for the backside of the circuit board above the sensor elements

### Assembly Jigs

* `jig-dens-main-assembly` - Jig to hold the main housing during assembly
* `jig-dens-base-assembly` - Jig to hold the base, upside down, during assembly
* `jig-display-mounting` - Jig to help align the PCB and OLED display for assembly
* `jig-overlay-mounting` - Jig to help align the main cover and graphic overlay for assembly
* `jig-spring-trimmer-base` - Base portion of a jig to help hold the torsion springs
  in place while trimming them with a rotary tool
* `jig-spring-trimmer-cover` - Cover portion of a jig to help hold the torsion springs
  in place while trimming them with a rotary tool
* `jig-upside-down` - Jig to hold the complete device in an upside-down orientation,
  which is useful for mating the top and bottom halves and for installing the base cover
* `jig-cal-strip-measure` - Jig for holding a calibration reference for measurement,
  when the label markings have not yet been applied

### Calibration Jigs

* `jig-cal-t2120-center` - Jig to repeatably position a Stouffer T2120 step wedge for linearity calibration (center position)
* `jig-cal-t2120-side` - Jig to repeatably position a Stouffer T2120 step wedge for linearity calibration (side position)
* `jig-cal-t5100-full` - Jig to repeatably position s Stouffer T5100 step wedge for measurement or target calibration (measurement spot at the patch center)
* `jig-cal-t5100-labeled` - Jig to repeatably position a Stouffer T5100 step wedge for measurement or target calibration (measurement spot shifted to accommodate a 4x1" label)

## 2D Models

Models designed for laser cutting are provided as DXF files.
They are provided in both single-part and multi-part grid versions.

These files are in the `models_2d` subdirectory.

* `stage-plate-ring.dxf` - The stage plate outer ring. Cut from a white acrylic
  material that is 1.5mm thick.
* `filter-stack-upper-ring.dxf` - Alternative version of the filter stack upper ring model
  from above, to be used if laser cutting from a stock material. Must use a clean, opaque,
  and flexible material that is 1.5mm thick.
* `sensor-board-cover.dxf` - Alternative version of the sensor board cover
  from above. Must be cut from a material that is opaque to visible and
  infrared light and that is approximately 1mm thick. If thicker, then the
  mounting screws in the sensor head area will need to be longer.

## Graphic Overlay

The graphic overlay is a piece of custom cut polycarbonate material
that has printing and a transparent window for the display.
For all device builds so far, it has been ordered from [JRPanel](https://www.jrpanel.com/).
These files are the design artifacts that have been used when placing
that order.

These files are in the `overlay` subdirectory.

* `graphic-overlay-dims.pdf` - CAD drawing showing all the relevant
  dimensions of the overlay
* `graphic-overlay.dxf` - 2D CAD file representing the relevant
  dimensions of the overlay
* `graphic-overlay.svg` - Inkscape design for the overlay, annotated as appropriate.
* `graphic-overlay.pdf` - PDF export of the design, for ease of exchange.

## Product Labels

* 2x1" label, Zebra Z-Xtreme 4000T High-Tack Silver, SKU: 10023174

## Additional Components

### Non-PCB Electronic Components

These are components that are not included in the PCB's bill-of-materials,
but are ordered from the same kinds of vendors and are necessary to assemble
a complete device.

* Button caps (x4)
  * **E-Switch 1SWHT**
  * Mouser PN: 612-1S-WHT
* 1.3" OLED Display Screen
  * **ER-OLED013A1-1W**
  * https://www.buydisplay.com/white-1-3-inch-oled-i2c-arduino-ssd1306-display-module-connector-fpc
* Cable to connect Transmission LED Board to Main Board
  * **JST JUMPER 04SR-3S - 04SR-3S 4"**
  * Manufacturer PN: A04SR04SR30K102A

### Non-Electronic Components

These components are not necessarily sourced from electronics vendors, but
are still necessary for the assembly of the device.

* Sensor head lens
  * Aspherical focusing acrylic plano-convex lens, 5mm diameter
  * Sourced from AliExpress
  * https://www.aliexpress.us/item/3256802937733209.html
* Sensor head aperture
  * M2 Hard Fiber Washer, 2.3mm inside diameter, 5mm outside diameter
  * McMaster-Carr PN: 95225A210
* Sensor head diffuser
  * UV Fused Silica diffuser, 10mm diameter, 1mm thick
  * Sourced from UQG Optics, part number **DUV-1010**
* Transmission light diffuser
  * White flashed opal, 6mm diameter, 2.5mm thick
  * Sourced from UQG Optics as a custom order
* Transmission light diffuser gasket
  * Square-Profile Oil-Resistant Buna-N O-Ring
  * 1.5mm wide, 6mm inside diameter
  * McMaster-Carr PN: 1171N142

## Assembly Hardware

* M2 x 4mm thread forming screws (x2)
  * McMaster-Carr PN: 90380A324
* M2 x 5mm thread forming screws (x2)
  * McMaster-Carr PN: 90380A325
* M2 x 6mm thread forming screws (x4)
  * McMaster-Carr PN: 90380A326
* M2 x 8mm thread forming screws (x4)
  * McMaster-Carr PN: 90380A328
* M2 x 8mm flat head black screws (x4)
  * McMaster-Carr PN: 91698A204
* M3 x 20mm low-profile socket head partially threaded screws (x2)
  * McMaster-Carr PN: 93070A076
* Torsion Spring, 180 deg Right-Hand wound
  * McMaster-Carr PN: 9271K97
  * Gardner Spring PN: GT2814018-MR
* Torsion Spring, 180 deg Left-Hand wound
  * McMaster-Carr PN: 9271K98
  * Gardner Spring PN: GT2814018-ML
* Flanged sleeve bearings for 3mm shaft, 5mm long (x2)
  * McMaster-Carr PN: 2705T112
  * Igus PN: JFM-0304-05
* Rubber bumbers (x4)
  * **3M BUMPER CYLIN 0.315" DIA BLACK**
  * Manufacturer PN: SJ5076
  * Digi-Key PN: 3M156065-ND
* Adhesive wheel weights, 1/4 oz (x6)
  * Available from a variety of vendors
* All-purpose adhesive
  * E6600 QuickHOLD
* UV-Curing adhesives
  * Norland Products NOA 68
  * Norland Products NOA 72
* Foam mounting tape (3/4"x 15ft)
  * **3M VHB 3/4-5-4991**
  * McMaster-Carr PN: 8127A88
