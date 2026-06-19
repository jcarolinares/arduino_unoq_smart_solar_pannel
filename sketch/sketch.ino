/*
UNO Q Smart Solar Pannel System
Created by Julián Caro Linares for Arduino and Qualcomm
CC-BY-SA

Updated to include Temporal Gradient calculation and predictive 
tracking for the next 5 sun positions.
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
const int min_position = 25;
const int max_position = 155;
const int scan_step_degrees = 10; 
const int scan_delay_ms = 100;

// Calculate the required matrix size automatically
const int num_steps = ((max_position - min_position) / scan_step_degrees) + 1;

// 2D Array to store the sensor values (The Sky Heatmap)
int scan_matrix[num_steps][num_steps];

// Global Memory for tracking movement over time
int last_best_bottom = idle_position;
int last_best_top = idle_position;
int previous_best_bottom = idle_position;
int previous_best_top = idle_position;

// Array to store the next 5 predicted positions
// [5 rows for steps][2 columns for Bottom/Top angles]
int predicted_positions[5][2];

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

// Keeps track of whether we have enough data to predict
bool has_scanned_twice = false; 

void setup() {
  // Bridge Initialization
  Serial.begin(9600);
  Bridge.begin();
  Bridge.provide("set_weather_alert", set_weather_alert);
  Bridge.provide("set_infrared_mode", set_infrared_mode);
  
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

  lux = light.getAL();              
  luxCalibrated = light.getLux();   
  ir = light.getIR();               

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

    Serial.println(analogRead(A0));
  }
  
  delay(50); 
}

// ---------------------------------------------------------
// Function: scanForMaxValue
// ---------------------------------------------------------
void scanForMaxValue(int stepSize, int stepDelay) {
  int maxValueFound = -1;
  int bestBottomPos = idle_position;
  int bestTopPos = idle_position;
  
  bool goingUp = true; 

  for (int b = min_position; b <= max_position; b += stepSize) {
    servo_bottom.write(b);
    int b_idx = (b - min_position) / stepSize;
    
    if (goingUp) {
      for (int t = min_position; t <= max_position; t += stepSize) {
        servo_top.write(t);
        delay(stepDelay);
        light.update();
        int currentValue = use_infrared ? light.getIR() : light.getAL();
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
        int currentValue = use_infrared ? light.getIR() : light.getAL();
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

  // --- TEMPORAL GRADIENT & PREDICTION LOGIC ---
  
  // 1. Shift memory back
  previous_best_bottom = last_best_bottom;
  previous_best_top = last_best_top;
  
  // 2. Save new current position
  last_best_bottom = bestBottomPos;
  last_best_top = bestTopPos;

  // 3. Calculate Gradient Vector (Delta)
  int delta_bottom = last_best_bottom - previous_best_bottom;
  int delta_top = last_best_top - previous_best_top;

  // 4. Calculate the next 5 positions
  for (int i = 0; i < 5; i++) {
    int step_multiplier = i + 1;
    predicted_positions[i][0] = last_best_bottom + (delta_bottom * step_multiplier);
    predicted_positions[i][1] = last_best_top + (delta_top * step_multiplier);
  }

  // Move servos to the best position
  servo_bottom.write(bestBottomPos);
  servo_top.write(bestTopPos);

  // Print Heatmap
  printScanMatrix();

  // Print Results
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

  // Print Predictions
  if (has_scanned_twice) {
    Serial.println("---------------------------------");
    Serial.print("TEMPORAL GRADIENT VECTOR: [Δ Bottom: ");
    Serial.print(delta_bottom);
    Serial.print("°, Δ Top: ");
    Serial.print(delta_top);
    Serial.println("°]");
    
    Serial.println("NEXT 5 PREDICTED POSITIONS:");
    for (int i = 0; i < 5; i++) {
      Serial.print(" +");
      Serial.print(i + 1);
      Serial.print(" min -> Bottom: ");
      Serial.print(predicted_positions[i][0]);
      Serial.print("°, Top: ");
      Serial.print(predicted_positions[i][1]);
      Serial.println("°");
    }
  } else {
    Serial.println("---------------------------------");
    Serial.println("Waiting for next scan to establish temporal gradient...");
    has_scanned_twice = true; // Set to true so predictions run next time
  }
  Serial.println("=================================");
}

// ---------------------------------------------------------
// Function: printScanMatrix
// ---------------------------------------------------------
void printScanMatrix() {
  String sourceName = use_infrared ? "INFRARED" : "RAW LIGHT";
  
  Serial.print("--- Sky Heatmap [Data Source: ");
  Serial.print(sourceName);
  Serial.println("] ---");
  
  Serial.print("B\\T\t");
  for (int t = min_position; t <= max_position; t += scan_step_degrees) {
    Serial.print(t);
    Serial.print("\t");
  }
  Serial.println();

  for (int b_idx = 0; b_idx < num_steps; b_idx++) {
    int b_angle = min_position + (b_idx * scan_step_degrees);
    Serial.print(b_angle);
    Serial.print("\t");

    for (int t_idx = 0; t_idx < num_steps; t_idx++) {
      Serial.print(scan_matrix[b_idx][t_idx]);
      Serial.print("\t");
    }
    Serial.println();
  }
  Serial.println("-----------------------");
}

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

void set_infrared_mode(bool state) {
  use_infrared = state;
  if (use_infrared == true){
    Serial.println("Sensor mode changed to: INFRARED");    
  }
  else{
    Serial.println("Sensor mode changed to: RAW LIGHT");    
  }
}