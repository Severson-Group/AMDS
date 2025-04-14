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

| Question             | Answer      |
|----------------------|-------------|
|Board Size| 203.4 x 203.4 mm|
|Number of Layer| 4 Layers|
|Surface Finish| Tin lead|
|Silkscreen| White|
|Board type| Single pieces|
|Different Design in Panel| 1|
|Material: FR-4| TG150|
|Thickness| 1.6 mm|
|Min Track/Spacing| 6/6mil|
|Min Hole Size| 0.3mm|
|Solder Mask| Black|
|Gold fingers| No|
|"HASL" to "ENIG"| No|
|Via Process| Tenting vias|
|Finished Copper| 1 oz Cu (Inner Copper: 1 oz)|
|Additional options| UL Marking: None|


## Assembly Details

| Question                       | Answer           |
|--------------------------------|------------------|
| Unique Part Count[^1]          | 37               |
| Number of Total Parts          | 191              |
| Total SMT Part Count           | 167              |
| Total Thru Hole Part Count     | 24               |
| Double Sided                   | No               |
| Total Mechanical Screws        | 54               |
| Total Mechanical Standoffs[^2] | 27               |
[^1]: This number does not include screws or standoffs. With screws and standoffs included, the unique part count is 39.

[^2]: There are a total of 27 standoffs per board, where 11 standoffs will be used for the mainboard mounting holes, and 16 standoffs will be used for mounting all the 8 sensor cards.

## Bill of Materials
The [Bill of Materials (BOM)](Mainboard_BOM.csv) contains components for 1 board. If users of this repo discover that any components are unvailable, please maintain an `alternate-components.md` file in this folder that lists approved alternates.
