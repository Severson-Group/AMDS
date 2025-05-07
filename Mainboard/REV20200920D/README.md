# AMDS REV20200920D

This folder contains the compiled design outputs for AMDS REV D. This includes the schematics, images of the PCB layout, the bill of materials (BOM), and the files needed to order fully populated boards from an online vendor.

## Board Parameters

| Question                  | Answer          |
|---------------------------|-----------------|
| Board Size                | 8" x 8"         |
| Number of Layers          | 4 Layers        |
| Double Sided              | No              |
| Surface Finish            | Tin lead        |
| Silkscreen                | White           |
| Material                  | FR-4, TG150     |
| Board Thickness           | 1.6 mm          |
| Min Trace Spacing         | 0.15 mm (6 mil) |
| Min Hole Size             | 0.35 mm (14 mil)|
| Number of Holes           | 569             |
| Solder Mask               | Black           |
| HASL or ENIG              | No              |
| Finished Copper           | 1 oz            |

## Assembly Details

| Question                          | Answer           |
|-----------------------------------|------------------|
| Unique Part Count[^1]             | 37               |
| Number of Total Parts[^1]         | 191              |
| Total SMT Part Count              | 167              |
| Total Through-hole Part Count     | 24               |
| Total Mechanical Screws           | 54               |
| Total Mechanical Standoffs[^2]    | 27               |
| BGA / QFN                         | No               |

[^1]: This number does not include screws or standoffs. With screws and standoffs included, the unique part count is 39 and total parts is 272. Users are advised to not have their assembly vendor place screws and standoffs.

[^2]: There are a total of 27 standoffs per board, where 11 standoffs will be used for the mainboard mounting holes, and 16 standoffs will be used for mounting all the 8 sensor cards.

## Bill of Materials
The [Bill of Materials (BOM)](mainboard-bom.csv) contains components for 1 board. If users of this repo discover that any components are unvailable, please maintain an `alternate-components.md` file in this folder that lists approved alternates. 

Note that several resistors are indicated with a value of `DNP` (do not populate). These are components that should not be soldered onto the board in standard builds to avoid ground loops and allow for daisy chain operation[^3].

[^3]: An audit of the DNP components is discussed in [this GitHub issue](https://github.com/Severson-Group/AMDS/issues/63).
