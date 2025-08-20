# Printalyzer UV/VIS Densitometer Assembly

This document describes the process of assembling a complete
Printalyzer UV/VIS Densitometer from its major components. It assumes that the
assembly of printed circuit boards, along with any soldered-in components,
has been performed prior to following this document. The steps laid out
herein are written with the assumption that they are performed in sequential
order. In practice, independent steps may be reordered as appropriate for
batch assembly.

The majority of the bill-of-materials for this process is contained within
the [enclosure](../enclosure/enclosure.md) document. Please refer there for
detailed descriptions and part numbers for any components listed here.


## Main Board Assembly

### Components

* Main Board (assembled PCB)
* Button caps (x4)
* OLED Display Screen
* Foam mounting tape

### Tools

* Display mounting jig (`jig-display-mounting`)
* Knife or scissors

### Steps

* Mount the display
  * Cut a 3cm piece of mounting tape, and affix to the main board over the
    rectangular silkscreen markings that represent the center of a display
    component.
  * Insert the display screen, face-down, into the recessed rectangular area
    on the display mounting jig. Do not remove the protective cover.
  * Remove the outer film from the mounting tape, exposing the rest of the
    adhesive.
  * Carefully lower the main board onto the dowel pins sticking up from the
    mounting jig, until the display is affixed to the tape.
  * Remove the main board from the jig, and carefully apply additional
    pressure with your fingers to ensure everything is securely in place.
  * Insert the display flat flex cable into the socket on the opposing side
    of the main board, and lock it in place with the socket tabs.
* Press-fit the button caps onto the tactile switches on the main board.


## Base

### Components

* Base housing (`dens-base`)
* Stage Plate Outer Ring (`stage-plate-rings`)
* Transmission light diffuser (6x2.5mm flashed opal)
* Transmission light diffuser gasket (1.5mm square profile, 6mm ID, o-ring)
* Transmission Board (assembled PCB)
* Adhesive wheel weights (x6)
* Jumper cable assembly
* M2 x 4mm thread forming screws (x2)

### Tools

 * Base assembly jig (`jig-dens-base-assembly`)
 * Diffuser positioning jig
 * Scrap paper
 * Toothpick
 * Screwdriver
 * Knife
 * All-purpose adhesive (E6600)
 * Norland Products NOA 68 UV curing adhesive
 * UV curing light
 * Cyanoacrylate glue

### Steps

* Affix adhesive weights
  * Use a knife on a hard surface to cut the wheel weights into four distinct
    pieces; two singles and two pairs.
  * Remove the backing paper and press the pieces into the underside of the base.
    The singles go in recesses just behind the board mounting area, and the
    pairs go in larger recesses further back.
  * Squirt beads of all-purpose adhesive into all the interior crevices surrounding
    the weights.
  * Allow to dry for 24 hours.
* Install the transmission light diffuser
  * Place the o-ring gasket into the center of the diffuser positioning jig.
  * Place the flashed opal diffuser, white-side-down, into the middle of the o-ring.
    Both pieces should be flush on the bottom, with the diffuser sticking out a bit on the top.
  * Squirt a blob of NOA 68 adhesive onto a piece of scrap paper.
  * Use a toothpick to carefully apply this adhesive around the surface of the o-ring
    and the crevice where the diffuser and o-ring meet, being careful to not get any onto the
    flat surface of the diffuser. (This is most easily done under a stereo inspection microscope.)
  * Place the base, upside-down, on top of the diffuser positioning jig, being careful to hold it
    centered and level with respect to the hole.
  * When you are certain that everything is flat, level, and positioned correctly,
    use the UV curing light to cure the adhesive.
* Install the stage plate outer ring
  * Squirt a bead of cyanoacrylate glue liquid around the stage plate outer
    ring recess in the base housing. Use a minimal amount, preferably in a
    circle around the midpoint.
  * Insert the stage plate outer ring, and press down firmly.
  * Let the piece sit and allow the glue to dry.
* Place the base housing upside-down in the base assembly jig.
* Install transmission board
  * Place the board into its spot, and secure with screws.
  * Attach the cable assembly to the board connector.
  * Loop the cable if necessary, for length.
  * Feed the cable out through the cable hole at the rear of the base.


## Base Cover

### Components

* Base cover (`dens-base-cover`)
* Product label
* Rubber bumbers (x4)
* M2 x 8mm flat head black screws (x4)

### Tools

* Base assembly jig (`jig-dens-base-assembly`)
* Screwdriver

### Steps

