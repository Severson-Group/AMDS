# High Voltage Card

This document describes the design consideration and implementation details for the high voltage card, which serves as a daughter card for the AMDS mainboard.
A block diagram is presented and each component is discussed in detail. Specifications of each component are provided based on the datasheet.

## Relevant Hardware Versions

REV C

<img src="Images/PCB_3D.png" width="25%"/>

## Design Requirements and Considerations

The voltage measurement card was designed to the following specifications:

1. Voltage measurement range of +/- 500V
2. SPI output to interface with the AMDS mainboard
3. +/- 10V BNC output

## Block Diagram
The high level block diagram of the voltage sensor card is shown below:

<img src="Images/Voltage_card.svg" />

### Voltage Sensor
The voltage sensor selected is the LEM LV 25-P, which basically senses the current proportional to the voltage to be measured. The sensor has a nominal input current of 10mA, an accuracy of 0.8%, and a linearity error under 0.2%. Two 25kΩ, 5W input resistors `R9` and `R10` are used to get a differential voltage measuring range up to +/- 500V. 
The sensor output is a current in the range of +/-25mA. 

To vary the voltage measurement range, the input resistors `R9` and `R10` can be suitably varied such that the input current to the sensor is limited to 10mA.
For example, to get a voltage measurement range of 250V, the resistor values of `R9` and `R10` should be reduced to 12.5kΩ each.

### Burden Resistor
The output of the voltage sensor is a current in the range of +/-25mA, which is converted to a voltage in the desired range using a burden resistor `R5`. The present implementation converts the sensor output current to a +/-8.75V signal using a burden resistor of 350Ω.

### Level shift stage
The voltage across the burden resistor is a bipolar signal (voltage span includes both positive and negative voltages). A non-inverting level translation circuit is designed using Op Amps as shown here:

<img src="Images/volt-sensor-opamp-stage.svg" width="30%" />

This circuit is used to translate the voltage across the burden resistor, which is bipolar, to the ADC input range of 0-4.5V. The resistor values can be calculated analytically using the following formula:

<img src="https://latex.codecogs.com/gif.latex?V_\text{out}%20%3D%20%5Cfrac%7BR_b%7C%7CR_c%7D%7BR_a&plus;%28R_b%7C%7CR_c%29%7D%5Ctimes%20V_%5Ctext%7BBURDEN%7D&plus;%5Cfrac%7BR_a%7C%7CR_b%7D%7BR_c&plus;%28R_a%7C%7CR_b%29%7D%5Ctimes%20V_%5Ctext%7BREF%7D" />

The algebra can get quite complicated when solving it analytically. So the resistor values were computed to be R<sub>a</sub> = 10kΩ, R<sub>b</sub> = 8.45kΩ, and R<sub>c</sub> = 4.64kΩ using the TI analog engineer's calculator.

**Note:** As the op-amp output voltage approaches the supply rails, it tends to distort and behave nonlinearly so the output voltage is limited to actually be 0.2V to 4.5V.

### RC Filter
A first order RC filter is implemented on the output of the op amp circuit. The cutoff frequency was set at 48kHz and the following equations was used for the computation:

_f_<sub>c</sub> = 1 / (2 π _RC_)

**Note:** The cutoff frequency can easily be changed by swapping out `R3`.

### Analog to Digital Converter
To increase noise immunity, the card has an inbuilt Analog to Digital Conversion (ADC) IC. The ADC used is the Texas Instruments ADS8860. It is pseudo-differential input, SPI output, SAR ADC. The maximum data throughput for a single chip is 1 MSPS but decreases by a factor of N for N devices in the daisy-chain. 

The different stages of the voltage sensor card described above convert the input voltage, to a voltage in the range of 0.2V - 4.5V. The voltage at the ADC input V<sub>ADC</sub> is related to the sensor card input voltage V<sub>IN</sub> as follows:

<img src="https://latex.codecogs.com/gif.latex?V_\text{ADC}&space;=&space;\frac{R_b||R_c}{R_a&plus;(R_b||R_c)}\left(&space;\frac{V_\text{IN}R_\text{BURDEN}}{R_\text{IN}}K_\text{N}\right)&plus;\frac{R_a||R_b}{R_c&plus;(R_a||R_b)}\times&space;V_\text{REF}" />

where R<sub>BURDEN</sub> is the burden resistor, R<sub>IN</sub> is the input resistance, and K<sub>N</sub> is the sensor conversion ratio from the datasheet. For LV-25P, the data sheet lists K<sub>N</sub> = 2500:1000.

For the current design, 0V input voltage corresponds to 2.35V at the ADC input. The positive peak corresponds to 4.5V and the negative peak corresponds to 0.2V.

### Connectors
- The HV terminal block `B1` with screw connectors is used to connect the HV terminals across which voltage has to be measured. 
- A screw terminal block `P1` is used to connect the +-15V supply for the current sensor
- A BNC terminal is available to directly measure the output across the burden resistor.

### Footprints
A user may want to change some of the passive components based on the range required and the RC filter cutoff frequency desired. The footprints of passive components that may need to be replaced i.e, the burden resistor (`R5`), the resistors in the Op Amp stage, and the RC filter components is provided here for quick reference. Note that these footprints are imperial codes and **not metric codes**.

| Component | Function | Footprint | 
| ---- | ----- |-----------|
| R3   | RC filter resistor | 0603| 
| R4   | Level shift stage|0603 |
| R5 | Burdern resistor |2512 |
| R6 |Level shift stage |0603 |
| R8 |Level shift stage |0603 |
| R9 | Input resistor|Through hole|
| R10| Input resistor |Through hole|
| C5 | RC filter capacitor |0603 |

### Datasheets

- [Voltage sensor](../datasheets/LV25P_Sensor.pdf)
- [Op Amp](../datasheets/OPA320_OpAmp.pdf)
- [Voltage Reference (LDO)](../datasheets/REF5045_LDO.pdf)
- [ADC](../datasheets/ADS_8860_ADC.pdf)
