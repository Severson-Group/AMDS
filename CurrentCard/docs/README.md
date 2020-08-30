# Current Measurement Card

This document describes the design considerations and implementation details for the current measurement card. 
A block diagram is presented and each component is discussed in detail. Specifications of each component are provided based on the datasheet.

## Relevant Hardware Versions

REV C

## Design Requirements and Considerations

The current measurement card was designed to the following specifications:

1. Current measurement range of +/- 55V
2. Noise immunity
3. Quick adjustment of the sensing range
4. High sensor bandwidth
5. SPI output to interface with the AMDC

## Block Diagram

< TO BE ADDED>

### Current Sensor
LEM LA 55-P current sensor for this design as it is the only sensor available from LEM with an open aperture and PC pins that can measure +/-55A. 
The open aperture was a requirement as it allows for the range to be easily scaled down just by adding turns to the primary. 
The LA 55-P is a closed loop compensated hall effect transducer that has an accuracy of +/-0.65% and linearity of <0.15% which is quite good compared to other sensors from LEM. 
It has an excellent bandwidth of 200khz and a low impedance current output that is inherently more immune to noise than a high impedance voltage output. 

### Burden Resistor
A burden resistor (`R5`) is used to convert the current output of the sensor to a voltage. The burden resistance, _R_<sub>_burden_</sub> was calculated using the following equation

_V_<sub>_burden_</sub>  = _I_<sub>_primary, max_</sub>(_N_<sub>2</sub>/_N_<sub>1</sub>)_R_<sub>_burden_</sub>

_R_<sub>_burden_</sub>  = (10 V/70 A)*(1000/1) = 143Ω 

The LA 55-P datasheet specifies the burden resistor value must be between 135Ω and 155Ω so a 150Ω resistor was selected.


### ADC
The voltage across the burden resistor is a bipolar signal (voltage span includes both positive and negative voltages) that is single ended because the burden resistor is referenced to common. 
For this reason, a single-ended ADC was selected. The ADC used is the Texas Instruments ADS8860. It is pseudo-differential input, SPI output, SAR ADC. 
The maximum data throughput for a single chip is 1 MSPS but decreases by a factor of N for N devices in the daisy-chain. 
The input voltage range is 0-5V. Therefore, an intermediate stage is needed to shift and scale the voltage from -10V – 10V to the ADC input range.

### Op Amps

< To BE ADDED>




