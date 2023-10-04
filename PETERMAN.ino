// Grant Wang 2023
// Nexteer Competition
// P.E.T.E.R.M.A.N Project

// Libraries
#include <Wire.h>
#include <LiquidCrystal.h>
#include <IRremote.h>
#include <SevSeg.h>
#include <Stepper.h>
#include <EEPROM.h>

const int BUTTON_PIN = 1;  // Push Button Pin
int buttonState = 0;       // Push Button State (0/1)

const int RECV_PIN = 2;  // Infrared Receiver Pin

SevSeg sevseg;                              // Single Seven Segment Display Object
LiquidCrystal lcd(A5, A4, A3, A2, A1, A0);  // LCD Object (rs, en, d4, d5, d6, d7)

const int STEPS_PER_REVOLUTION = 2048;                     // Steps/Revolution of Stepper Motor
const double STEPS_PER_DIGIT = STEPS_PER_REVOLUTION / 20;  // Steps/Digit on the Safe
Stepper myStepper(STEPS_PER_REVOLUTION, 5, 3, 4, 2);       // Stepper Motor Object (1N1, 1N3, 1N2, 1N4)

int secondOffset = 0;  // Account for Time Spent on Lock 1
int movement = 2;      // Direction of the Lock 4

void setup() {
  Wire.begin();  // join i2c bus

  lcd.begin(16, 2);         // Set up LCD's number of columns and rows
  lcd.print("Grant 2023");  // Print name to the LCD

  // Set cursor to the column 0, line 1
  lcd.setCursor(0, 1);

  pinMode(BUTTON_PIN, INPUT);  // Button is INPUT

  IrReceiver.begin(RECV_PIN);  // Start IR Receiver

  // Seven Segment Display Setup
  byte numDigits = 1;  // One Digit Display
  byte digitPins[] = {};
  byte segmentPins[] = { 12, 13, 8, 7, 6, 11, 10, 9 };  // Segment Pins
  bool resistorsOnSegments = true;
  byte hardwareConfig = COMMON_CATHODE;

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments);
  sevseg.setBrightness(90);

  // Lock 1
  sevseg.setNumber(getActiveLock());  // Set Seven Segment Display to Current Lock
  EEPROM.write(0, getTime(0x01));     // Write Hour to EEPROM
  EEPROM.write(1, getTime(0x02));     // Write Minute to EEPROM
  EEPROM.write(2, getTime(0x03));     // Write Second to EEPROM
  setTimeLock(8, 0, 0);               // Set Safe Time to 8:00:00
  lockVerification();                 // Verify Lock

  secondOffset = millis() / 1000;  // Offset System Millis to Get Time From End of Lock 1
}

void loop() {
  updateLCDTime();  // Update Seconds on LCD

  // Set Seven Segment Display to Current Lock Number
  int currentLock = getActiveLock();
  sevseg.setNumber(currentLock);
  sevseg.refreshDisplay();

  switch (currentLock) {
    case 2:  // Lock Two
      enterTechID();
      lockVerification();
      break;
    case 3:  // Lock Three
      seedKey();
      lockVerification();
      break;
    case 4:  // Lock Four
      combinationDial();
      lockVerification();
      break;
    case 5:  // Lock Five
      restoreTime();
      break;
  }
}

void updateLCDTime() {
  // Set cursor to the column 0, line 1
  lcd.setCursor(0, 1);
  // Print number of seconds after SetTime in Lock 1
  lcd.print(millis() / 1000 - secondOffset);
}

int getActiveLock() {
  int activeLock;                                // Active Lock Number
  byte getLockPayload[] = { 0x01, 0x01, 0x01 };  // Command Group ID, Read, Command Group

  Wire.beginTransmission(0x10);                        // Start Transmission
  Wire.write(getLockPayload, sizeof(getLockPayload));  // Start Command
  Wire.requestFrom(0x10, 1);                           // Request Lock Number from Safe
  while (Wire.available() > 0) {                       // While Data Available
    activeLock = Wire.read();                          // Intake Lock Number
  }
  Wire.endTransmission();  // End Transmission

  delay(100);  // Delay 100ms

  return activeLock;  // Return Active Lock
}

