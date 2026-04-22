# AMDS Daisy Chain Adapter Board

This document describes the design considerations and implementation details for the Daisy Chain Adapter board. The purpose of the board is to provide the AMDS the ability to daisy chain multiple times.

## Relevant Hardware Versions

| Board                 | Version |
| --------------------- | ------- |
| AMDS                  | REV D   |
| Daisy Chain Adapter   | REV A   |

## Design Requirements and Considerations

The Daisy Chain Adapter board was designed with the following requirements:

1. Differential and single ended sides should be isolated from each other
2. Include screw terminal for powering the board
3. Testpoints to allow ease of Saleae probing

## DB15 Connector 1: Differential

Below is a description of the pin configuration on the differential sided DB-15 connector, which is the connection point to the AMDC Link port on the AMDS

| Pin # | Description  | Voltage Level |
| ----- | ------------ | ------------- |
| 1     | 5V_CN        | 5V            |
| 2     | UARTA_IN_P   | 5V            |
| 3     | UARTA_IN_N   | 5V            |
| 4     | UARTB_IN_P   | 5V            |
| 5     | UARTB_IN_N   | 5V            |
| 6     | NC           | -             |
| 7     | NC           | -             |
| 8     | NC           | -             |
| 9     | NC           | -             |
| 10    | NC           | -             |
| 11    | GND_CN       | 5V            |
| 12    | UARTA_OUT_P  | 5V            |
| 13    | UARTA_OUT_N  | 5V            |
| 14    | UARTB_OUT_P  | 5V            |
| 15    | UARTB_OUT_N  | 5V            |

## DB15 Connector 2: Single-ended

Below is a description of the pin configuration on the single-end sided DB-15 connector, which is the connection point to the IO Link port on the AMDS

| Pin # | Description  | Voltage Level |
| ----- | ------------ | ------------- |
| 1     | 3V3          | 3V3           |
| 2     | UARTA_IN     | 3V3           |
| 3     | UARTA_OUT    | 3V3           |
| 4     | UARTB_IN     | 3V3           |
| 5     | UARTB_OUT    | 3V3           |
| 6     | NC           | -             |
| 7     | NC           | -             |
| 8     | NC           | -             |
| 9     | NC           | -             |
| 10    | NC           | -             |
| 11    | NC           | -             |
| 12    | NC           | -             |
| 13    | NC           | -             |
| 14    | GND          | 3V3           |
| 15    | GND          | 3V3           |

## Datasheets

- [DTS Transceiver](https://www.ti.com/lit/ds/symlink/iso3086t.pdf)
