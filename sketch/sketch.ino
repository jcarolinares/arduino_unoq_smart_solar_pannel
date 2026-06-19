/*
UNO Q Smart Solar Pannel System
Created by Julián Caro Linares for Arduino and Qualcomm
CC-BY-SA
*/

#include <Arduino_RouterBridge.h>
#include <Arduino_Modulino.h>
#include "Servo.h"

Servo servo_bottom;
Servo servo_top;

ModulinoLight light;

// Define the pin the servo's signal wire is connected to
const int servoPin_bottom = 5;
const int servoPin_top = 6;

// Servo saved positions & Scanning Variables
const int idle_position = 90;
const int min_position = 45;
const int max_position = 135;
const int scan_step_degrees = 10; // 5 degrees for High Resolution 
const int scan_delay_ms = 100;

// Calculate the required matrix size automatically
const int num_steps = ((max_position - min_position) / scan_step_degrees) + 1;

// 2D Array to store the sensor values (The Sky Heatmap)
int scan_matrix[num_steps][num_steps];

// Global Memory for the last known best position
int last_best_bottom = idle_position;
int last_best_top = idle_position;

// Non-blocking Timer Variables
unsigned long previousScanTime = 0;
const unsigned long scanInterval = 60000; // 60,000 ms = 1 minute

// Modulino Light Variables
int lux = 0;
int luxCalibrated = 0;
int ir = 0;

// Sensor Mode Toggle
// false = Raw Ambient Light (AL), true = Infrared (IR)
bool use_infrared = false; 

// Alerts
bool weather_alert = false;

void setup() {
  // Bridge Initialization
  Serial.begin(9600);
  Bridge.begin();
  Bridge.provide("set_weather_alert", set_weather_alert);
  Bridge.provide("set_infrared_mode", set_infrared_mode); // New bridge command
  
  // Modulino Init
  Modulino.begin();
  light.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  
  // Servo motors Init
  servo_bottom.attach(servoPin_bottom);
  servo_top.attach(servoPin_top);
  
  // Move to idle initially
  servo_bottom.write(idle_position);
  servo_top.write(idle_position);
  delay(500); 
}

void loop() {
  // Update sensor readings continuously
  light.update();

  lux = light.getAL();              // Ambient light (raw)
  luxCalibrated = light.getLux();   // Calibrated lux for the Human Eye. High green predominance
  ir = light.getIR();               // Infrared level

  if (weather_alert == true){
    servo_bottom.write(min_position);
    servo_top.write(min_position);
  } 
  else if (luxCalibrated < 100) {
    servo_bottom.write(idle_position);
    servo_top.write(idle_position);
  }
  else {
    unsigned long currentMillis = millis();
    
    if (currentMillis - previousScanTime >= scanInterval || previousScanTime == 0) {
      Serial.println("Starting Sky Scan...");
      scanForMaxValue(scan_step_degrees, scan_delay_ms);
      
      previousScanTime = millis(); 
    }
  }
  
  delay(50); 
}

// ---------------------------------------------------------
// Function: scanForMaxValue
// Purpose: Scans the sky, saves the map to scan_matrix based
//          on the selected sensor, and points the panel at it.
// ---------------------------------------------------------
void scanForMaxValue(int stepSize, int stepDelay) {
  int maxValueFound = -1;
  int bestBottomPos = idle_position;
  int bestTopPos = idle_position;
  
  bool goingUp = true; 

  for (int b = min_position; b <= max_position; b += stepSize) {
    servo_bottom.write(b);
    
    // Calculate the array X index for the bottom servo
    int b_idx = (b - min_position) / stepSize;
    
    if (goingUp) {
      for (int t = min_position; t <= max_position; t += stepSize) {
        servo_top.write(t);
        delay(stepDelay);
        
        light.update();
        
        // Check which sensor data to use based on the toggle
        int currentValue = use_infrared ? light.getIR() : light.getAL();
        
        // Calculate the array Y index for the top servo and save it
        int t_idx = (t - min_position) / stepSize;
        scan_matrix[b_idx][t_idx] = currentValue;
        
        if (currentValue > maxValueFound) {
          maxValueFound = currentValue;
          bestBottomPos = b;
          bestTopPos = t;
        }
      }
    } else {
      for (int t = max_position; t >= min_position; t -= stepSize) {
        servo_top.write(t);
        delay(stepDelay);
        
        light.update();
        
        // Check which sensor data to use based on the toggle
        int currentValue = use_infrared ? light.getIR() : light.getAL();
        
        // Calculate the array Y index for the top servo and save it
        int t_idx = (t - min_position) / stepSize;
        scan_matrix[b_idx][t_idx] = currentValue;
        
        if (currentValue > maxValueFound) {
          maxValueFound = currentValue;
          bestBottomPos = b;
          bestTopPos = t;
        }
      }
    }
    
    goingUp = !goingUp; 
  }

  // Save the newly found best positions to the GLOBAL variables
  last_best_bottom = bestBottomPos;
  last_best_top = bestTopPos;

  // Scan complete! Move to the best position.
  servo_bottom.write(bestBottomPos);
  servo_top.write(bestTopPos);

  // Print the final matrix to the Serial Monitor
  printScanMatrix();

  // Print the final chosen coordinates and Sensor Value
  String sourceName = use_infrared ? "Infrared (IR)" : "Raw Light (AL)";
  
  Serial.println("=================================");
  Serial.print("TARGET ACQUIRED (Source: ");
  Serial.print(sourceName);
  Serial.println(")");
  Serial.print("Value: ");
  Serial.print(maxValueFound);
  Serial.print(" at (Bottom: ");
  Serial.print(bestBottomPos);
  Serial.print("°, Top: ");
  Serial.print(bestTopPos);
  Serial.println("°)");
  Serial.println("=================================");
}

// ---------------------------------------------------------
// Function: printScanMatrix
// Purpose: Outputs the saved 2D array to the Serial Monitor
// ---------------------------------------------------------
void printScanMatrix() {
  String sourceName = use_infrared ? "INFRARED" : "RAW LIGHT";
  
  Serial.print("--- Sky Heatmap [Data Source: ");
  Serial.print(sourceName);
  Serial.println("] ---");
  
  // Print Top Servo Angles (Columns)
  Serial.print("B\\T\t");
  for (int t = min_position; t <= max_position; t += scan_step_degrees) {
    Serial.print(t);
    Serial.print("\t");
  }
  Serial.println();

  // Print Bottom Servo Angles (Rows) and Matrix Data
  for (int b_idx = 0; b_idx < num_steps; b_idx++) {
    // Print the Bottom Angle label
    int b_angle = min_position + (b_idx * scan_step_degrees);
    Serial.print(b_angle);
    Serial.print("\t");

    // Print the sensor values for this row
    for (int t_idx = 0; t_idx < num_steps; t_idx++) {
      Serial.print(scan_matrix[b_idx][t_idx]);
      Serial.print("\t");
    }
    Serial.println();
  }
  Serial.println("-----------------------");
}

// App Lab Command to set the weather alert
void set_weather_alert(bool state) {
  if (state == true){
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

// App Lab Command to switch the sensor reading mode
void set_infrared_mode(bool state) {
  use_infrared = state;
  if (use_infrared == true){
    Serial.println("Sensor mode changed to: INFRARED");    
  }
  else{
    Serial.println("Sensor mode changed to: RAW LIGHT");    
  }
}