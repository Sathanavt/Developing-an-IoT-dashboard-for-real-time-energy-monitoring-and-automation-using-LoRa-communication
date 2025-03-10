#include <SPI.h>
#include <LoRa.h>

#define NODE_ID 1          // Change to 2 for Slave 2
#define NSS 15
#define RST 17
#define DIO0 4
#define LED_PIN 2

// --- Current sensor configuration ---
const int currentSensorPin = 35;
const float CURRENT_V_REF = 3.3;
const int CURRENT_ADC_MAX = 4095;
const float CURRENT_SENSOR_SENSITIVITY = 0.79;

// --- Voltage sensor configuration ---
const int voltageSensorPin = 34;
const float VOLTAGE_V_REF = 3.3;
const int VOLTAGE_ADC_MAX = 4095;
const float VOLTAGE_DIVIDER_RATIO = 66.67;

float totalEnergy = 0;

// For sensor transmission timing
unsigned long previousSensorMillis = 0;
const unsigned long sensorInterval = 2000; // 2 seconds

void setup() {
  Serial.begin(115200);
  Serial.println("Slave " + String(NODE_ID) + " starting...");

  // Initialize LoRa
  LoRa.setPins(NSS, RST, DIO0);
  SPI.begin(14, 12, 13);
  if (!LoRa.begin(865E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa Transmitter and Receiver initialized.");

  // Setup LED for remote control
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  // ---- Check for incoming LoRa commands ----
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String command = "";
    while (LoRa.available()) {
      command += (char)LoRa.read();
    }
    command.trim();
    command.toLowerCase();
    Serial.print("Received command: ");
    Serial.println(command); // Debug print

    // Make sure the command is directed to this node
    String nodeStr = String(NODE_ID);
    if (command.startsWith(nodeStr)) {
      // Check if this is a sensor request command
      if (command.indexOf("request") != -1) {
        Serial.println("Matched REQUEST command. Sending sensor response...");
        sendSensorData();
      }
      // If command contains "on" (and not a request) then turn LED on
      else if (command.indexOf("on") != -1) {
        Serial.println("Received LED ON command.");
        digitalWrite(LED_PIN, HIGH);
      }
      // If command contains "off" then turn LED off
      else if (command.indexOf("off") != -1) {
        Serial.println("Received LED OFF command.");
        digitalWrite(LED_PIN, LOW);
      }
      else {
        Serial.println("Unrecognized command for this node.");
      }
    }
    else {
      Serial.println("Command not for this node. Ignoring...");
    }
  }

  // ---- Periodically send sensor data (if no command is received) ----
  unsigned long currentMillis = millis();
  if (currentMillis - previousSensorMillis >= sensorInterval) {
    // You may choose to send sensor data periodically OR only on a request.
    previousSensorMillis = currentMillis;
    sendSensorData();
  }
}

// ---- Function to read sensors and send sensor data over LoRa ----
void sendSensorData() {
  // Read sensor values
  int rawCurrentADC = analogRead(currentSensorPin);
  float currentSensorVoltage = (rawCurrentADC * CURRENT_V_REF) / CURRENT_ADC_MAX;
  float measuredCurrent = currentSensorVoltage / CURRENT_SENSOR_SENSITIVITY;

  int rawVoltageADC = analogRead(voltageSensorPin);
  float voltageSensorVoltage = (rawVoltageADC * VOLTAGE_V_REF) / VOLTAGE_ADC_MAX;
  float mainsVoltage = voltageSensorVoltage * VOLTAGE_DIVIDER_RATIO;

  float power = (mainsVoltage * measuredCurrent) / 1000.0; // in kW
  // Calculate energy based on elapsed time (in hours)
  float deltaTime = (millis() - previousSensorMillis) / 3600000.0;
  // Update previousSensorMillis immediately after to avoid long delays
  previousSensorMillis = millis();
  float energy = power * deltaTime;
  totalEnergy += energy;

  // Construct sensor data string
  String sensorData = String(NODE_ID) + ",Voltage: " + String(mainsVoltage, 3) + " V, Current: " 
                      + String(measuredCurrent, 3) + " A, Power: " + String(power, 3) 
                      + " kW, Energy: " + String(totalEnergy, 6) + " kWh";

  LoRa.beginPacket();
  LoRa.print(sensorData);
  LoRa.endPacket();

  Serial.print("Sending sensor response: ");
  Serial.println(sensorData);
}
