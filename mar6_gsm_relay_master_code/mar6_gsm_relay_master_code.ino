//gsm master 
#include <WiFi.h>
#include <ThingSpeak.h>
#include <SPI.h>
#include <LoRa.h>

#define NSS 15
#define RST 17
#define DIO0 4

// --- WiFi & ThingSpeak configuration ---
const char* ssid = "Galaxy S25 Ultra 7144";
const char* password = "spm12345";

// ThingSpeak settings
unsigned long myChannelNumber = 2863678;
const char* myWriteAPIKey = "4HMTN3WLK0O15447";

// Pushbutton pin definitions
#define BUTTON_1 23  // Pin for Slave 1 LED ON
#define BUTTON_2 25  // Pin for Slave 1 LED OFF
#define BUTTON_3 26  // Pin for Slave 2 LED ON
#define BUTTON_4 27  // Pin for Slave 2 LED OFF

// GPIO pins for threshold monitoring signals
#define SLAVE1_SIGNAL_PIN 18
#define SLAVE2_SIGNAL_PIN 19

// Energy threshold values for each slave
const float ENERGY_THRESHOLD_SLAVE1 = 0.00004; // Threshold for Slave 1
const float ENERGY_THRESHOLD_SLAVE2 = 0.00006; // Threshold for Slave 2

WiFiClient client;
String currentSlave = "1"; // Start with Slave 1

// Variables to hold sensor data from the slaves
float current1 = 0.0, voltage1 = 0.0, power1 = 0.0, energy1 = 0.0;
float current2 = 0.0, voltage2 = 0.0, power2 = 0.0, energy2 = 0.0;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Master Starting...");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected.");

  // Initialize ThingSpeak
  ThingSpeak.begin(client);

  // Initialize LoRa
  LoRa.setPins(NSS, RST, DIO0);
  SPI.begin(14, 12, 13);
  if (!LoRa.begin(865E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa initialized.");

  // Set button pins as input
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);
  pinMode(BUTTON_4, INPUT_PULLUP);

  // Set signal pins as output
  pinMode(SLAVE1_SIGNAL_PIN, OUTPUT);
  pinMode(SLAVE2_SIGNAL_PIN, OUTPUT);

  // Initially set signal pins LOW
  digitalWrite(SLAVE1_SIGNAL_PIN, LOW);
  digitalWrite(SLAVE2_SIGNAL_PIN, LOW);
}

