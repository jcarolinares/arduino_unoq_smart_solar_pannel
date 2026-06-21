/*
UNO Q Smart Solar Pannel System
Created by Julián Caro Linares for Arduino and Qualcomm
CC-BY-SA

Updated to support independent minimum and maximum angles for 
the top and bottom servos, including dynamic matrix sizing.
*/

#include <Arduino_RouterBridge.h>
#include "Servo.h"

Servo servo_bottom;
Servo servo_top;

// Define the pin the servo's signal wire is connected to
const int servoPin_bottom = 5;
const int servoPin_top = 6;

// Define the Analog pin for the Solar Panel
const int solarPin = A0;

// Servo saved positions & Scanning Variables
const int idle_position = 90;

// Independent Bottom (Pan) Limits
const int bottom_min_position = 15;
const int bottom_max_position = 155;

// Independent Top (Tilt) Limits
const int top_min_position = 5; 
const int top_max_position = 90; 

const int scan_step_degrees = 5; // 5 degrees for High Resolution 
const int scan_delay_ms = 100;

// Calculate the required matrix sizes automatically based on independent limits
const int num_bottom_steps = ((bottom_max_position - bottom_min_position) / scan_step_degrees) + 1;
const int num_top_steps = ((top_max_position - top_min_position) / scan_step_degrees) + 1;

// 2D Array to store the sensor values (The Sky Heatmap)
// It is now a dynamically sized rectangle rather than a square
int scan_matrix[num_bottom_steps][num_top_steps];

// Global Memory for the last known best position
int last_best_bottom = idle_position;
int last_best_top = idle_position;

// Non-blocking Timer Variables
unsigned long previousScanTime = 0;
const unsigned long scanInterval = 60000; // 60,000 ms = 1 minute

// Alerts
bool weather_alert = false;

void setup() {
  // Bridge Initialization
  Serial.begin(9600);
  Bridge.begin();
  Bridge.provide("set_weather_alert", set_weather_alert);
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(solarPin, INPUT); // Initialize the analog pin
  
  // Servo motors Init
  servo_bottom.attach(servoPin_bottom);
  servo_top.attach(servoPin_top);
  
  // Move to idle initially
  servo_bottom.write(idle_position);
  servo_top.write(idle_position);
  delay(5000);

    // Move to idle initially
  servo_bottom.write(bottom_min_position);
  delay(5000);

  servo_bottom.write(bottom_max_position);
  delay(5000);

   // Move to idle initially
  servo_top.write(top_min_position);
  delay(5000);

  servo_top.write(top_max_position);
  delay(5000);
}

void loop() {
  // Read the current solar panel analog value (0 - 1023)
  int solarValue = analogRead(solarPin);
  // Serial.print("Solar value: ");
  // Serial.println(solarValue);

  
  if (weather_alert == true){
    // Fold to safe limits during bad weather
    servo_bottom.write(bottom_min_position);
    servo_top.write(top_min_position);
  } 
  // If the value is very low, it's likely night time. 
  else if (solarValue < 100) { 
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
//          on the solar panel output, and points the panel at it.
// ---------------------------------------------------------
void scanForMaxValue(int stepSize, int stepDelay) {
  int maxValueFound = -1;
  int bestBottomPos = idle_position;
  int bestTopPos = idle_position;
  
  bool goingUp = true; 

  // Outer loop uses independent Bottom limits
  for (int b = bottom_min_position; b <= bottom_max_position; b += stepSize) {
    servo_bottom.write(b);
    
    // Calculate the array X index for the bottom servo
    int b_idx = (b - bottom_min_position) / stepSize;
    
    if (goingUp) {
      // Inner loop uses independent Top limits (Going UP)
      for (int t = top_min_position; t <= top_max_position; t += stepSize) {
        servo_top.write(t);
        delay(stepDelay);
        
        int currentValue = analogRead(solarPin);
        
        // Calculate the array Y index for the top servo and save it
        int t_idx = (t - top_min_position) / stepSize;
        scan_matrix[b_idx][t_idx] = currentValue;
        
        if (currentValue > maxValueFound) {
          maxValueFound = currentValue;
          bestBottomPos = b;
          bestTopPos = t;
        }
      }
    } else {
      // Inner loop uses independent Top limits (Going DOWN)
      for (int t = top_max_position; t >= top_min_position; t -= stepSize) {
        servo_top.write(t);
        delay(stepDelay);
        
        int currentValue = analogRead(solarPin);
        
        // Calculate the array Y index for the top servo and save it
        int t_idx = (t - top_min_position) / stepSize;
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
  Serial.println("=================================");
  Serial.println("TARGET ACQUIRED (Source: Solar Panel A0)");
  Serial.print("Peak Value: ");
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
  Serial.println("--- Sky Heatmap [Data Source: SOLAR PANEL (A0)] ---");
  
  // Print Top Servo Angles (Columns)
  Serial.print("B\\T\t");
  for (int t = top_min_position; t <= top_max_position; t += scan_step_degrees) {
    Serial.print(t);
    Serial.print("\t");
  }
  Serial.println();

  // Print Bottom Servo Angles (Rows) and Matrix Data
  for (int b_idx = 0; b_idx < num_bottom_steps; b_idx++) {
    // Print the Bottom Angle label
    int b_angle = bottom_min_position + (b_idx * scan_step_degrees);
    Serial.print(b_angle);
    Serial.print("\t");

    // Print the sensor values for this row
    for (int t_idx = 0; t_idx < num_top_steps; t_idx++) {
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