/* 
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
*/

#include <Servo.h>

Servo myservo;  // create servo object to control a servo
// twelve servo objects can be created on most boards

//PCD8544 lcd;


// include the library code: HD44780
#include <LiquidCrystal.h>

enum state{setting, countdown, exposure_timer} systemState;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);


int potPin = A0;    // change value
int potVal = 0;
int roundedExposureTime = 0;  // rounded to 1/3 stops
float reciprocityCorrectionStops = 0;
int roundedExposureTimeReciprocity = 0;  // rounded to 1/3 stops

int buttonPin = 8;  // start/stop
int prevButtonVal = 0;
int buttonVal = 0;

int switchPin = 6;  // switch compensation for reciprocity failure
int prevSwitchVal = 0;
int switchVal = 0;

//int pos = 0;    // variable to store the servo position

int servoPosOff = 170;
int servoPosOn = 0;

//
unsigned long exposureTime = 0;
unsigned long countDownDurationMillis = 3000;

unsigned long stopCountDownMillis = 0;
unsigned long startExposureTimeMillis = 0;
unsigned long stopExposureTimeMillis = 0;

unsigned long currentMillis = 0;

char s[16];

void setup() {
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  //lcd.begin(84, 48);  // nokia
  myservo.write(servoPosOff);
          
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  pinMode(buttonPin, INPUT);           // set pin to input
  digitalWrite(buttonPin, HIGH);  

  pinMode(switchPin, INPUT);           // set pin to input
  digitalWrite(switchPin, HIGH);  

  systemState = setting;
}

void loop() {
  /*lcd.setCursor(0, 0);
  lcd.print("   WELCOME  ");
  lcd.setCursor(0, 1);
  lcd.print("     To");
  lcd.setCursor(0,2);
  lcd.print("blabla");  
  */
  potVal = analogRead(potPin);    // read the value from the sensor
  prevButtonVal = buttonVal;
  buttonVal = digitalRead(buttonPin);
  prevSwitchVal = switchVal;
  switchVal = digitalRead(switchPin);
  // most ugly debouncing ever
  /*if (prevButtonVal != buttonVal) 
  {
    delay(100);
  }*/
  currentMillis = millis();
  roundedExposureTime = round(pow(2, float(round((float(potVal) / 115) * 3)) / 3));  // resize roughly 0-1000 to 0-9, then round to 1/3's 
  reciprocityCorrectionStops = 0.5167 * log(pow(2, float(potVal) / 115)) - 0.2; // kodak portra 400, from: https://www.flickr.com/groups/477426@N23/discuss/72157635197694957/
  roundedExposureTimeReciprocity = round(pow(2, float(round(((float(potVal) / 115) + reciprocityCorrectionStops) * 3)) / 3)); 
  
  switch (systemState) 
  {
     case setting:
        if (prevSwitchVal != switchVal) {
          lcd.clear();
        }
        sprintf(s, "TIME        %4d", roundedExposureTime);
        lcd.setCursor(0, 0);
        lcd.print(s);  
        if (switchVal)
        {
          sprintf(s, "PORTRA COMP.%4d", roundedExposureTimeReciprocity);
          lcd.setCursor(0, 1);
          lcd.print(s);  
        }
        if ((prevButtonVal == 1) && (buttonVal == 0)) 
        {
          if (switchVal)
          {
            exposureTime = roundedExposureTimeReciprocity;
          } else {
            exposureTime = roundedExposureTime;
          }
          stopCountDownMillis = currentMillis + countDownDurationMillis;
          myservo.write(servoPosOff);
          lcd.clear();
          systemState = countdown; 
        }
        break;
     
     case countdown:
        sprintf(s, "EXPOSURETIME%4d", exposureTime);
        lcd.setCursor(0, 0);
        lcd.print(s);  

        sprintf(s, "COUNTDOWN   %4d", min(1 + (stopCountDownMillis - currentMillis) / 1000, 9999));
        lcd.setCursor(0, 1);
        lcd.print(s);
        if ((prevButtonVal == 1) && (buttonVal == 0)) 
        {
          lcd.setCursor(0, 1);
          lcd.print("COUNTDOWNSTOPPED");
          delay(1000);
          lcd.clear();
          systemState = setting;
        }
        if (currentMillis > stopCountDownMillis)
        {
          startExposureTimeMillis = currentMillis;
          stopExposureTimeMillis = currentMillis + exposureTime * 1000;
          myservo.write(servoPosOn);
          lcd.clear();
          systemState = exposure_timer;
        }
        break;
     
     case exposure_timer:
        sprintf(s, "EXPOSING    %4d", min(exposureTime, 9999));
        lcd.setCursor(0, 0);
        lcd.print(s);
        lcd.setCursor(0, 1);
        lcd.print(min((currentMillis - startExposureTimeMillis) / 1000, 9999));
        sprintf(s, "%4d", min((stopExposureTimeMillis - currentMillis) / 1000, 9999));
        lcd.setCursor(12, 1);
        lcd.print(s);
        if ((currentMillis > stopExposureTimeMillis) || ((prevButtonVal == 1) && (buttonVal == 0)))
        {
          myservo.write(servoPosOff);
          lcd.clear();
          lcd.write("DONE!");
          delay(1000);
          lcd.clear();
          systemState = setting;
        }
        break;
  }  
  

  //myservo.write(170);              // tell servo to go to position in variable 'pos'
  //delay(5000);                       // waits 15ms for the servo to reach the position
  //myservo.write(0);              // tell servo to go to position in variable 'pos'
  //delay(5000);                       // waits 15ms for the servo to reach the position
}


