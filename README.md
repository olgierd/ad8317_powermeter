# 10GHz AD8317 RF power meter

![Picture of OLED display](https://pbs.twimg.com/media/EOG-YURXsAAglKo?format=jpg&name=large)

## Features
* -55 to 0 dBm RF power measurement up to 10 GHz
* 2-point calibration (linear function), separate for each band
* Measuring `average`, `max` (peak) and `min` power.
* Ability to use an external RF attenuator
* Headless mode - control over serial only

## Parts

* Arduino Nano
* AD8317 power meter module
* SSD1306 128x32px OLED display
* LM4040 reference voltage (2.048 V)

## Assembly

* Build and flash Arduino software
* Build a voltage reference and connect it to `AREF` pin of Arduino, VCC (5V) and GND.
![Reference voltage schematic](reference_voltage.png)
* Connect AD8317 module output to `A0` pin of Arduino, connect the grounds together. Provide 5V to the module.
* Connect OLED display to VCC, GND & SDA (`A4`), SCL (`A5`) Arduino pins.

## TODO
* Add buttons
* Show power also in mW/W
* Display raw ADC readout for calibration
* Store settings in EEPROM
