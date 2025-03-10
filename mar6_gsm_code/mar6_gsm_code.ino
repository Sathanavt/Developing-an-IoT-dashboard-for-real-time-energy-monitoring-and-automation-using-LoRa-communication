#include <SoftwareSerial.h>

#define GSM_RX 10  // RX pin for GSM module
#define GSM_TX 11  // TX pin for GSM module

#define BUTTON1 2  // Pin for push button 1
#define BUTTON2 3  // Pin for push button 2

SoftwareSerial GSM(GSM_RX, GSM_TX);  // Create a SoftwareSerial object for GSM communication

void setup() {
  Serial.begin(9600);   // Start serial communication for debugging
  GSM.begin(9600);      // Start GSM module communication

  pinMode(BUTTON1, INPUT_PULLUP);  // Set button pins as input with internal pull-up resistors
  pinMode(BUTTON2, INPUT_PULLUP);

  Serial.println("Initializing GSM Module...");
  
  // Test GSM module with AT command
  GSM.println("AT");
  delay(1000);
  printGSMResponse();

  // Set SMS to text mode
  GSM.println("AT+CMGF=1");
  delay(1000);
  printGSMResponse();
}

void loop() {
  // Check if button 1 is pressed
  if (digitalRead(BUTTON1) == LOW) {
    sendSMS("+916383590160", "80% alert");
    delay(1000);  // Debounce delay
  }

  // Check if button 2 is pressed
  if (digitalRead(BUTTON2) == LOW) {
    sendSMS("+919944486867", "50% alert");
    delay(1000);  // Debounce delay
  }
}

// Function to send an SMS
void sendSMS(String phoneNumber, String message) {
  Serial.println("Sending SMS...");

  GSM.print("AT+CMGS=\"");     // Send SMS command
  GSM.print(phoneNumber);      // Add phone number
  GSM.println("\"");
  delay(1000);                 // Wait for the prompt
  printGSMResponse();          // Print response to check for issues

  GSM.print(message);          // Add the message content
  delay(500);
  
  GSM.write(26);               // ASCII code for CTRL+Z to send the SMS
  delay(5000);                 // Wait for the SMS to be sent

  Serial.println("SMS sent!");
  printGSMResponse();          // Print the GSM module response
}

// Function to print the GSM module response to the Serial Monitor
void printGSMResponse() {
  while (GSM.available()) {
    Serial.write(GSM.read());
  }
}