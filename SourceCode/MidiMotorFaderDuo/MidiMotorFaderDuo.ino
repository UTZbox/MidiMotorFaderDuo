/* -----------------------------------------------------------------------
 * Programmer:   Mike Utz
 * Date:         June 23, 2022
 * Platform:     Teensy LC or 3.x
 * USB Type:     “Serial + MIDI“
 * Clock:        48 MHz for LC / 72 MHz or 96 MHz for 3.2
 * Description:  A Dual MotorFader Interface for Controlling Q-Lab (Light Cues and Dashboard) via USB MIDI
 *               The first Fader is motorized and fixed to MIDI CC Control Nr. 0
 *               The second is also motorized and the desired Channel can be switched by buttons
 *               Two Buttons for increment/decrement the MIDI Control Nr. (Fader Nr.)
 *               One Button for Flash on selected Fader Nr.
 *               All Values From MIDI or Faders ar sent out to DMX (1 - 32)
 *               The Device is acting as USB MIDI Interface, MIDI Control Change at Channel 1, Control Nr. 0 - 31
 *               
 */

#include <MIDI.h>
#include <DmxSimple.h>
#include "LedControl.h"

//Pin Assignments
const byte motorDown1    = 6;    // H-Bridge control motor (fader) down
const byte motorUp1      = 7;    // H-Bridge control motor (fader) up
const byte motorPwm1     = 20;   // H-Bridge control enable PWM
const byte motorDown2    = 8;    // H-Bridge control motor (fader) down
const byte motorUp2      = 9;    // H-Bridge control motor (fader) up
const byte motorPwm2     = 21;   // H-Bridge control enable PWM
const byte flashIn       = 2;    // Input pin for flash button
const byte ccCtrlSelInc  = 3;    // Input pin for MiDi CC Control select increase
const byte ccCtrlSelDec  = 4;    // Input pin for MiDi CC Control  switch decrease
const byte dmxOut        = 23;    // Output Pin for DMX Signal
LedControl lc=LedControl(12,11,10,1); // Outputs for Display MX72XX - DIN, CLK, LOAD

//Inputs
const int fader1       = 0;   // Position of fader relative to GND (Analog 0)
const int fader2       = 1;    // Position of fader relative to GND (Analog 1) 
const int touchInput1  = 17;   // Receive pin for Capacitance Sensing Circuit
const int touchInput2  = 18;   // Receive pin for Capacitance Sensing Circuit

//Variables
int touchTresh        = 2500; // Treshhild Level touching the Fader
int touchValue1       = 0;
int touchValue2       = 0;
int faderMax1         = 0;   // Value read by fader's maximum position (0-4096)
int faderMin1         = 0;   // Value read by fader's minimum position (0-4096)
int faderMax2         = 0;   // Value read by fader's maximum position (0-4096)
int faderMin2         = 0;   // Value read by fader's minimum position (0-4096)
int faderPos1         = 0;
int faderPos2         = 0;
int lastFaderPos1     = 0;
int lastFaderPos2     = 0;
byte faderCcChan      = 1;  // MIDI Channel Nr. to listen to...
byte dmxChan          = 1;
byte ccChan           = 1;
byte ccCtrl           = 0;
byte ccVal            = 1;

