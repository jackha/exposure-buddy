#include <SPI.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

#include <Servo.h>
#include <math.h>

#define LCD_BACKLIGHT_TIMEOUT 30000
#define DONE_TIMEOUT 3000

#define BACKLIGHT_ON LOW
#define BACKLIGHT_OFF HIGH

#define COMPENSATION_MODE_NONE 0
#define COMPENSATION_MODE_PORTRA 1
#define COMPENSATION_MODE_ILFORD_PAN_FP 2  // =base index for ilford exponential lookup table t^c 
#define COMPENSATION_MODE_ILFORD_FP4 3
#define COMPENSATION_MODE_ILFORD_HP5 4

#define NUM_COMPENSATION_MODES 5

const char *compensationModeText[NUM_COMPENSATION_MODES] = {
  "",
  "PORTRA 160/400",
  "ILFORD PAN F+",
  "ILFORD FP4",
  "ILFORD HP5"
};

/* 
(Analog) camera helper: Exposure setter using cable shutter.


Features:
- Servo to "(un)press" the cable shutter -> https://www.arduino.cc/en/reference/servo

Using ServoTimer2 to avoid problems with timer (which is the case when using Nokia 5110)
https://github.com/nabontra/ServoTimer2


- Rotary encoder for all the settings
- Potentiometer -> https://www.arduino.cc/en/tutorial/AnalogInput

- Start/stop button


- Nokia 5110 monochrome screen 84x48 -> 
Adafruit 5110 library ver 1.1.1
Adafruit GFX 1.7.5

Pinout:
1 RST -> 3
2 CE -> 4
3 DC Data/Command -> 5
4 DIN Serial Data In -> 6
5 Clk -> 10
6 Vcc -> 3v
7 Light
8 Gnd -> gnd

https://www.adafruit.com/product/338

(Hacky, no level shifters!) https://create.arduino.cc/projecthub/muhammad-aqib/interfacing-nokia-5110-lcd-with-arduino-7bfcdd
*/

Servo myservo;  // create servo object to control a servo
//ServoTimer2 myservo;  // create servo object to control a servo

//PCD8544 lcd;

// Software SPI (slower updates, more flexible pin options):
// pin 10 - Serial clock out (SCLK)
// pin 6 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(10, 6, 5, 4, 3);

// include the library code: HD44780
//#include <LiquidCrystal.h>

enum state{setting, countdown, exposure_timer, done} systemState, prevSystemState;

// initialize the library with the numbers of the interface pins
//LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

int servoPin = 9;

int potPin = A0;    // change value
int potVal = 0;

int prevRoundedExposureTime = 0;  // for detecting change
int roundedExposureTime = 0;  // rounded to 1/3 stops
float reciprocityCorrectionStops = 0;
int roundedExposureTimeReciprocity = 0;  // rounded to 1/3 stops

int pot2Pin = A1;    // compensation for reciprocity failure
int pot2Val = 0;
int compensationMode = 0;
int prevCompensationMode = -1;

int buttonPin = 8;  // start/stop
int prevButtonVal = 0;
int buttonVal = 0;

int lcdBacklightVal = BACKLIGHT_ON;
int requestLcdBacklightVal = BACKLIGHT_ON;

//int pos = 0;    // variable to store the servo position

int servoPosOff = 160;  // if you hear a buzzing sound, you need to decrease this value
int servoPosOn = 0;   // if you hear a buzzing sound, you need to increase this value
int servoPos = -1;

int lcdBacklightPin = 11;

//
unsigned long exposureTime = 0;
unsigned long countDownDurationMillis = 3000;

unsigned long stopCountDownMillis = 0;
unsigned long startExposureTimeMillis = 0;
unsigned long stopExposureTimeMillis = 0;

unsigned long currentMillis = 0;
unsigned long lcdBacklightTimeoutMillis = 0;

unsigned long doneFinishedTimeMillis = 0;

unsigned long powerDrawTimeoutMillis = 0;


