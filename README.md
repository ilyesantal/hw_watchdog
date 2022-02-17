creator: Antal Ilyes
date: January 2022 - February 2022
This repository was made to contain the development files of a hardware watchdog project for SoftFlex kft.
The hardware watchdog is made of a PCB and a program written in embedded C.
The device is controlled by an ATTyin10 microcontroller. It periodically gets an "alive" tick from the devices connected to it. When an input is driven high, it also turns on a green LED (through an NPN transistor), to visualize it. There is also a red LED indicating the device is powered.
There are two channels that the device can control, as there are only 4 pins on the ATTiny10.
The printed circuit board project can be found under the PCB library. The design was made with KiCAD 6.0
The program works in the following way:
  Initialization: The pins of the microcontroller are set. The corresponding pins to output and to input. The input pins' internal pull-up resistors are also enabled. Then the timer which counts the time passed since the last tick is initialized. Lastly, the interrupts are set up for the input pins.
  Timer: There is only one timer in the ATTiny10, so everything has to be done in this. For every trigger (which happens in a frequency that can be set up using the defines), it checks the status of both devices. If the device is inactive, there is nothing to do. If a device is in init status, it is counting the time between the first two ticks. If the device is in active mode, the timer counts the time passed since the last tick.
  Input: When an input is driven high, it triggers an interrupt. In the ISR (Interrupt Service Routine) the program determines which input generated the interrupt (as the interrupt vector cannot distinguish which pin did, it triggers when any set pin is triggered), then checks the currenct status of the device corresponding to the pin, and changes the status based on that.
  Main loop: If there is a device, which reached the point, where it should be reset, it is indicated to the main loop by the counter which shows that there is a time for which the output should be driven high (which makes the relay to switch off). After it was driven high for the given time, it sets the output back to low.