byte LastReceivedCC[] ={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

volatile bool Flash         = 0;
volatile bool TouchOn1      = false; // Is the fader currently being touched?
volatile bool TouchOn2      = false; // Is the fader currently being touched?
volatile bool FlashOn       = false;
volatile byte CcSendVal1    = 0;
volatile byte CcSendVal2    = 0;
volatile byte DmxVal        = 0;
volatile byte Fader1CcCtrl  = 0;
volatile byte Fader2CcCtrl  = 1;
volatile int Setpoint1      = 0;
volatile int Setpoint2      = 0;
volatile int DisplayCcVal   = 0;
volatile int DisplayCcCtrl  = 0;
volatile int ValOnes        = 0;
volatile int ValTens        = 0;
volatile int ValHundreds    = 0;
volatile int CcOnes         = 0;
volatile int CcTens         = 0;


//Timers
elapsedMillis sinceLastMidiSend;
elapsedMillis sincelastTouch1;
elapsedMillis sincelastTouch2;
elapsedMillis sincelastPrint;
elapsedMillis sinceLastFlash;
elapsedMillis sinceLastCcSel;
elapsedMillis sinceLastDisplay;
elapsedMillis sinceLastDmxSend;

// startup routine
void setup() {
    pinMode (motorUp1, OUTPUT);
    pinMode (motorDown1, OUTPUT);
    pinMode (motorPwm1, OUTPUT);
    pinMode (motorUp2, OUTPUT);
    pinMode (motorDown2, OUTPUT);
    pinMode (motorPwm2, OUTPUT);
    pinMode (flashIn, INPUT_PULLUP);
    pinMode (ccCtrlSelInc, INPUT_PULLUP);
    pinMode (ccCtrlSelDec, INPUT_PULLUP);    
    analogReadResolution(12);

    usbMIDI.setHandleControlChange(OnControlChange);

    DmxSimple.usePin(dmxOut);
    DmxSimple.maxChannel(32);
    Serial.begin(9600);

    lc.shutdown(0,false);   // Wake Up Max72XX
    lc.setIntensity(0,5);   // Set the brightness to a medium values
    lc.clearDisplay(0);   // and clear the display

    analogWrite(motorPwm1, 255);  // Set PWM to Max
    analogWrite(motorPwm2, 255);  // Set PWM to Max
    calibrateFader(); 
}

void loop() {
    usbMIDI.read();
    Flash = !digitalRead(flashIn);
    faderPos1 = analogRead(fader1);
    faderPos2 = analogRead(fader2);
    
    checkTouch();                    // Check to see if the fader is touched
    checkFlash();                    // Handle the behavior when the flash button is pressed
    updateCCtrlNum();                // Check the selected MiDi Cc Coonrtol Number
    
    if (ccChan == faderCcChan && sincelastTouch1 > 500){
      Setpoint1 = int(LastReceivedCC[Fader1CcCtrl] * 32);          // Read the Setpoint of MIDI ControlChange Value for Fader 1
    }
    if (ccChan == faderCcChan && sincelastTouch2 > 500){
      Setpoint2 = int(LastReceivedCC[Fader2CcCtrl] * 32);          // Read the Setpoint of MIDI ControlChange Value for Fader 2
    }


    if(FlashOn == 1){
      Setpoint2 = faderMax2 -16;        // Set the Setpoint to the Fader Max. Position if the Flash Button is pressed
    }

    // Move the Faders if the Setpoint differs to the Actual Positions
    if (Setpoint1 < analogRead(fader1) - 64 && faderPos1 > faderMin1 && !TouchOn1) {
        digitalWrite(motorDown1, HIGH);
        while (Setpoint1 < analogRead(fader1) - 32 && !TouchOn1) {
          if (analogRead(fader1) > Setpoint1 && analogRead(fader1) < Setpoint1 + 256){
            analogWrite(motorPwm1, 168);
          }
          };  //Loops until motor is done moving
        digitalWrite(motorDown1, LOW);
        analogWrite(motorPwm1, 255);
        
    }
    else if (Setpoint1 > analogRead(fader1) + 64 && faderPos1 < faderMax1 && !TouchOn1) {
        digitalWrite(motorUp1, HIGH);
        while (Setpoint1 > analogRead(fader1) + 32 && !TouchOn1) {
          if (analogRead(fader1) < Setpoint1 && analogRead(fader1) > Setpoint1 - 256){
            analogWrite(motorPwm1, 168);
          }
          }; //Loops until motor is done moving
        digitalWrite(motorUp1, LOW);
        analogWrite(motorPwm1, 255);
    }

    if (Setpoint2 < analogRead(fader2) - 64 && faderPos2 > faderMin2 && !TouchOn2) {
        digitalWrite(motorDown2, HIGH);
        while (Setpoint2 < analogRead(fader2) - 32 && !TouchOn2) {
          if (analogRead(fader2) > Setpoint2 && analogRead(fader2) < Setpoint2 + 256){
            analogWrite(motorPwm2, 168);
          }
          };  //Loops until motor is done moving
        digitalWrite(motorDown2, LOW);
        analogWrite(motorPwm2, 255);
        
    }
    else if (Setpoint2 > analogRead(fader2) + 64 && faderPos2 < faderMax2 && !TouchOn2) {
        digitalWrite(motorUp2, HIGH);
        while (Setpoint2 > analogRead(fader2) + 32 && !TouchOn2) {
          if (analogRead(fader2) < Setpoint2 && analogRead(fader2) > Setpoint2 - 256){
            analogWrite(motorPwm2, 168);
          }
          }; //Loops until motor is done moving
        digitalWrite(motorUp2, LOW);
        analogWrite(motorPwm2, 255);
    }


    // When the Faders are touched, read the position and send the Value out to MIDI
    if (TouchOn1 && sinceLastMidiSend > 50){
      CcSendVal1 = faderPos1 / 32;
      usbMIDI.sendControlChange(Fader1CcCtrl, CcSendVal1, faderCcChan);  // Send Out the MIDI Value which correspond to the Faders Position
      LastReceivedCC[Fader1CcCtrl] = CcSendVal1;
      sinceLastMidiSend = 0;
    }
    
    if (TouchOn2 && sinceLastMidiSend > 50){
      CcSendVal2 = faderPos2 / 32;
      usbMIDI.sendControlChange(Fader2CcCtrl, CcSendVal2, faderCcChan);  // Send Out the MIDI Value which correspond to the Faders Position
      LastReceivedCC[Fader2CcCtrl] = CcSendVal2;
      sinceLastMidiSend = 0;
    }
    
    
    DmxSend();
    DisplayUpdate();
   
    /* Debug
    if (sincelastPrint > 500){
    Serial.print("faderPos1      ");
    Serial.println(faderPos1);
    Serial.print("Setpoint1      ");
    Serial.println(Setpoint1);
    Serial.print("faderPos2      ");
    Serial.println(faderPos2);
    Serial.print("Setpoint2      ");
    Serial.println(Setpoint2);
    Serial.print("faderMinMax1   ");
    Serial.print(faderMin1);
    Serial.print(" ");
    Serial.println(faderMax1);
    Serial.print("faderMinMax2   ");
    Serial.print(faderMin2);  
    Serial.print(" "); 
    Serial.println(faderMax2);   
    Serial.print("ccVal          ");
    Serial.println(ccVal);
    Serial.print("CcSendVal1     ");
    Serial.println(CcSendVal1);
    Serial.print("CcSendVal2     ");
    Serial.println(CcSendVal2);
    Serial.print("LastReceivedCc ");
    Serial.print(LastReceivedCC[0]);
    Serial.print(" ");
    Serial.print(LastReceivedCC[1]);
    Serial.print(" ");
    Serial.println(LastReceivedCC[2]);
    Serial.print("Fader1CcCtrl   ");
    Serial.println(Fader1CcCtrl);
    Serial.print("Fader2CcCtrl   ");
    Serial.println(Fader2CcCtrl);
    Serial.print("ccCtrl         ");
    Serial.println(ccCtrl);
    Serial.print("DMX Chan       ");
    Serial.println(dmxChan);
    Serial.print("DMX Val        ");
    Serial.println(DmxVal);
    Serial.print("touchLevel1    ");
    Serial.println(touchValue1);
    Serial.print("touchLevel2    ");
    Serial.println(touchValue2);
    Serial.print("Flash          ");
    Serial.println(Flash);
    sincelastPrint = 0;
    } */
}


    
/* Functions ---------------------*/

//! Called when we recieve a ControlChange event
void OnControlChange(byte channel, byte control, byte value) {
  // Store the incoming values to global variables
  ccChan = channel;
  ccCtrl = control;
  ccVal = value;
  LastReceivedCC[ccCtrl] = ccVal;
}

//Calibrates the min and max position of the fader
void calibrateFader() {
    //Move Fader1 to the top and read max position
    digitalWrite(motorUp1, HIGH);
    delay(250);
    digitalWrite(motorUp1, LOW);
    faderMax1 = analogRead(fader1);

    //Move Fader1 to the bottom and read max position
    digitalWrite(motorDown1, HIGH);
    delay(250);
    digitalWrite(motorDown1, LOW);
    faderMin1 = analogRead(fader1);
    
    //Move Fader2 to the top and read max position
    digitalWrite(motorUp2, HIGH);
    delay(250);
    digitalWrite(motorUp2, LOW);
    faderMax2 = analogRead(fader2);

    //Move Fader to the bottom and read max position
    digitalWrite(motorDown2, HIGH);
    delay(250);
    digitalWrite(motorDown2, LOW);
    faderMin2 = analogRead(fader2);
}

//Check to see if the fader is being touched
void checkTouch() {
    touchValue1 = touchRead(touchInput1);
    if (touchValue1 > touchTresh && TouchOn1 == 0){
      TouchOn1 = 1;
    }
    if (touchValue1 < touchTresh && TouchOn1 == 1){
      Setpoint1 = faderPos1; 
      TouchOn1 = 0;
      sincelastTouch1 = 0;
    }
    
    touchValue2 = touchRead(touchInput2);
    if (touchValue2 > touchTresh && TouchOn2 == 0){
      TouchOn2 = 1;
    }
    if (touchValue2 < touchTresh && TouchOn2 == 1){
      Setpoint2 = faderPos2; 
      TouchOn2 = 0;
      sincelastTouch2 = 0;
    }
    
}

// Handle the behavior when the flash button is pressed
void checkFlash() {
    if (Flash == 1 && FlashOn == 0 && sinceLastFlash > 100){
      lastFaderPos2 = faderPos2;
      CcSendVal2 = 127;
      usbMIDI.sendControlChange(Fader2CcCtrl, CcSendVal2, faderCcChan);
      FlashOn = 1;
    }
    if (Flash == 0 && FlashOn == 1){
      faderPos2 = lastFaderPos2;
      CcSendVal2 = faderPos2 / 32;
      usbMIDI.sendControlChange(Fader2CcCtrl, CcSendVal2, faderCcChan);
      Setpoint2 = faderPos2;
      FlashOn = 0;
      sinceLastFlash = 0;
    }
}

// Read the INC / DEC Button inputs and change Value
void updateCCtrlNum() {
  if (!digitalRead(ccCtrlSelInc) && sinceLastCcSel > 250){
      Fader2CcCtrl = Fader2CcCtrl + 1;
        if (Fader2CcCtrl > 31) {
          Fader2CcCtrl = 31;
          }
      Setpoint2 = int(LastReceivedCC[Fader2CcCtrl] * 32);
      sinceLastCcSel = 0;
      }

  if (!digitalRead(ccCtrlSelDec)&& sinceLastCcSel > 250){
      Fader2CcCtrl = Fader2CcCtrl - 1;
        if (Fader2CcCtrl > 31) {
          Fader2CcCtrl = 0;
          }
      Setpoint2 = int(LastReceivedCC[Fader2CcCtrl] * 32);
      sinceLastCcSel = 0;
  }
}

// Read the FaderPos if touched or Midi Value and send it out to DMX
void DmxSend() {
  if (TouchOn1 == 1){
    DmxVal = faderPos1 / 16;
    dmxChan = Fader1CcCtrl + 1;
  }
  
  if (TouchOn2 == 1){
    DmxVal = faderPos2 / 16;
    dmxChan = Fader2CcCtrl + 1;
  }

  else{
    DmxVal = ccVal * 2;
    dmxChan = ccCtrl + 1;
    }
    
  if (sinceLastDmxSend > 40){
    DmxSimple.write(dmxChan, DmxVal);
    sinceLastDmxSend = 0;
  }
  
}

// Calculate the Digits and write it to the Display (MAX72XX)
void DisplayUpdate() {
  if (sinceLastDisplay > 500);{
    DisplayCcCtrl = Fader2CcCtrl;
    CcOnes = DisplayCcCtrl % 10;
    DisplayCcCtrl = DisplayCcCtrl / 10;
    CcTens = DisplayCcCtrl % 10;
    DisplayCcVal = map(LastReceivedCC[Fader2CcCtrl],0,127,0,100);
    ValOnes = DisplayCcVal % 10;
    DisplayCcVal = DisplayCcVal / 10;
    ValTens = DisplayCcVal % 10;
    DisplayCcVal = DisplayCcVal / 10;
    ValHundreds = DisplayCcVal;
    lc.setChar(0,7,'c',false);
    lc.setChar(0,6,'c',false);
    lc.setDigit(0,5,(byte)CcTens, false);
    lc.setDigit(0,4,(byte)CcOnes, false);
    lc.setChar(0,3,'-',false);
    lc.setDigit(0,2,(byte)ValHundreds, false);
    lc.setDigit(0,1,(byte)ValTens, false);
    lc.setDigit(0,0,(byte)ValOnes, false);  
    sinceLastDisplay = 0;
  }
}
