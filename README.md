(Analog) camera helper: Exposure setter using cable shutter.

Features:
- Servo to "(un)press" the cable shutter -> https://www.arduino.cc/en/reference/servo

- Rotary encoder for all the settings
- Potentiometer -> https://www.arduino.cc/en/tutorial/AnalogInput

- Start/stop button


- Nokia 5110 monochrome screen 84x48 -> (Hacky, no level shifters!) https://create.arduino.cc/projecthub/muhammad-aqib/interfacing-nokia-5110-lcd-with-arduino-7bfcdd
- Hitachi HD44780 -> https://www.arduino.cc/en/Tutorial/HelloWorld

 The circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * LCD VSS pin to ground
 * LCD VCC pin to 5V
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)
