# AMDS Daisy Chain Adapter Board

This document describes the design considerations and implementation details for the Daisy Chain Adapter board. The purpose of the board is to provide the AMDS the ability to daisy chain multiple times.

## Relevant Hardware Versions

| Board                 | Version |
| --------------------- | ------- |
| AMDS                  | REV D   |
| Daisy Chain Adapter   | REV A   |

## Design Requirements and Considerations

The CAN board was designed with the following requirements:

1. Two CAN buses
2. Differential and single ended sides should be isolated from each other
3. Include screw terminal for powering the board
4. Testpoints to allow ease of Sal
5. Minimal routing on the bottom layer

## Block Diagram

The block diagram below illustrates the high level design of the signals as they come from the `AMDC` board to the `AMDS` boards.

## DB15 Connector 1: Differential

Below is a description of the pin configuration on the differential sided DB-15 connector, which is the connection point to the AMDC Link port on the AMDS

| Pin # | Description  | Voltage Level |
| ----- | ------------ | ------------- |
| 1     | -            | -             |
| 2     | -            | -             |
| 3     | -            | -             |
| 4     | -            | -             |
| 5     | -            | -             |
| 6     | -            | -             |
| 7     | -            | -             |
| 8     | -            | -             |
| 9     | -            | -             |
| 10    | -            | -             |
| 11    | -            | -             |
| 12    | -            | -             |
| 13    | -            | -             |
| 14    | -            | -             |
| 15    | -            | -             |

## DB15 Connector 2: Single-ended

Below is a description of the pin configuration on the single-end sided DB-15 connector, which is the connection point to the IO Link port on the AMDS

| Pin # | Description  | Voltage Level |
| ----- | ------------ | ------------- |
| 1     | -            | -             |
| 2     | -            | -             |
| 3     | -            | -             |
| 4     | -            | -             |
| 5     | -            | -             |
| 6     | -            | -             |
| 7     | -            | -             |
| 8     | -            | -             |
| 9     | -            | -             |
| 10    | -            | -             |
| 11    | -            | -             |
| 12    | -            | -             |
| 13    | -            | -             |
| 14    | -            | -             |
| 15    | -            | -             |

## Datasheets
- [DTS Transceiver](https://www.digikey.com/htmldatasheets/production/112178/0/0/1/st490ab.html)
- [CAN Transceiver](https://www.analog.com/media/en/technical-documentation/data-sheets/ADM3055E-3057E.pdf)
