#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Wire.h>

LiquidCrystal lcd(2, 3, 4, 5, 6, 7);

// Button pins
#define bt_set  A0
#define bt_next A1
#define bt_up   A2
#define bt_down A3

// Output pins
#define relay   8
#define buzzer 13

// RTC DS3231
#define DS3231_I2C_ADDRESS 0x68

// System settings
const int MAX_ALARMS = 30;
const int DEBOUNCE_DELAY = 200;
const int FLASH_INTERVAL = 500;

// System variables
int hh, mm, ss, dayOfWeek;
int bellDuration = 10; // Default duration in seconds
int remainingTime = 0;
int selectedWeekday = 0; // 0=All weekdays, 1-7=specific day
int currentAlarm = 1;
int alarmHour = 8, alarmMinute = 0; // Default alarm time 8:00
int setMode = 0;
int editField = 0;
bool bellActive = false;
bool flashState = false;
unsigned long lastFlashTime = 0;
unsigned long lastButtonTime = 0;
unsigned long lastSecondCount = 0;

const String weekDays[8] = {"", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

void setup() {
  // Initialize hardware
  Wire.begin();
  lcd.begin(16, 2);
  
  // Configure pins
  pinMode(bt_set, INPUT_PULLUP);
  pinMode(bt_next, INPUT_PULLUP);
  pinMode(bt_up, INPUT_PULLUP);
  pinMode(bt_down, INPUT_PULLUP);
  pinMode(relay, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(relay, HIGH);
  
  // Load settings from EEPROM
  loadSettings();
  
  // Welcome message
  showWelcomeScreen();
  
  // Initialize timer
  lastSecondCount = millis();
}

void loop() {
  // Update RTC time
  readRTC();
  
  // Check if alarm should trigger
  checkAlarm();
  
  // Handle bell timing
  updateBellTimer();
  
  // Handle button presses
  handleButtons();
  
  // Update display
  updateDisplay();
  
  // Handle buzzer timeout
  static unsigned long lastBuzzerTime = 0;
  if (millis() - lastBuzzerTime > 100) {
    digitalWrite(buzzer, LOW);
  }
  
  // Small delay to prevent button bounce
  delay(50);
}

void loadSettings() {
  // Check if EEPROM needs initialization
  if (EEPROM.read(0) != 123) { // 123 is our magic number
    initializeEEPROM();
  }
  
  // Load settings
  bellDuration = EEPROM.read(1);
  selectedWeekday = EEPROM.read(2);
  loadAlarm(currentAlarm);
}

void initializeEEPROM() {
  // Set default values
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  
  // Mark as initialized
  EEPROM.write(0, 123);
  
  // Set default values
  EEPROM.write(1, 10); // Default duration 10 seconds
  EEPROM.write(2, 0);  // All weekdays by default
  
  // Set first alarm to 8:00
  saveAlarm(1, 8, 0);
}

void readRTC() {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  
  ss = bcdToDec(Wire.read() & 0x7F);
  mm = bcdToDec(Wire.read());
  hh = bcdToDec(Wire.read() & 0x3F);
  dayOfWeek = bcdToDec(Wire.read());
}

void setRTC(int second, int minute, int hour, int day) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);
  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(day));
  Wire.endTransmission();
}

byte decToBcd(byte val) {
  return ((val / 10 * 16) + (val % 10));
}

byte bcdToDec(byte val) {
  return ((val / 16 * 10) + (val % 16));
}

void checkAlarm() {
  static int lastTriggerMinute = -1;
  
  // Only check at minute changes and if not already active
  if (mm != lastTriggerMinute && !bellActive) {
    lastTriggerMinute = mm;
    
    // Check if we should trigger the alarm
    if (hh == alarmHour && mm == alarmMinute) {
      // Check if today is the right day
      if (selectedWeekday == 0 || dayOfWeek == selectedWeekday) {
        triggerAlarm();
      }
    }
  }
}

void triggerAlarm() {
  remainingTime = bellDuration;
  bellActive = true;
  digitalWrite(relay, LOW);
  lastSecondCount = millis(); // Reset timer
}

void updateBellTimer() {
  if (bellActive) {
    unsigned long currentMillis = millis();
    
    // Check if one second has passed
    if (currentMillis - lastSecondCount >= 1000) {
      lastSecondCount = currentMillis;
      remainingTime--;
      
      if (remainingTime <= 0) {
        bellActive = false;
        digitalWrite(relay, HIGH);
      }
    }
  }
}

void handleButtons() {
  unsigned long currentTime = millis();
  
  // Debounce check
  if (currentTime - lastButtonTime < DEBOUNCE_DELAY) {
    return;
  }
  
  // SET button - cycle through modes
  if (digitalRead(bt_set) == LOW) {
    beep();
    setMode = (setMode + 1) % 7;
    editField = 0;
    lastButtonTime = currentTime;
    lcd.clear();
    
    // Save settings when returning to normal mode
    if (setMode == 0) {
      saveSettings();
    }
    return;
  }
  
  // NEXT button - cycle through fields in current mode
  if (digitalRead(bt_next) == LOW && setMode > 0) {
    beep();
    if (setMode == 2) editField = (editField + 1) % 2; // Time has 2 fields
    if (setMode == 6) editField = (editField + 1) % 3; // Alarm has 3 fields
    lastButtonTime = currentTime;
    return;
  }
  
  // UP button - increase value
  if (digitalRead(bt_up) == LOW) {
    beep();
    adjustValue(1);
    lastButtonTime = currentTime;
    return;
  }
  
  // DOWN button - decrease value
  if (digitalRead(bt_down) == LOW) {
    beep();
    adjustValue(-1);
    lastButtonTime = currentTime;
    return;
  }
}

