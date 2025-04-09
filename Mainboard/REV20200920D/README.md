This folder contains schematics pdf, PCB pdf, bill of material (BOM), 3-D PCB, gerber files and all the required files to fabricate the boards.

## How to order a board

1. Start a quote from any of the following vendors.

| Vendor Name             | Country      | Type |
|----------------------|-------------|--|
| [Screaming Circuits](https://www.screamingcircuits.com/)| US| Fabrication/Assembly |
| [OshPark](https://oshpark.com/home)| US| Fabrication |
| [Sunstone Circuits](https://www.sunstone.com/) | US | Fabrication |
| [PCBWay](https://www.pcbway.com/) | China | Fabrication/Assembly |
| [JLCPCB](https://jlcpcb.com/?_gl=1%2a1kjh7v7%2a_gcl_au%2aMzYwNzcxMDQwLjE3NDMxODc1MTI.%2a_ga%2aMjM3NTY3OTQzLjE3NDMxODc1MTI.%2a_ga_VB33MLCQS2%2aMTc0MzE4NzUxMi4xLjAuMTc0MzE4NzUxMi42MC4wLjA.%2a_ga_BZ8D96C9TK%2aMTc0MzE4NzUxMi4xLjAuMTc0MzE4NzUxMi42MC4wLjA.) | China | Fabrication/Assembly |


2. Use the following table to set the pcb properties.

### Fabrication Details
| Question             | Answer      |
|----------------------|-------------|
|Board Size| 203.4 x 203.4 mm|
|Number of Layer| 4 Layers|
|Surface Finish| HASL with lead|
|Board type| Single pieces|
|Different Design in Panel| 1|
|Material: FR-4| TG150|
|Thickness| 1.6 mm|
|Min Track/Spacing| 6/6mil|
|Min Hole Size| 0.3mm|
|Solder Mask| Black|
|Silkscreen| White|
|Gold fingers| No|

|"HASL" to "ENIG"| No|
|Via Process| Tenting vias|
|Finished Copper| 1 oz Cu (Inner Copper: 1 oz)|
|Additional options| UL Marking: None|


### Assembly Details
| Question                  | Answer           |
|---------------------------|------------------|
| Unique Part Count    | 37               |
| Number of Total Parts     | 191              |
| Total SMT Part Count | 167              |
| Total Thru Hole Part Count | 24               |
| Double Sided   | No      |
| Total Mechanical Screws          | 54               |
| Total Mechanical Standoffs       | 27               |

There are a total of 27 standoffs per board, where 11 standoffs will be used for the mainboard mounting holes, and 16 standoffs will be used for mounting all the 8 sensor cards.

1. Upload the gerbers from the [gerber folder](gerbers).

2. Place the order.

## How to order components for the board seperately

1. The BOM contains components for 1 board.

2. Upload the [BOM](Mainboard_BOM.csv) in https://www.digikey.com/ordering/shoppingcart. 

3. Place the order.
