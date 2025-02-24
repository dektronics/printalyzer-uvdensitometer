# 3D Component Models

## Introduction

The 3D models used by the components in these schematics come from a variety
of sources. Some were created for this project, using KiCad's library
scripts, FreeCAD, or Fusion 360. Source files for those are included where
practical. Many others were downloaded from component manufacturer websites
or other 3rd party sources.

Some manufacturers have a habit of freely distributing 3D models for their
components with attached notes saying that they shall not be redistributed.
These notes also often mention that the models are proprietary, confidential,
for personal use only, or something else that would scare you off from
actually admitting that you used them. As such, these models have not been
included in the project repository. Instead, links for downloading them
are provided.

Other models are made available under unspecified or unclear terms.
When those are included, the source is mentioned. If anyone takes issue,
they can be removed from the repository.

## Model List

The models are listed by base filename. Typical distribution files have
the ".step" extension, while source CAD models may have other extensions.

To include models marked as not redistributable, download them from the
original source and convert using the KiCad StepUp plugin for FreeCAD.
Make sure the original and converted files have the same base name as
listed below.

Due to the incremental nature of project development, this list may include
models for components that are no longer part of the actual board design.

Models that are explicitly excluded from the repository have been added to
this directory's `.gitignore` file.

### AMS_AS7341
* Status: Included (Same license as project)
* Source: Created for project using Autodesk Fusion 360

### AMS_AS7343
* Status: Included
* Source: Created by using KiCad to extract the component model from the
  sensor evalation board PCB layout files, which are distributed within
  the vendor evaluation board data package:
  [Evaluation Software](https://ams.com/o/download-server/document-download/download/8404661)

### AMS_TCS3472
* Status: Included (Same license as project)
* Source: Created for project using Autodesk Fusion 360

### AMS_TSL2591
* Status: Included (Same license as project)
* Source: Created for project using FreeCAD

### FTSH-107-01-L-DV-K-A
* Status: Included ([Terms](https://www.cadenas.de/terms-of-use-3d-cad-models)
  somewhat unclear, but may be okay for project use.)
* Source: Manufacturer <https://www.samtec.com/partnumber/ftsh-107-01-l-dv-k-a>

### Amphenol 10118194
* Status: Included (Terms unspecified, linked from Digi-Key product page)
* Source: Manufacturer <https://www.amphenol-icc.com/media/wysiwyg/files/3d/s10118194-0001lf.zip>

### Keystone_5006
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/5006/Keystone%20Electronics/view-part/>

### STM_ESDALC6V1-1U2
* Status: Included
* Source: Digi-Key / UltraLibrarian download

### SOD882_STM.step
* Status: Included
* Source: Digi-Key / UltraLibrarian download

### SunLike_LED_S1S0
* Status: Included (Same license as project)
* Source: Created for project using Autodesk Fusion 360

### WL-SWTP-3022
* Status: Included
* Source: Digi-Key product page

### CONN_OLED_128x64
* Status: Included (CERN OHL V1.1)
* Source: Tinkerforge KiCad Libraries <https://github.com/Tinkerforge/kicad-libraries>

### OLED_128x64
* Status: Included (CERN OHL V1.1)
* Source: Tinkerforge KiCad Libraries <https://github.com/Tinkerforge/kicad-libraries>
* Notes: Modified using Autodesk Fusion 360 to meet project needs

### TL1105SPFxxxx
* Status: Included (Terms unspecified, linked from Digi-Key product page)
* Source: Manufacturer <http://stp-3d-models.e-switch.com/stp3dmodels/TL1105SPFxxxx.stp>

### KSU213WLFG
* Status: Included ([Terms](https://www.cadenas.de/terms-of-use-3d-cad-models)
  somewhat unclear, but may be okay for project use.)
* Source: Digi-Key Distributor OEM Embedded PARTcommunity Portal

### 0ZCJ0025FF2E
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/0ZCJ0025FF2E/Bel%20Fuse/view-part/>

### BM02B-SRSS-TB
* Status: Not included (Not redistributable)
* Source: https://www.jst-mfg.com/product/detail_e.php?series=231

### BM04B-SRSS-TB
* Status: Not included (Not redistributable)
* Source: https://www.jst-mfg.com/product/detail_e.php?series=231

### SM02B-SRSS-TB
* Status: Not included (Not redistributable)
* Source: https://www.jst-mfg.com/product/detail_e.php?series=231

### SM04B-SRSS-TB
* Status: Not included (Not redistributable)
* Source: https://www.jst-mfg.com/product/detail_e.php?series=231

### EVQP7-LC-01P
* Status: Not included (Download requires registration, terms of use restrictive)
* Source: Manufacturer <https://www3.panasonic.biz/ac/ae/search_num/index.jsp?c=detail&part_no=EVQP7L01P>

### USB4105-15-A-120
* Status: Included (Terms unspecified, downloaded from vendor page linked from Digi-Key product page)
* Source: Manufacturer <https://gct.co/connector/digi-key/usb4105>

### AMS\_TSL2585
* Status: Included
* Source: Created for project using Autodesk Fusion 360

### LED\_2835
* Status: Included
* Source: GrabCAD <https://grabcad.com/library/led-smd-2835-1>

### Nexperia\_SOT762-1
* Status: Included
* Source: Manufacturer <https://www.nexperia.com/packages/SOT762-1.html>
* Notes: Re-saved via Fusion 360 to fix STEP color parsing in KiCad

### ECS\_327MVATX-2-CN-TR
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/ECS-327MVATX-2-CN-TR/ECS Inc./view-part/>

### Seoul\_Viosys\_Z5\_CUN26A1B
* Status: Included
* Source: GrabCAD <https://grabcad.com/library/seoul-viosys-z5-cun26a1b-1>
