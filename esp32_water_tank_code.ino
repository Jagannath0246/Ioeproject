// ESP32 Water Tank Management System
// Two water sensors (top and bottom), relay for motor control, WiFi connectivity

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server URL for API
const char* serverURL = "http://YOUR_SERVER_IP:5000/api";

// Pin definitions
#define TOP_SENSOR_PIN 34      // Analog pin for top water sensor
#define BOTTOM_SENSOR_PIN 35   // Analog pin for bottom water sensor
#define RELAY_PIN 26           // Digital pin for relay module
#define LED_PIN 2              // Built-in LED for status indication

// Sensor thresholds
const int WATER_THRESHOLD = 500;  // Adjust based on your sensor readings

// System variables
bool motorState = false;
bool autoMode = true;  // true = automatic, false = manual
unsigned long lastSensorRead = 0;
unsigned long lastDataSend = 0;
const unsigned long SENSOR_INTERVAL = 5000;   // Read sensors every 5 seconds
const unsigned long DATA_SEND_INTERVAL = 10000; // Send data every 10 seconds

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(TOP_SENSOR_PIN, INPUT);
  pinMode(BOTTOM_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Initialize relay (OFF state)
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to WiFi. IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long currentTime = millis();

  // Read sensors periodically
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    readSensorsAndControl();
    lastSensorRead = currentTime;
  }

  // Send data to server periodically
  if (currentTime - lastDataSend >= DATA_SEND_INTERVAL) {
    sendDataToServer();
    checkServerCommands();
    lastDataSend = currentTime;
  }

  delay(100);
}

void readSensorsAndControl() {
  // Read water level sensors
  int topSensorValue = analogRead(TOP_SENSOR_PIN);
  int bottomSensorValue = analogRead(BOTTOM_SENSOR_PIN);

  // Determine water levels
  bool waterAtTop = (topSensorValue > WATER_THRESHOLD);
  bool waterAtBottom = (bottomSensorValue > WATER_THRESHOLD);

  // Print sensor readings
  Serial.println("=== Sensor Readings ===");
  Serial.print("Top Sensor: "); Serial.print(topSensorValue);
  Serial.print(" (Water: "); Serial.print(waterAtTop ? "YES" : "NO"); Serial.println(")");
  Serial.print("Bottom Sensor: "); Serial.print(bottomSensorValue);
  Serial.print(" (Water: "); Serial.print(waterAtBottom ? "YES" : "NO"); Serial.println(")");

  // Automatic motor control logic
  if (autoMode) {
    if (!waterAtBottom && !motorState) {
      // Tank is empty, start motor
      startMotor();
      Serial.println("AUTO: Tank empty - Motor started");
    }
    else if (waterAtTop && motorState) {
      // Tank is full, stop motor
      stopMotor();
      Serial.println("AUTO: Tank full - Motor stopped");
    }
  }

  // Update LED status
  digitalWrite(LED_PIN, motorState ? HIGH : LOW);
}

void startMotor() {
  digitalWrite(RELAY_PIN, HIGH);
  motorState = true;
  Serial.println("MOTOR: Started");
}

void stopMotor() {
  digitalWrite(RELAY_PIN, LOW);
  motorState = false;
  Serial.println("MOTOR: Stopped");
}

void sendDataToServer() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(serverURL) + "/sensor-data");
    http.addHeader("Content-Type", "application/json");

    // Prepare JSON data
    StaticJsonDocument<200> doc;
    doc["device_id"] = "ESP32_TANK_01";
    doc["top_sensor"] = analogRead(TOP_SENSOR_PIN);
    doc["bottom_sensor"] = analogRead(BOTTOM_SENSOR_PIN);
    doc["motor_state"] = motorState;
    doc["auto_mode"] = autoMode;
    doc["timestamp"] = millis();

    String jsonString;
    serializeJson(doc, jsonString);

    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
      Serial.print("Data sent successfully. Response: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error sending data: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
}

void checkServerCommands() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(serverURL) + "/commands");

    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String response = http.getString();

      StaticJsonDocument<200> doc;
      deserializeJson(doc, response);

      // Check for manual motor control commands
      if (doc.containsKey("motor_command")) {
        String command = doc["motor_command"];
        if (command == "start") {
          autoMode = false;
          startMotor();
          Serial.println("SERVER: Manual motor start command received");
        }
        else if (command == "stop") {
          autoMode = false;
          stopMotor();
          Serial.println("SERVER: Manual motor stop command received");
        }
        else if (command == "auto") {
          autoMode = true;
          Serial.println("SERVER: Auto mode enabled");
        }
      }
    }

    http.end();
  }
}