void beep() {
  digitalWrite(buzzer, HIGH);
  lastButtonTime = millis();
}

void adjustValue(int direction) {
  switch (setMode) {
    case 1: // Set day
      dayOfWeek = constrain(dayOfWeek + direction, 1, 7);
      setRTC(ss, mm, hh, dayOfWeek);
      break;
      
    case 2: // Set time
      if (editField == 0) hh = constrain(hh + direction, 0, 23);
      else mm = constrain(mm + direction, 0, 59);
      setRTC(ss, mm, hh, dayOfWeek);
      break;
      
    case 3: // Set duration
      bellDuration = constrain(bellDuration + direction, 1, 99);
      break;
      
    case 4: // Set weekend
      selectedWeekday = constrain(selectedWeekday + direction, 0, 7);
      break;
      
    case 5: // Set bell for
      selectedWeekday = constrain(selectedWeekday + direction, 0, 7);
      break;
      
    case 6: // Set alarm
      if (editField == 0) currentAlarm = constrain(currentAlarm + direction, 1, MAX_ALARMS);
      else if (editField == 1) alarmHour = constrain(alarmHour + direction, 0, 23);
      else alarmMinute = constrain(alarmMinute + direction, 0, 59);
      break;
  }
}

void updateDisplay() {
  // Handle display flashing for edit modes
  if (setMode > 0 && millis() - lastFlashTime > FLASH_INTERVAL) {
    flashState = !flashState;
    lastFlashTime = millis();
  }
  
  lcd.setCursor(0, 0);
  
  switch (setMode) {
    case 0: // Normal display
      lcd.print(weekDays[dayOfWeek]);
      lcd.print(" ");
      printTime(hh, mm, ss);
      
      lcd.setCursor(0, 1);
      if (bellActive) {
        lcd.print("Bell ON ");
        lcd.print(remainingTime);
        lcd.print("s ");
      } else {
        lcd.print("Next: ");
        printTime(alarmHour, alarmMinute, 0);
      }
      break;
      
    case 1: // Set day
      lcd.print("SET DAY:");
      lcd.setCursor(0, 1);
      if (flashState) lcd.print(weekDays[dayOfWeek]);
      else lcd.print("       ");
      break;
      
    case 2: // Set time
      lcd.print("SET TIME:");
      lcd.setCursor(0, 1);
      if (flashState) {
        printTime(hh, mm, 0);
      } else {
        if (editField == 0) lcd.print("  ");
        else lcd.print(hh);
        lcd.print(":");
        if (editField == 1) lcd.print("  ");
        else lcd.print(mm);
      }
      break;
      
    case 3: // Set duration
      lcd.print("DURATION:");
      lcd.setCursor(0, 1);
      if (flashState) {
        lcd.print(bellDuration);
        lcd.print(" seconds");
      } else {
        lcd.print("         ");
      }
      break;
      
    case 4: // Set weekend
      lcd.print("WEEKEND:");
      lcd.setCursor(0, 1);
      if (flashState) {
        if (selectedWeekday == 0) lcd.print("None");
        else lcd.print(weekDays[selectedWeekday]);
      } else {
        lcd.print("     ");
      }
      break;
      
    case 5: // Set bell for
      lcd.print("BELL FOR:");
      lcd.setCursor(0, 1);
      if (flashState) {
        if (selectedWeekday == 0) lcd.print("All days");
        else lcd.print(weekDays[selectedWeekday]);
      } else {
        lcd.print("        ");
      }
      break;
      
    case 6: // Set alarm
      lcd.print("ALARM ");
      lcd.print(currentAlarm);
      lcd.print("/");
      lcd.print(MAX_ALARMS);
      
      lcd.setCursor(0, 1);
      if (flashState) {
        printTime(alarmHour, alarmMinute, 0);
      } else {
        if (editField == 0) lcd.print("  ");
        else lcd.print(alarmHour);
        lcd.print(":");
        if (editField == 1) lcd.print("  ");
        else lcd.print(alarmMinute);
      }
      break;
  }
}

void printTime(int hours, int minutes, int seconds) {
  if (hours < 10) lcd.print("0");
  lcd.print(hours);
  lcd.print(":");
  if (minutes < 10) lcd.print("0");
  lcd.print(minutes);
  if (seconds > 0) {
    lcd.print(":");
    if (seconds < 10) lcd.print("0");
    lcd.print(seconds);
  }
}

void saveAlarm(int alarmNum, int hour, int minute) {
  int address = 10 + (alarmNum * 2); // Each alarm uses 2 bytes
  EEPROM.write(address, hour);
  EEPROM.write(address + 1, minute);
}

void loadAlarm(int alarmNum) {
  int address = 10 + (alarmNum * 2);
  alarmHour = EEPROM.read(address);
  alarmMinute = EEPROM.read(address + 1);
}

void saveSettings() {
  EEPROM.write(1, bellDuration);
  EEPROM.write(2, selectedWeekday);
  saveAlarm(currentAlarm, alarmHour, alarmMinute);
}

void showWelcomeScreen() {
  lcd.setCursor(0, 0);
  lcd.print("Khulna Pol. Ins.");
  lcd.setCursor(0, 1);
  lcd.print("Automatic Bell");
  delay(2000);
  lcd.clear();
}
