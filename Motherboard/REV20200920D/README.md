This folder contains schematics pdf, PCB pdf, bill of material (BOM), 3-D PCB, gerber files and all the required files to fabricate the boards.

## How to order 10 PCB boards

1. Place a new quote in https://www.pcbway.com/ 

2. Use the following table to set the pcb properties.

| Question             | Answer      |
|----------------------|-------------|
|Size| 203.4 x 203.4 mm|
|Layer| 4 Layers|
|Quantity| 10|
|Board type| Single pieces|
|Different Design in Panel| 1|
|Material: FR-4| TG150|
|Thickness| 1.6 mm|
|Min Track/Spacing| 6/6mil|
|Min Hole Size| 0.3mm|
|Solder Mask| Black|
|Silkscreen| White|
|Gold fingers| No|
|Surface Finish| HASL with lead|
|"HASL" to "ENIG"| No|
|Via Process| Tenting vias|
|Finished Copper| 1 oz Cu (Inner Copper: 1 oz)|
|Additional options| UL Marking: None|

This table includes all parts in BOM for one board. 

| Question                  | Answer           |
|---------------------------|------------------|
| Number of Unique Elec. Parts    | 37               |
| Number of Total Elec. Parts     | 191              |
| Number of Total Elec. SMT Parts | 167              |
| Number of Total Elec. THT Parts | 24               |
| Number of Mech. Screws          | 54               |
| Number of Mech. Standoffs       | 27               |

There is a total of 27 standoffs per board, where 11 standoffs will be used for the motherboard mounting holes, and 16 standoffs will be used for mounting all the 8 daughtercards.

3. Upload the gerbers from the [gerber folder](https://github.com/Severson-Group/SensorCard/tree/feature/Add_Motherboard_assembly_files/Motherboard/REV20200920D/gerbers).

4. Place the order, it should arrive in 7-9 days.

## How to order components for the board

1. The BOM contains components for 10 boards. The DNP components are not included in the BOM.

2. Upload the [BOM](https://github.com/Severson-Group/SensorCard/blob/feature/Add_Motherboard_assembly_files/Motherboard/REV20200920D/SensorMotherBoard_BOM_10qty.csv) in https://www.digikey.com/ordering/shoppingcart. 

3. Place the order.