void lockVerification() {
  byte lockVerif[] = { 0x01, 0x02, 0x01 };   // Command Group ID, Write, Command ID
  Wire.beginTransmission(0x10);              // Start Transmission
  Wire.write(lockVerif, sizeof(lockVerif));  // Command Start
  Wire.endTransmission();                    // End Transmission

  delay(100);
}

// Lock 1/5
byte getTime(byte timeUnit) {
  byte time;  // Time Unit

  // Get Hour
  byte getTime[] = { 0x10, 0x01, timeUnit };  // Command Group ID, Read, Command ID
  Wire.beginTransmission(0x10);               // Start Transmission
  Wire.write(getTime, sizeof(getTime));       // Command Start
  Wire.requestFrom(0x10, 1);                  // Request Time from Safe
  while (Wire.available() > 0) {              // Read in Time while Communication is Available
    time = Wire.read();
  }
  Wire.endTransmission();  // End Transmission

  delay(100);  // Delay 100ms

  return time;  // Return time
}

void setTimeLock(byte hour, byte minute, byte second) {
  byte setTime[] = { 0x10, 0x01, 0x03, hour, minute, second };  // Command Group ID, Read, Command ID, Hour, Minute, Second
  Wire.beginTransmission(0x10);                                 // Start Transmission
  Wire.write(setTime, sizeof(setTime));                         // Command Start
  Wire.endTransmission();                                       // End Transmission

  delay(100);  // Delay 100ms
}

// Lock 2
void enterTechID() {
  int index = 0;                              // Index of techID
  byte temp = 0;                              // Duplicate input detection
  byte techID[6];                             // Store ID for Export
  byte techIDStart[] = { 0x20, 0x02, 0x01 };  // Command Group ID, Write, Command ID

  Wire.beginTransmission(0x10);                  // Begin Transmission
  Wire.write(techIDStart, sizeof(techIDStart));  // Command Start
  while (index < 6) {

    //Given that we've exited the loop() and entered a while loop(), continue to allow the lcd to update while waiting for this lock to complete
    updateLCDTime();

    if (IrReceiver.decode()) {  // If IrReceiver receives Input
      IrReceiver.resume();

      if (IrReceiver.decodedIRData.command != 0 && IrReceiver.decodedIRData.command != temp) {  // Given that the code has no consecutive digits,
        temp = IrReceiver.decodedIRData.command;                                                // Store Pre-Deciphered Input to Prevent Consecutive Input

        // Decipher Remote Control Input
        switch (techID[index]) {
          case 22:
            techID[index] = 0;
            break;
          case 12:
            techID[index] = 1;
            break;
          case 24:
            techID[index] = 2;
            break;
          case 94:
            techID[index] = 3;
            break;
          case 8:
            techID[index] = 4;
            break;
          case 28:
            techID[index] = 5;
            break;
          case 90:
            techID[index] = 6;
            break;
          case 66:
            techID[index] = 7;
            break;
          case 82:
            techID[index] = 8;
            break;
          case 74:
            techID[index] = 9;
            break;
        }
        index++;  // One Digit down
      }
    }
  }
  Wire.write(techID, sizeof(techID));  // Write Inputted TechID to Safe
  Wire.endTransmission();              // End Transmission

  delay(100);  // Delay 100ms
}

// Lock 3
void seedKey() {
  byte seedKeyStart[] = { 0x30, 0x01, 0x01 };  // Command Group ID, Read, Command ID

  byte seeds[2];  // Randomly Generated Seed
  int index = 0;  // Index of Seed

  Wire.beginTransmission(0x10);                    // Begin Transmission
  Wire.write(seedKeyStart, sizeof(seedKeyStart));  // Command Start

  Wire.requestFrom(0x10, 2);      // Request Bytes
  while (Wire.available() > 0) {  // Feed Bytes into Seed Array
    seeds[index] = Wire.read();
    index++;
  }
  Wire.endTransmission();  // End Transmission

  delay(5000);  // Delay 100ms

  byte seed = (seeds[0] << 8) | seeds[1];           // Convert MSB and LSB to Seed
  byte key = (((seed * seed) ^ seed) << 4) ^ seed;  // Convert Seed to Key

  byte msb = (key >> 8) & 0xFF;  // Most Significant Byte
  byte lsb = key & 0xFF;         // Least Signifcant Byte

  byte sendKey[] = { 0x30, 0x02, 0x01, msb, lsb };  // Command Group ID, Write, Command ID, MSB, LSB

  Wire.beginTransmission(0x10);          // Begin Transmission
  Wire.write(sendKey, sizeof(sendKey));  // Write Key to Safe
  Wire.endTransmission();                // End Transmission
}

