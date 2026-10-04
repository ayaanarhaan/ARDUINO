#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pin Definitions
const int SOIL_PIN = 34;       // Analog input for soil sensor
const int TRIG_PIN = 5;        // Ultrasonic Trig
const int ECHO_PIN = 18;       // Ultrasonic Echo
const int BUTTON_PIN = 4;      // Push button pin
const int SERVO_PIN = 13;      // SG90 Servo pin
const int BUZZER_PIN = 12;     // Buzzer alert pin

// Logic Variables
int buttonClicks = 1;
int targetMoisture = 40;
unsigned long lastPersonTime = 0;
const unsigned long AUTO_WATER_INTERVAL = 600000; // 10 minutes in milliseconds (10 * 60 * 1000)

Servo bottleServo;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  bottleServo.attach(SERVO_PIN);
  bottleServo.write(0); // Valve closed

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  lastPersonTime = millis();
}

int getSoilMoisture() {
  int raw = analogRead(SOIL_PIN);
  // Map raw analog value (e.g., 4095 dry to 1500 wet) to 0-100%
  int moisture = map(raw, 4095, 1500, 0, 100);
  return constrain(moisture, 0, 100);
}

float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2; // Distance in cm
}

void loop() {
  // 1. Read Button Clicks
  static int lastBtnState = HIGH;
  int btnState = digitalRead(BUTTON_PIN);
  if (lastBtnState == HIGH && btnState == LOW) {
    buttonClicks++;
    if (buttonClicks > 3) buttonClicks = 1;
    
    if (buttonClicks == 1) targetMoisture = 40;
    else if (buttonClicks == 2) targetMoisture = 60;
    else if (buttonClicks == 3) targetMoisture = 80;
    delay(200); // Debounce
  }
  lastBtnState = btnState;

  // 2. Read Sensors
  int currentMoisture = getSoilMoisture();
  int requiredMoisture = targetMoisture - currentMoisture;
  if (requiredMoisture < 0) requiredMoisture = 0;
  
  float distance = getDistance();
  bool personNearby = (distance > 0 && distance < 50);

  // 3. Handle Timers & Automation Logic
  if (personNearby) {
    lastPersonTime = millis(); // Reset 10-minute timer if someone is seen
    if (currentMoisture < targetMoisture) {
      digitalWrite(BUZZER_PIN, HIGH); // Alert nearby person
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // 10-Minute Timeout Trigger (No person detected for 10 min AND soil is dry)
  if (!personNearby && (millis() - lastPersonTime >= AUTO_WATER_INTERVAL)) {
    if (currentMoisture < targetMoisture) {
      // Pour water automatically
      bottleServo.write(90); // Open valve
      delay(3000);           // Dispense for 3 seconds
      bottleServo.write(0);  // Close valve
      lastPersonTime = millis(); // Reset timer after watering
    }
  }

  // 4. Update OLED Display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.printf("SOIL MOISTURE: %d%%\n", currentMoisture);
  
  display.setCursor(0, 20);
  display.printf("TARGET: %d%%\n", targetMoisture);
  
  display.setCursor(0, 40);
  display.printf("WATER REQ: %d%%\n", requiredMoisture);

  display.display();
  delay(100);
}
