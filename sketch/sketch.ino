/*
UNO Q Smart Solar Pannel System
Created by Julián Caro Linares for Arduino and Qualcomm
CC-BY-SA

Updated to fully integrate the Bridge variables for Python Telemetry,
fixing the variable scope errors.
*/

#include <Arduino_RouterBridge.h>
#include <Arduino_Modulino.h>
#include "Servo.h"

// Servo Objects
Servo servo_bottom;
Servo servo_top;

// Modulino Objects
ModulinoLight light;
ModulinoThermo thermo;
ModulinoMovement movement;
ModulinoVibro vibro;

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

const int scan_step_degrees = 10; // 5 degrees for High Resolution 
const int scan_delay_ms = 100;

// Calculate the required matrix sizes automatically based on independent limits
const int num_bottom_steps = ((bottom_max_position - bottom_min_position) / scan_step_degrees) + 1;
const int num_top_steps = ((top_max_position - top_min_position) / scan_step_degrees) + 1;

// 2D Arrays to store the sensor values (The Sky Heatmaps)
int scan_matrix[num_bottom_steps][num_top_steps];
int ir_matrix[num_bottom_steps][num_top_steps];

// Global Memory for the last known best position
int last_best_bottom = idle_position;
int last_best_top = idle_position;

// --- GLOBAL VARIABLES FOR PYTHON BRIDGE ---
int latest_solar_val = 0;
int latest_ir_val = 0;
// ------------------------------------------

// Non-blocking Timer Variables
unsigned long previousScanTime = 0;
const unsigned long scanInterval = 60000 * 5; // 300,000 ms = 5 minutes

// Modulino Variables
int lux = 0;           
int luxCalibrated = 0; 
int ir = 0;            
float celsius = 0;
float humidity = 0;
float x_accel, y_accel, z_accel;
float roll, pitch, yaw;

// Alerts
bool weather_alert = false;

// ==========================================
// FORWARD DECLARATIONS
// ==========================================
void set_weather_alert(bool state);
void scanForMaxValue(int stepSize, int stepDelay);
void printScanMatrix();
void printIRMatrix();
void panel_cleaning();
int get_solar();
int get_infrared();

void setup() {
  // Bridge Initialization
  Serial.begin(9600);
  Bridge.begin();
  
  // Register all Python-accessible functions
  Bridge.provide("set_weather_alert", set_weather_alert);
  Bridge.provide("get_solar", get_solar);
  Bridge.provide("get_infrared", get_infrared);
  Bridge.provide("get_bottom", get_bottom);
  Bridge.provide("get_top", get_top);
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(solarPin, INPUT); 

  // Modulino Init
  Modulino.begin();
  light.begin();
  thermo.begin();
  vibro.begin();
  
  // Servo motors Init
  servo_bottom.attach(servoPin_bottom);
  servo_top.attach(servoPin_top);
  
  // Move to idle initially
  servo_bottom.write(idle_position);
  servo_top.write(idle_position);
  delay(10000);
}

void loop() {
  // Read the current solar panel analog value (0 - 1023)
  int solarValue = analogRead(solarPin);

  // Modulino Readings
  light.update();
  movement.update();
  
  // Modulino Light Readings
  lux = light.getAL();              
  luxCalibrated = light.getLux();   
  ir = light.getIR();               
  
  // Update the global variables so Python can fetch the latest data!
  latest_solar_val = solarValue;
  latest_ir_val = ir;
  
  // Modulino Temp Humidity Readings
  celsius = thermo.getTemperature();
  humidity = thermo.getHumidity();

  // Control Loop State Machine
  if (weather_alert == true){
    servo_bottom.write(bottom_min_position);
    servo_top.write(top_min_position);
    panel_cleaning();
  } 
  else if (solarValue < 100) { 
    servo_bottom.write(idle_position);
    servo_top.write(idle_position);
  }
  else if (celsius > 36) { 
    Serial.println("Excessive heat alert. Safe mode activated");
    servo_bottom.write(idle_position);
    servo_top.write(top_min_position);
  }
  else {
    unsigned long currentMillis = millis();
    if (currentMillis - previousScanTime >= scanInterval || previousScanTime == 0) {
      Serial.println("Starting Sky Scan...");
      scanForMaxValue(scan_step_degrees, scan_delay_ms);
      previousScanTime = millis(); 
    }
  }
  
  delay(1000); 
}

