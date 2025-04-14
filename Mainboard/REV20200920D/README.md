# AMDS REV20200920D

This folder contains the compiled design outputs for AMDS REV D. This includes the schematics, images of the PCB layout, the bill of materials (BOM), and the files needed to order fully populated boards from an online vendor.

## Potential Vendors

| Vendor Name             | Country      | Type |
|----------------------|-------------|--|
| [Screaming Circuits](https://www.screamingcircuits.com/)| US| Fabrication/Assembly |
| [OshPark](https://oshpark.com/home)| US| Fabrication |
| [Sunstone Circuits](https://www.sunstone.com/) | US | Fabrication |
| [PCBWay](https://www.pcbway.com/) | China | Fabrication/Assembly |
| [JLCPCB](https://jlcpcb.com/?_gl=1%2a1kjh7v7%2a_gcl_au%2aMzYwNzcxMDQwLjE3NDMxODc1MTI.%2a_ga%2aMjM3NTY3OTQzLjE3NDMxODc1MTI.%2a_ga_VB33MLCQS2%2aMTc0MzE4NzUxMi4xLjAuMTc0MzE4NzUxMi42MC4wLjA.%2a_ga_BZ8D96C9TK%2aMTc0MzE4NzUxMi4xLjAuMTc0MzE4NzUxMi42MC4wLjA.) | China | Fabrication/Assembly |

## Board Parameters

| Question                 | Answer          |
|--------------------------|-----------------|
|Board Size                | 203.4 x 203.4 mm|
|Number of Layers          | 4 Layers        |
|Double Sided              | No              |
|Surface Finish            | Tin lead        |
|Silkscreen                | White           |
|Board Type                | Single piece    |
|Material                  | FR-4, TG150     |
|Board Thickness           | 1.6 mm          |
|Min Trace Spacing         | 0.15 mm (6 mil) |
|Min Hole Size             | 0.35 mm (14 mil)|
|Solder Mask               | Black           |
|"HASL" to "ENIG"          | No              |
|Finished Copper           | 1 oz            |

## Assembly Details

| Question                       | Answer           |
|--------------------------------|------------------|
| Unique Part Count[^1]          | 37               |
| Number of Total Parts          | 191              |
| Total SMT Part Count           | 167              |
| Total Thru Hole Part Count     | 24               |
| Total Mechanical Screws        | 54               |
| Total Mechanical Standoffs[^2] | 27               |
[^1]: This number does not include screws or standoffs. With screws and standoffs included, the unique part count is 39.

[^2]: There are a total of 27 standoffs per board, where 11 standoffs will be used for the mainboard mounting holes, and 16 standoffs will be used for mounting all the 8 sensor cards.

## Bill of Materials
The [Bill of Materials (BOM)](mainboard-bom.csv) contains components for 1 board. If users of this repo discover that any components are unvailable, please maintain an `alternate-components.md` file in this folder that lists approved alternates.
