/*
UNO Q Smart Solar Pannel System
Created by Julián Caro Linares for Arduino and Qualcomm
CC-BY-SA

Updated to include a degree-by-degree smooth sweeping function 
to protect the mechanics and prevent sudden voltage spikes.
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

const int scan_step_degrees = 10; 
const int scan_delay_ms = 300; // Delay AFTER reaching the position to let sensors settle

// Calculate the required matrix sizes automatically
const int num_bottom_steps = ((bottom_max_position - bottom_min_position) / scan_step_degrees) + 1;
const int num_top_steps = ((top_max_position - top_min_position) / scan_step_degrees) + 1;

// 2D Arrays to store the sensor values
int scan_matrix[num_bottom_steps][num_top_steps];
int ir_matrix[num_bottom_steps][num_top_steps];

// Global Memory for the last known best position
int last_best_bottom = idle_position;
int last_best_top = idle_position;

// --- GLOBAL VARIABLES FOR PYTHON BRIDGE ---
int latest_solar_val = 0;
int latest_ir_val = 0;

// Non-blocking Timer Variables
unsigned long previousScanTime = 0;
const unsigned long scanInterval = 60000 * 5; 

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
void smoothMove(int target_b, int target_t); // NEW FUNCTION
int get_solar();
int get_infrared();
int get_bottom();
int get_top();


void setup() {
  Serial.begin(9600);
  Bridge.begin();
  
  Bridge.provide("set_weather_alert", set_weather_alert);
  Bridge.provide("get_solar", get_solar);
  Bridge.provide("get_infrared", get_infrared);
  Bridge.provide("get_bottom", get_bottom);
  Bridge.provide("get_top", get_top);
  Bridge.provide("get_temp", get_temp);
  Bridge.provide("get_humidity", get_humidity);
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(solarPin, INPUT); 

  Modulino.begin();
  light.begin();
  thermo.begin();
  vibro.begin();
  
  servo_bottom.attach(servoPin_bottom);
  servo_top.attach(servoPin_top);
  
  // Set initial position (Without smoothMove since it just booted up)
  servo_bottom.write(idle_position);
  servo_top.write(idle_position);
  delay(20000); 
}

void loop() {
  int solarValue = analogRead(solarPin);

  light.update();
  movement.update();
  
  lux = light.getAL();              
  luxCalibrated = light.getLux();   
  ir = light.getIR();               
  
  latest_solar_val = solarValue;
  latest_ir_val = ir;
  
  celsius = thermo.getTemperature();
  humidity = thermo.getHumidity();

  // Control Loop State Machine (Replaced jumps with smoothMove)
  if (weather_alert == true){
    smoothMove(bottom_min_position, top_min_position);
    panel_cleaning();
  } 
  else if (solarValue < 100) { 
    smoothMove(idle_position, idle_position);
  }
  else if (celsius > 36) { 
    Serial.println("Excessive heat alert. Safe mode activated");
    smoothMove(idle_position, top_min_position);
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
// Function: smoothMove
// Purpose: Moves servos degree-by-degree to prevent violent jerks
// ---------------------------------------------------------
void smoothMove(int target_b, int target_t) {
  int current_b = servo_bottom.read();
  int current_t = servo_top.read();

  // Keep looping until BOTH servos have reached their targets
  while (current_b != target_b || current_t != target_t) {
    
    // Step the bottom servo
    if (current_b < target_b) current_b++;
    else if (current_b > target_b) current_b--;

    // Step the top servo
    if (current_t < target_t) current_t++;
    else if (current_t > target_t) current_t--;

    // Write the new incremental positions
    servo_bottom.write(current_b);
    servo_top.write(current_t);
    
    // A 15ms delay is the industry standard for smooth servo sweeps
    delay(15); 
  }
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
    // Smooth move to the new bottom column (keeping top where it is)
    smoothMove(b, servo_top.read());
    
    int b_idx = (b - bottom_min_position) / stepSize;
    
    if (goingUp) {
      for (int t = top_min_position; t <= top_max_position; t += stepSize) {
        
        // Smooth move to the next scan point
        smoothMove(b, t);
        delay(stepDelay); // Let the panel stop shaking before reading sensors
        
        int currentSolarValue = analogRead(solarPin);
        light.update();
        int currentIRValue = light.getIR();
        
        latest_solar_val = currentSolarValue;
        latest_ir_val = currentIRValue;
        
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
        
        // Smooth move to the next scan point
        smoothMove(b, t);
        delay(stepDelay);
        
        int currentSolarValue = analogRead(solarPin);
        light.update();
        int currentIRValue = light.getIR();
        
        latest_solar_val = currentSolarValue;
        latest_ir_val = currentIRValue;
        
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

  // Gracefully glide back to the best position found during the scan!
  smoothMove(bestBottomPos, bestTopPos);

  printScanMatrix();
  printIRMatrix();

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
  smoothMove(idle_position, idle_position);
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
int get_solar() { return latest_solar_val; }
int get_infrared() { return latest_ir_val; }
int get_bottom() { return servo_bottom.read(); }
int get_top() { return servo_top.read(); }
float get_temp() { return celsius; }
float get_humidity() { return humidity; }