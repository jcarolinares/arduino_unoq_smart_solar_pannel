/*

UNO Q Smart Solar Pannel System

Created by Julián Caro Linares for Arduino and Qualcomm

CC-BY-SA

*/

#include <Arduino_RouterBridge.h>
#include <Arduino_Modulino.h>
#include "Servo.h"
// #include <Modulino.h>


Servo servo_bottom;
Servo servo_top;

ModulinoLight light;

// Define the pin the servo's signal wire is connected to
const int servoPin_bottom = 5;
const int servoPin_top = 6;

// Servo saved positions
const int idle_position = 90;
const int min_position = 45;
const int max_position = 135;


// Modulino Light Variables
int lux = 0;
int luxCalibrated = 0;
int ir = 0;


// Alerts
bool weather_alert = false;

void setup() {

    // Bridge Initialization
  // Serial.begin(115200);
  Serial.begin(9600);
  Bridge.begin();
  Bridge.provide("set_weather_alert", set_weather_alert);
  
  // Modulino Init
  Modulino.begin();
  light.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  
  // Servo motors Init
  servo_bottom.attach(servoPin_bottom);
  servo_top.attach(servoPin_top);
  servo_bottom.write(idle_position);
  servo_top.write(idle_position);
}

void loop() {

  // Update sensor readings
  light.update();

  lux = light.getAL();              // Ambient light (raw)
  luxCalibrated = light.getLux();   // Calibrated lux
  ir = light.getIR();               // Infrared level

  // Modulino Light Debugging
  // Serial.print("Lux :");
  // Serial.println(lux);

  // Serial.print("Lux Calibrated :");
  // Serial.println(luxCalibrated);

  // Serial.print("Infrared Lux :");
  // Serial.println(ir);
  // Serial.println();


  if (lux < 100 || weather_alert == true){
    servo_bottom.write(min_position);
    servo_top.write(min_position);
    delay(100);
  }
  else{
    servo_bottom.write(idle_position);
    servo_top.write(idle_position);
    delay(100);
  }
  
  // for (int j=idle_position; j<max_position; j++){
  //   servo_bottom.write(j);
  //   servo_top.write(j);
  //   delay(100);
  // }
  delay(1000);
}

void set_weather_alert(bool state) {
    
  if (state == true){
    // LOW state means LED is ON
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Weather Alert Detected");    
    weather_alert = true;
  }
  else{
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Weather Alert Finished");    
    weather_alert = false;
  }
}