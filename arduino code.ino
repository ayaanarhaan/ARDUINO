#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pin Mappings
#define TRIG_PIN 5
#define ECHO_PIN 18
#define PAN_PIN  13
#define TILT_PIN 12
#define DOOR_LEFT_PIN  14
#define DOOR_RIGHT_PIN 27
#define LDR_PIN 34  // LDR Sensor Module DO Pin

// Servo Objects
Servo panServo;
Servo tiltServo;
Servo doorLeft;
Servo doorRight;

long duration;
int distance;
bool studentDetected = false;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);

  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);
  doorLeft.attach(DOOR_LEFT_PIN);
  doorRight.attach(DOOR_RIGHT_PIN);

  // Set Default Servo Positions
  panServo.write(90);    // Center camera
  tiltServo.write(45);   // Face height
  closeDoors();          // Lock double door

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  showIdleScreen();
}

void loop() {
  // Read Distance from Ultrasonic Sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.034 / 2;

  // 1. Detect Student
  if (distance > 0 && distance <= 30 && !studentDetected) {
    studentDetected = true;
    triggerScanSequence();
  }

  // 2. Read Python Verification Results over Serial
  if (Serial.available() > 0) {
    String response = Serial.readStringUntil('\n');
    response.trim();

    if (response.startsWith("SUCCESS:")) {
      String name = response.substring(8);
      executeDoorEntrySequence(name);
    } 
    else if (response == "DUPLICATE") {
      displayMessage("ALREADY MARKED", "Attendance recorded");
      delay(3000);
      resetRobot();
    }
    else if (response == "UNKNOWN") {
      displayMessage("ACCESS DENIED", "Unknown Student");
      delay(3000);
      resetRobot();
    }
  }

  delay(100);
}

void triggerScanSequence() {
  displayMessage("STUDENT DETECTED", "Scanning Face & ID...");
  tiltServo.write(45);
  delay(800);
  tiltServo.write(85);
  delay(500);

  Serial.println("START_SCAN");
}

void executeDoorEntrySequence(String name) {
  displayMessage("WELCOME!", name.c_str());

  // Open both door servos
  openDoors();

  // Wait for the single student to pass through the LDR beam
  unsigned long timeoutStart = millis();
  bool personPassed = false;

  displayMessage("PASS THROUGH", "Door Unlocked");

  while (millis() - timeoutStart < 8000) { // 8-second passage timeout limit
    int ldrState = digitalRead(LDR_PIN);

    // If light beam is interrupted (person is passing through doorway)
    if (ldrState == HIGH) { // Adjust HIGH/LOW based on LDR module active state
      personPassed = true;
    }
    
    // Once beam clears after interruption, close doors immediately
    if (personPassed && ldrState == LOW) {
      delay(500); // Brief buffer time to let person clear the leaves
      break;
    }
    delay(50);
  }

  closeDoors();
  resetRobot();
}

void openDoors() {
  doorLeft.write(90);   // Swing Left Door Open
  doorRight.write(90);  // Swing Right Door Open
}

void closeDoors() {
  doorLeft.write(0);    // Closed Position Left
  doorRight.write(180); // Closed Position Right
}

void resetRobot() {
  tiltServo.write(45);
  panServo.write(90);
  studentDetected = false;
  showIdleScreen();
}

void displayMessage(const char* title, const char* subtitle) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 15);
  display.println(title);
  display.setCursor(10, 35);
  display.println(subtitle);
  display.display();
}

void showIdleScreen() {
  displayMessage("SMART ATTENDANCE", "[ Step Closer ]");
}