// Lock 4
void combinationDial() {
  byte getCombo[] = { 0x40, 0x01, 0x01 };  // Command Group ID, Read, Command ID
  byte combination[6];                     // Combination
  int index = 0;                           // Index of Combination

  Wire.beginTransmission(0x10);  // Start Transmission
  Wire.write(0x40);              // Command Start
  Wire.requestFrom(0x10, 6);     // Request 6 Digit Combination
  while (Wire.available() > 0) {
    combination[index] = Wire.read();
    index++;
  }
  Wire.endTransmission();
  index = 0;

  delay(100);  // Delay 100ms

  bool clockwise = false;  // Direction Checker

  myStepper.setSpeed(15);  // Good speed through testing

  while (index < 6) {

    updateLCDTime();  // Exited loop() into a while loop, continue to update LCD.

    int currentPosition = getCurrentCombinationDialPosition();
    int stepsToTravelCW = abs(currentPosition - combination[index]);
    int stepsToTravelCCW = abs(combination[index] - currentPosition);
    if (currentPosition == combination[index]) {
      while (buttonState != HIGH) {             // While Button Hasn't Been Pressed
        buttonState = digitalRead(BUTTON_PIN);  // Read in Button Input
      }
      setCombinationEntry();   // Set Combination Entry after Button Press
      clockwise = !clockwise;  // Invert Direction
      index++;
    } else {
      // Find shortest path for first digit
      if (index == 0) {
        if (movement == 2) {  // If There is No Known Travel Direction
          // Find the shortest path
          if (stepsToTravelCW >= stepsToTravelCCW) {  // Travel CounterClockWise
            myStepper.step(-STEPS_PER_DIGIT);
          } else {  // Travel ClockWise
            myStepper.step(STEPS_PER_DIGIT);
            clockwise = true;
          }
        } else {
          // Move According to Known Travel Direction
          if (movement < 0) {
            myStepper.step(-STEPS_PER_DIGIT);
          } else {
            myStepper.step(STEPS_PER_DIGIT);
            clockwise = true;
          }
        }
      }
      if (clockwise) {  // Move in Clockwise Direction
        myStepper.step(STEPS_PER_DIGIT);
      } else {
        myStepper.step(-STEPS_PER_DIGIT);  // Move in CounterClockwise Direction
      }
    }
  }
}

int getCurrentCombinationDialPosition() {
  int position[2];  // Store Digits
  int index = 0;    // Index

  byte getDialPosition[] = { 0x40, 0x01, 0x02 };         // Command Group ID, Read, Command ID
  Wire.beginTransmission(0x10);                          // Start Transmission
  Wire.write(getDialPosition, sizeof(getDialPosition));  // Command Start
  Wire.requestFrom(0x10, 3);                             // Request Current Dial Position
  while (Wire.available() > 0) {                         // If Communication Found
    char hexVal = Wire.read();                           // Read in ASCII Hex
    position[index] = int(hexVal);                       // Convert ASCII Hex to Decimal
    if (index == 1) {                                    // If Two Digits Have Been Read
      movement = Wire.read();                            // Read in Dial Direction
    }
    index++;  // Go down a digit
  }
  Wire.endTransmission();  // End Transmission

  int currentPosition = (position[0] - '0') * 10 + (position[1] - '0');  // Convert Two Digit ASCII Decimal to Decimal Number

  return currentPosition;  // Return Current Dial Position
}

void setCombinationEntry() {
  byte setCombEntry[] = { 0x40, 0x02, 0x01 };      // Command Group ID, Write, Command ID
  Wire.beginTransmission(0x10);                    // Start Transmission
  Wire.write(setCombEntry, sizeof(setCombEntry));  // Command Start
  Wire.endTransmission();                          // End Transmission
}

// Lock 5
void restoreTime() {
  // Set Time Lock to "Current" Time
  setTimeLock(EEPROM.read(0) + getTime(0x01) - 8, EEPROM.read(1) + getTime(0x02), EEPROM.read(2) + getTime(0x03));
}
