#include <Ticker.h>
/////////////////// build-in LED control ////////////////////////

// temporizadores LED build-in
Ticker LEDticker;
float timeON = 0.1;
float timeOFF = 0.1;

void blink(){
  // Toggle the LED state
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  // Reset the ticker interval
  LEDticker.attach(digitalRead(LED_BUILTIN) ? timeOFF : timeON, blink);
}

void setLEDTimeOnOff(float onTime, float offtime){
  timeON = onTime;
  timeOFF = offtime;
}

void setupLEDbuildin(){
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  LEDticker.attach(timeOFF, blink); 
}

void finishSetupLEDbuildin(){
  setLEDTimeOnOff(0.1, 3);
}