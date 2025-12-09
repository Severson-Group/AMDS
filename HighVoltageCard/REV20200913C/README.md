# High Voltage Card REV20200913C

This folder contains the compiled design outputs for the High Voltage Card. This includes the schematics, images of the PCB layout, the bill of materials (BOM), and the files needed to order fully populated boards from an online vendor.

## Board Parameters

| Question                  | Answer          |
|---------------------------|-----------------|
| Board Size                | 3.57" x 1.46"   |
| Number of Layers          | 2 Layers        |
| Double Sided              | No              |
| Surface Finish            | Tin lead        |
| Silkscreen                | White           |
| Material                  | FR-4, TG150     |
| Board Thickness           | 1.6 mm          |
| Min Trace Spacing         | 0.15 mm (6 mil) |
| Min Hole Size             | 0.40 mm (16 mil)|
| Number of Holes           | 54              |
| Solder Mask               | Black           |
| HASL or ENIG              | No              |
| Finished Copper           | 1 oz            |

## Assembly Details

| Question                          | Answer           |
|-----------------------------------|------------------|
| Unique Part Count[^1]             | 23               |
| Number of Total Parts[^1]         | 42               |
| Total SMT Part Count              | 27               |
| Total Through-hole Part Count     | 15               |
| Total Mechanical Screws[^2]       | 2                |
| Total Mechanical Standoffs[^2]    | 2                |
| BGA / QFN                         | No               |

## Bill of Materials
The [Bill of Materials (BOM)](high-voltage-sensor-bom.csv) contains components for 1 board. If users of this repo discover that any components are unavailable, please maintain an `alternate-components.md` file in this folder that lists approved alternates.

[^1]: This number does not include screws or standoffs. With screws and standoffs included, the unique part count is 25 and total parts is 46. Users are advised to not have their assembly vendor place screws and standoffs.

[^2]: There are screws and standoffs required for mounting this card to the mainboard. For details on what those components are, please refer to the [Main Board](../../Mainboard/REV20200920D/README.md)