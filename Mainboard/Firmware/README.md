# Motherboard Firmware

The "motherboard" is part of the SensorCard platform. It can hold up to eight daughter cards, each of which contain a SPI ADC device. These ADCs are then sampled by the motherboard's MCU. The ADC samples are transmitted back to the AMDC so control algorithms can make use of the extra analog inputs. Several daughter card designs exist:

1. Current sense
2. High Voltage sense (for bus voltage levels)
3. Low Voltage sense (for +/- 10V levels)

This folder houses all the files needed to build the firmware which runs on the MCU on the motherboard PCB.

## Required Tools

To easily build this firmware and load it onto a motherboard, please install the following software on your host PC:

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) -- the latest version is fine.

This tool is analogous to the Xilinx SDK tool (for the AMDC). It is effectively the Eclipse IDE but with customizations from ST. You can write all the C code here, compile it, flash the MCU, debug, etc.

## Development Steps

To develop code for STM32 processors:

#### Generating New Projects

1. Use the STM32CubeIDE to generate a new project
2. When configuring this project, select the exact MCU device which you are targetting (the motherboard uses `STM32F765ZGT6`)
3. Open the CubeMX perspective (i.e. `*.ioc` file) and configure all desired peripherals (SPIs, UARTs, ETH, etc)
4. Change to the clock configuration tab and use the GUI to configure the PLL settings
5. Generate initialization code to boot-strap your project. This will configure the clock and peripherals like you specified.

#### Writing Code

6. Now that you have a base project, write all your application-specific code...

#### Testing / Debugging Code

7. Use the IDE to compile your code and ensure there are no errors.
8. To flash the hardware, plug in a SWD debugger device to your PC + board (highly recommend [`STLINK-V3SET`](https://www.digikey.com/product-detail/en/stmicroelectronics/STLINK-V3SET/497-18216-ND/9636028))
9. Configure a debug session in the IDE
10. Run the debug sessions. This will flash the MCU and start the code (probably breakpoint at `main()`)

NOTE: The STM32 devices are typically programmed into non-volatile memory. Therefore, there is no seperate "flashing" step. Every time you upload code, it is permanantely stored on the processor. You can power cycle the device and your code will start running again.

## Motherboard Firmware Design

The motherboard firmware is fairly simple, yet very specialized for the application. Before changing *anything* in the code, make sure you understand how it works. Practically every line of the code is optimized for speed and efficiency! Using a multi-channel logic analyzer / oscilloscope is absolutely required when updating the motherboard firmware to validate code timing.

**Do not blindly change the code.**