bool anythingChanged;
bool forceDisplay;

char s[16];

// To save power and to prevent any timer interference, only turn on servo when using it: this move is blocking!
void servoPosition(int pos)
{
  if (pos != servoPos)
  {
    servoPos = pos;
    myservo.attach(servoPin);
    myservo.write(pos);
    delay(1000);
    myservo.detach();
  }
}

float ilfordReciprocityCompensation(int film_type, float t)
{
  float lookup[3] = {
    1.33,  //PAN F+
    1.26,  //FP4+
    1.31   //HP5+
  };
  return pow(t, lookup[film_type-2]);
}


void setup() {

  servoPosition(servoPosOff);
          
  pinMode(buttonPin, INPUT);           // set pin to input
  digitalWrite(buttonPin, HIGH);  
  
  pinMode(lcdBacklightPin, OUTPUT);
  digitalWrite(lcdBacklightPin, LOW);
  lcdBacklightTimeoutMillis = millis() + LCD_BACKLIGHT_TIMEOUT;

  systemState = setting;
  prevSystemState = setting;

  display.begin();
  // init done

  // you can change the contrast around to adapt the display
  // for the best viewing!
  display.setContrast(60);

  display.clearDisplay();   // clears the screen and buffer
  display.setTextSize(1);
  display.setTextColor(BLACK);
  
}