* Place the Base housing sub-assembly upside-down in the base assembly jig.
* Place cover over the Base housing sub-assembly, and secure with screws
* Apply rubber bumpers in the indents over the screws
* Affix the product label to its indent (can leave this step to the very end)


## Main Housing

### Components

* Main Housing (`dens-main`)
* Sensor head lens
* Sensor head aperture washer
* Sensor head diffuser
* Diffuser holder (`filter-stack-holder`)
* Diffuser lower ring (`filter-stack-lower-ring`)
* Diffuser upper ring (`filter-stack-upper-ring`)
* Sensor Board Cover (`sensor-board-cover`)
* Main Board Assembly
* M2 x 6mm thread forming screws (x4)
* M2 x 5mm thread forming screws (x2)

### Tools

* Main housing assembly jig (`jig-dens-main-assembly`)
* Screwdriver
* Scissors or rotary trimmer
* Microfiber cloth
* Norland Products NOA 72 UV curing adhesive
* UV curing light

### Steps

* Place the Main Housing into the Main housing assembly jig.
* Install the sensor head lens
  * Insert the sensor head lens into the sensor head aperture hole, and make sure
    it is pressed all the way down.
    * The lens is installed with the more convex side facing down
    * Be careful to use non-marking tools and keep the area clean
  * Insert the sensor head aperture washer on top of the lens
  * Carefully apply the NOA 72 adhesive around the perimeter of the lens and washer hole
    * This adhesive has a very thin consistency, so care must be taken when applying it.
    * Best results involve the use of a very fine tip on a syringe, waiting for the fluid
      to stop dribbling, and then using mostly capillary action for the fluid to seep from
      the tip into the perimeter of the hole on the device.
  * When you are certain that everything is level and centered correctly,
    use the UV curing light to cure the adhesive.
  * To ensure consistent and complete curing, it may be advisable to shine the curing light
    from both sides of the lens.
* Install the sensor diffuser/filter stack
  * Insert the diffuser holder
  * Insert the sensor head diffuser, matte side up, into the holder
  * Insert the diffuser lower ring
  * Insert the diffuser upper ring
* Place the Main Board Assembly into the housing
* Place the Sensor Board Cover on top of the Main Board Assembly.
* Secure with screws the board with screws
  * 6mm long screws go on the side with the sensor head
  * 5mm long screws go on the side with the connectors


## Main Cover

### Components

* Main Cover (`main-cover`)
* Graphic Overlay
* Main Housing Assembly
* M2 x 8mm thread forming screws (x4)

### Tools

* Overlay mounting jig (`jig-overlay-mounting`)
* Screwdriver

### Steps

* If the protective cover over the display window on the graphic overlay is
  encroaching on the button holes, then move it as necessary.
* Place the graphic overlay face-down into the jig
* Carefully remove the backing material from the graphic overlay.
* Align and place the main cover, face down, onto the overlay.
* Remove the main cover, and press on a hard surface to make
  sure the overlay is well adhered.
* Remove the protective cover from the mounted display, which is on
  the main board which is installed within the main housing assembly.
* Place the main cover onto the main housing assembly,
  and secure with screws

_Note: The display window cover on the outer surface of the graphic overlay
should probably be left on through shipping, and removed by the end user._


## Final Assembly

### Components

* Main Body Assembly (Main Body + Main Body Cover)
* Base Assembly (Base + Base Cover)
* RH Torsion Spring
* LH Torsion Spring
* Flange sleeve bearings (x2)
* M3 x 20mm low-profile socket head partially threaded screws (x2)

### Tools

* Spring trimmer jig (`jig-spring-trimmer-base` and `jig-spring-trimmer-cover`)
* Rotary tool with cut-off disc
* Screwdriver

### Steps

* Insert springs into the trimmer jig, and cut ~12mm off the bottom end of
  each one using a rotary tool.
* Take trimmed springs, and insert them into the base assembly hinge area
* Insert the flange sleeve bearings onto the inside-facing spring hole,
  and insert the hinge screws from the outside to hold them in place.
* Connect the transmission LED cable to the Main Body Assembly.
* Carefully install the main body assembly onto the spring tines
  and hinge screw/flange assembly.
* Tighten the hinge screws, alternating, to keep centered alignment.


## Post-Assembly

These steps are just a rough outline of the final part of the process, and
will be further refined once the device is in production.

* Install the bootloader, either pre-loading prior to assembly or via
  USB DFU after assembly.
* Install the firmware, either pre-loading prior to assembly or via
  USB DFU/UF2 after assembly.
* Load the "factory" sensor slope calibration data.
* Run the sensor gain calibration routine.
* Calibrate reflection and transmission modes against references
  to be included with the package.
