# Firmware Architecture

**Description of the architecture design of the AMDS mainboard firmware.**

## Contents

- [Introduction](#introduction)
- [Architecture](#architecture)
- [Future Improvements](#future-improvements)

## Introduction

This document outlines the firmware architecture which runs the AMDS mainboard. By understanding how the mainboard works, users will be able to understand the performance limitations of the system. While the current firmware design will work for most applications, some users will find that the design must be tweaked to meet their system performance goals. Potential ideas for improvements are provided in the following sections which could be implemented in the future.

## Architecture

The main goal of the AMDS mainboard is to interface voltage/current sensor cards to an external control board (typically the [AMDC](https://amdc.dev/)). Therefore, the AMDS platform should be thought of as a slave to the main controller (master). In steady-state operation, the master is responsible for triggering the AMDS to do two things: (1) sample the analog signals on the sensor cards (2) send the latest data to the master. Since the AMDS is typically used in motor drives, these operations happen in real-time at 1000s of times per second. To facilitate the desired real-time operation, the AMDS has an embedded processor which orchestrates its behavior. The firmware running on this processor directly determines the performance of the sensor interface in terms of sampling latency and throughput.

### Interface to Master

TODO...

### Interrupt-Driven Design

TODO...

### Performance Limitations

TODO...

## Future Improvements

Foo bar.
