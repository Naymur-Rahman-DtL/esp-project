#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// HC-SR04 pins
#define TRIG_PIN D5  // GPIO14
#define ECHO_PIN D6  // GPIO12

void setup() {
  Serial.begin(115200);

  // HC-SR04 pin setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // OLED initialization
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 failed to initialize.");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Distance Sensor");
  display.display();
  delay(1000);
}

void loop() {
  // Send a 10µs pulse to trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo time
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance_cm = duration * 0.034 / 2;

  // Print to serial monitor
  Serial.print("Distance: ");
  Serial.print(distance_cm);
  Serial.println(" cm");

  // Display on OLED
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("Distance:");

  display.setTextSize(2);
  display.setCursor(10, 40);
  display.print(distance_cm, 1);
  display.print(" cm");

  display.display();

  delay(300);  // Update every 0.3s
}