void loop() {
  potVal = analogRead(potPin);    // read the value from the sensor
  pot2Val = analogRead(pot2Pin);
  
  prevButtonVal = buttonVal;
  buttonVal = digitalRead(buttonPin);

  // most ugly debouncing ever
  /*if (prevButtonVal != buttonVal) 
  {
    delay(100);
  }*/
  currentMillis = millis();
  prevRoundedExposureTime = roundedExposureTime;
  // The 0.03421571533791301 is here to round it off to 250,500,1000 instead of 256,512,1024, etc
  roundedExposureTime = round(pow(2, float(round((float(potVal+50) / 120) * 3)) / 3 - 0.03421571533791301));  // resize roughly 0-1000 to 0-9, then round to 1/3's 

  switch (compensationMode)
  {
    case COMPENSATION_MODE_PORTRA:
      reciprocityCorrectionStops = 0.5167 * log(pow(2, float(potVal+50) / 120)) - 0.2; // kodak portra 400, from: https://www.flickr.com/groups/477426@N23/discuss/72157635197694957/
      roundedExposureTimeReciprocity = round(pow(2, float(round(((float(potVal+50) / 120) + reciprocityCorrectionStops) * 3)) / 3)); 
      break;
    case COMPENSATION_MODE_ILFORD_PAN_FP:
    case COMPENSATION_MODE_ILFORD_FP4:
    case COMPENSATION_MODE_ILFORD_HP5:
      roundedExposureTimeReciprocity = ilfordReciprocityCompensation(compensationMode, roundedExposureTime);
      break;
    default:
      roundedExposureTimeReciprocity = roundedExposureTime;
      break;
  }
  
  prevCompensationMode = compensationMode;
  compensationMode = pot2Val * NUM_COMPENSATION_MODES / 1024;
  
  prevSystemState = systemState;
  anythingChanged = (prevRoundedExposureTime != roundedExposureTime) || (prevButtonVal != buttonVal) || (prevCompensationMode != compensationMode);
  
  forceDisplay = true;
  
  // update display and state
  switch (systemState) 
  {
     case setting:
        servoPosition(servoPosOff);

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.println("EXP.TIME");
        sprintf(s, "%4d", roundedExposureTime);
        display.setCursor(84-4*6,0);
        display.println(s);

        sprintf(s, "%2d:%02d", roundedExposureTimeReciprocity / 60, roundedExposureTimeReciprocity % 60);
        display.setCursor(84-5*6,3*8);
        display.println(s);

        if (compensationMode != COMPENSATION_MODE_NONE)
        {
          display.setCursor(0, 8);
          display.println("RECIPR.COMP.");
          display.println(compensationModeText[compensationMode]);
        }

        display.setTextSize(2);
        display.setCursor(84-4*12, 32);
        sprintf(s, "%4d", roundedExposureTimeReciprocity);
        display.println(s);
  
        if (anythingChanged)
        {
          lcdBacklightTimeoutMillis = currentMillis + LCD_BACKLIGHT_TIMEOUT;
          requestLcdBacklightVal = BACKLIGHT_ON;
        }
        if ((prevButtonVal == 1) && (buttonVal == 0)) 
        {
          if (compensationMode != COMPENSATION_MODE_NONE)
          {
            exposureTime = roundedExposureTimeReciprocity;
          } else {
            exposureTime = roundedExposureTime;
          }
          stopCountDownMillis = currentMillis + countDownDurationMillis;
          systemState = countdown; 
        }

        break;
     
     case countdown:
        servoPosition(servoPosOff);

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.println("COUNTDOWN");
        display.setCursor(0,40);
        display.println("EXP.TIME");
        display.setCursor(8*7,40);
        sprintf(s, "%4d", exposureTime);
        display.println(s);

        display.setTextSize(2);
        sprintf(s, "%d", min((1000 + stopCountDownMillis - currentMillis) / 1000, 999));
        display.setCursor(42 - strlen(s) * 7,16);
        display.println(s);
        if ((prevButtonVal == 1) && (buttonVal == 0)) 
        {
          systemState = setting;
        }
        if (anythingChanged)
        {
          lcdBacklightTimeoutMillis = currentMillis + LCD_BACKLIGHT_TIMEOUT;
          requestLcdBacklightVal = BACKLIGHT_ON;
        }
        if (currentMillis > stopCountDownMillis)
        {
          startExposureTimeMillis = currentMillis;
          stopExposureTimeMillis = currentMillis + exposureTime * 1000;
          requestLcdBacklightVal = BACKLIGHT_OFF;
          lcdBacklightTimeoutMillis = 0;
          systemState = exposure_timer;
        }

        break;
     
     case exposure_timer:
        servoPosition(servoPosOn);
     
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);

        display.println("EXPOSING....");
        display.setCursor(0,40);
        display.println("EXP.TIME");
        display.setCursor(56,40);
        sprintf(s, "%4d", min(exposureTime, 9999));
        display.println(s);
        
        //sprintf(s, "%4d", min((stopExposureTimeMillis - currentMillis) / 1000, 9999));
        display.setTextSize(2);
        sprintf(s, "%d", min((currentMillis - startExposureTimeMillis) / 1000, 9999));
        display.setCursor(42 - strlen(s) * 7,16);
        display.println(s);
        if ((currentMillis > stopExposureTimeMillis) || ((prevButtonVal == 1) && (buttonVal == 0)))
        {
          doneFinishedTimeMillis = currentMillis + DONE_TIMEOUT;
          requestLcdBacklightVal = BACKLIGHT_ON;
          lcdBacklightTimeoutMillis = 0;
          systemState = done;
        }
        break;
     case done:
        servoPosition(servoPosOff);
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(0, 0);
        display.println("DONE!");
        if (currentMillis > doneFinishedTimeMillis)
        {
          requestLcdBacklightVal = BACKLIGHT_ON;
          lcdBacklightTimeoutMillis = currentMillis + LCD_BACKLIGHT_TIMEOUT;
          systemState = setting;
        }
        break;
  }  

  if ((currentMillis > lcdBacklightTimeoutMillis) && (lcdBacklightTimeoutMillis != 0)) {
    requestLcdBacklightVal = BACKLIGHT_OFF;    
  }

  if (requestLcdBacklightVal != lcdBacklightVal) {
    digitalWrite(lcdBacklightPin, requestLcdBacklightVal);
    lcdBacklightVal = requestLcdBacklightVal;
  }
  
  //if (anythingChanged || prevSystemState != systemState || foeceDisplay) {
  display.display();
  //}
  
}