void loop() {
  // ---- Check pushbutton states and send commands accordingly ----
  if (digitalRead(BUTTON_1) == LOW) { // Button 1 pressed
    sendLoRaCommand("1 on"); // Slave 1 LED ON
    delay(300);              // Debounce delay
  } 
  if (digitalRead(BUTTON_2) == LOW) { // Button 2 pressed
    sendLoRaCommand("1 off"); // Slave 1 LED OFF
    delay(300);               // Debounce delay
  } 
  if (digitalRead(BUTTON_3) == LOW) { // Button 3 pressed
    sendLoRaCommand("2 on"); // Slave 2 LED ON
    delay(300);              // Debounce delay
  } 
  if (digitalRead(BUTTON_4) == LOW) { // Button 4 pressed
    sendLoRaCommand("2 off"); // Slave 2 LED OFF
    delay(300);               // Debounce delay
  }

  // ---- Sensor Request Cycle ----
  // Send a REQUEST command to the current slave (format: "1 request" or "2 request")
  String command = currentSlave + " request";
  LoRa.beginPacket();
  LoRa.print(command);
  LoRa.endPacket();
  Serial.print("Request sent to Slave ");
  Serial.println(currentSlave);

  // ---- Wait for a response for up to 3 seconds ----
  unsigned long startTime = millis();
  bool packetReceived = false;
  String receivedPacket = "";
  while (millis() - startTime < 3000) { // wait up to 3 seconds
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      packetReceived = true;
      while (LoRa.available()) {
        receivedPacket += (char)LoRa.read();
      }
      break;
    }
  }

  if (packetReceived) {
    Serial.println("Packet received from slave!");
    Serial.print("Received data: ");
    Serial.println(receivedPacket);
    
    // ---- Parse the sensor data based on the current slave ----
    if (currentSlave == "1") {
      current1 = parseValue(receivedPacket, "Current: ", " a,");
      voltage1 = parseValue(receivedPacket, "Voltage: ", " v,");
      power1   = parseValue(receivedPacket, "Power: ", " kw,");
      energy1  = parseValue(receivedPacket, "Energy: ", " kwh");

      // Check energy threshold for Slave 1
      if (energy1 > ENERGY_THRESHOLD_SLAVE1) {
        digitalWrite(SLAVE1_SIGNAL_PIN, HIGH); // Signal HIGH if energy exceeds threshold
        Serial.println("Slave 1 energy above threshold. Signal HIGH on pin 18.");
      } else {
        digitalWrite(SLAVE1_SIGNAL_PIN, LOW); // Signal LOW otherwise
        Serial.println("Slave 1 energy below threshold. Signal LOW on pin 18.");
      }
    } else if (currentSlave == "2") {
      current2 = parseValue(receivedPacket, "Current: ", " a,");
      voltage2 = parseValue(receivedPacket, "Voltage: ", " v,");
      power2   = parseValue(receivedPacket, "Power: ", " kw,");
      energy2  = parseValue(receivedPacket, "Energy: ", " kwh");

      // Check energy threshold for Slave 2
      if (energy2 > ENERGY_THRESHOLD_SLAVE2) {
        digitalWrite(SLAVE2_SIGNAL_PIN, HIGH); // Signal HIGH if energy exceeds threshold
        Serial.println("Slave 2 energy above threshold. Signal HIGH on pin 19.");
      } else {
        digitalWrite(SLAVE2_SIGNAL_PIN, LOW); // Signal LOW otherwise
        Serial.println("Slave 2 energy below threshold. Signal LOW on pin 19.");
      }
    }

    // ---- Send all sensor data (from both slaves) to ThingSpeak ----
    if (WiFi.status() == WL_CONNECTED) {
      ThingSpeak.setField(1, current1);
      ThingSpeak.setField(2, voltage1);
      ThingSpeak.setField(3, current2);
      ThingSpeak.setField(4, voltage2);
      ThingSpeak.setField(5, power1);
      ThingSpeak.setField(6, energy1);
      ThingSpeak.setField(7, power2);
      ThingSpeak.setField(8, energy2);

      int status = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
      if (status == 200) {
        Serial.println("Data successfully sent to ThingSpeak!");
      } else {
        Serial.print("Error sending data: ");
        Serial.println(status);
      }
    } else {
      Serial.println("WiFi not connected!");
    }
  } else {
    Serial.println("No response from Slave " + currentSlave);
  }

  // ---- Alternate to the next Slave for the next cycle ----
  if (currentSlave == "1") {
    currentSlave = "2";
  } else {
    currentSlave = "1";
  }

  delay(15000); // Delay to respect ThingSpeak's update rate (15 seconds)
}

// ---- Function to send LoRa commands ----
void sendLoRaCommand(String command) {
  LoRa.beginPacket();
  LoRa.print(command);
  LoRa.endPacket();
  Serial.print("Sent command: ");
  Serial.println(command);
}

// ---- Helper function to parse values from a string ----
float parseValue(String data, String startDelimiter, String endDelimiter) {
  data.toLowerCase();  // make lowercase for easier matching
  int startIndex = data.indexOf(startDelimiter);
  if (startIndex != -1) {
    startIndex += startDelimiter.length();
    int endIndex = data.indexOf(endDelimiter, startIndex);
    if (endIndex != -1) {
      return data.substring(startIndex, endIndex).toFloat();
    }
  }
  return 0.0;
}