// ---------------------------------------------------------
// Function: scanForMaxValue
// ---------------------------------------------------------
void scanForMaxValue(int stepSize, int stepDelay) {
  int maxValueFound = -1;
  int bestBottomPos = idle_position;
  int bestTopPos = idle_position;
  
  bool goingUp = true; 

  for (int b = bottom_min_position; b <= bottom_max_position; b += stepSize) {
    servo_bottom.write(b);
    int b_idx = (b - bottom_min_position) / stepSize;
    
    if (goingUp) {
      for (int t = top_min_position; t <= top_max_position; t += stepSize) {
        servo_top.write(t);
        delay(stepDelay);
        
        // Read values
        int currentSolarValue = analogRead(solarPin);
        light.update();
        int currentIRValue = light.getIR();
        
        // Update global variables for Python while scanning
        latest_solar_val = currentSolarValue;
        latest_ir_val = currentIRValue;
        
        // Save to matrices
        int t_idx = (t - top_min_position) / stepSize;
        scan_matrix[b_idx][t_idx] = currentSolarValue;
        ir_matrix[b_idx][t_idx] = currentIRValue;
        
        if (currentSolarValue > maxValueFound) {
          maxValueFound = currentSolarValue;
          bestBottomPos = b;
          bestTopPos = t;
        }
      }
    } else {
      for (int t = top_max_position; t >= top_min_position; t -= stepSize) {
        servo_top.write(t);
        delay(stepDelay);
        
        // Read values
        int currentSolarValue = analogRead(solarPin);
        light.update();
        int currentIRValue = light.getIR();
        
        // Update global variables for Python while scanning
        latest_solar_val = currentSolarValue;
        latest_ir_val = currentIRValue;
        
        // Save to matrices
        int t_idx = (t - top_min_position) / stepSize;
        scan_matrix[b_idx][t_idx] = currentSolarValue;
        ir_matrix[b_idx][t_idx] = currentIRValue;
        
        if (currentSolarValue > maxValueFound) {
          maxValueFound = currentSolarValue;
          bestBottomPos = b;
          bestTopPos = t;
        }
      }
    }
    
    goingUp = !goingUp; 
  }

  last_best_bottom = bestBottomPos;
  last_best_top = bestTopPos;

  servo_bottom.write(bestBottomPos);
  servo_top.write(bestTopPos);

  printScanMatrix();
  printIRMatrix();

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
// Print Functions & Utilities
// ---------------------------------------------------------
void printScanMatrix() {
  Serial.println("\n--- Sky Heatmap [Data Source: SOLAR PANEL (A0)] ---");
  Serial.print("B\\T\t");
  for (int t = top_min_position; t <= top_max_position; t += scan_step_degrees) {
    Serial.print(t);
    Serial.print("\t");
  }
  Serial.println();
  for (int b_idx = 0; b_idx < num_bottom_steps; b_idx++) {
    int b_angle = bottom_min_position + (b_idx * scan_step_degrees);
    Serial.print(b_angle);
    Serial.print("\t");
    for (int t_idx = 0; t_idx < num_top_steps; t_idx++) {
      Serial.print(scan_matrix[b_idx][t_idx]);
      Serial.print("\t");
    }
    Serial.println();
  }
  Serial.println("-----------------------");
}

void printIRMatrix() {
  Serial.println("\n--- Sky Heatmap [Data Source: MODULINO INFRARED (IR)] ---");
  Serial.print("B\\T\t");
  for (int t = top_min_position; t <= top_max_position; t += scan_step_degrees) {
    Serial.print(t);
    Serial.print("\t");
  }
  Serial.println();
  for (int b_idx = 0; b_idx < num_bottom_steps; b_idx++) {
    int b_angle = bottom_min_position + (b_idx * scan_step_degrees);
    Serial.print(b_angle);
    Serial.print("\t");
    for (int t_idx = 0; t_idx < num_top_steps; t_idx++) {
      Serial.print(ir_matrix[b_idx][t_idx]);
      Serial.print("\t");
    }
    Serial.println();
  }
  Serial.println("-----------------------");
}

void panel_cleaning() {
  servo_bottom.write(idle_position);
  servo_top.write(idle_position);
  delay(100);
  vibro.on(5000);
  delay(1000);
  vibro.off();
  delay(10000);
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

// ==========================================
// PYTHON BRIDGE GETTER FUNCTIONS
// ==========================================
int get_solar() {
  return latest_solar_val;
}

int get_infrared() {
  return latest_ir_val;
}

int get_bottom() {
  return servo_bottom.read();
}

int get_top() {
  return servo_top.read();
}