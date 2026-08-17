# Changelog

This document summarizes the changes introduced to the code base for each release.

## v3.0.2

- Fix daisy chain race condition in data process routing function
- Optimize timing for daisy chain communication

## v3.0.1

- Fix swapped `UART` channels on `FBC` board when daisy chained

## v3.0.0

- Add daisy chain support
- Add daisy chain adapter board
- Update baud rate to 20MHz
- Update `adc_sample` function to be faster
- Rename motherboard folder to mainboard

## v2.0.1

- Fix and standarize PCB release file packages for all boards

## v2.0.0

- Sample and transmit data upon receipt of `SYNC_ADC` interrupt
- Remove `SYNC_TX` interrupt

## v1.0.0

Initial commit of released code base.

- Communication interface for AMDC v1.0.x - v1.2.x
