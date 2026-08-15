#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions
const int PIN_555_ON   = 14; // D5
const int PIN_555_OFF  = 12; // D6
const int PIN_RELAY    = 13; // D7
const int PIN_BUTTON   = 0;  // D3

// Timing Multiplier for CD4020 (14-bit binary counter)
const unsigned long COUNTER_MAX = 16384; 

// Variables for Measuring 555 ON Pulse Period
unsigned long lastMicrosON = 0;
unsigned long periodON = 0; 
int lastStateON = LOW;

// Variables for Measuring 555 OFF Pulse Period
unsigned long lastMicrosOFF = 0;
unsigned long periodOFF = 0; 
int lastStateOFF = LOW;

// Mode Tracking (Relay State)
int lastRelayState = LOW;
unsigned long modeStartTime = 0;

// Button & UI State
bool showConfiguredTimes = false; 
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Display refresh timer
unsigned long lastDisplayUpdate = 0;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_555_ON, INPUT);
  pinMode(PIN_555_OFF, INPUT);
  pinMode(PIN_RELAY, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  lastRelayState = digitalRead(PIN_RELAY);
  modeStartTime = millis();
}

// Helper: Format and print time to OLED strictly in HH:MM (Neglecting seconds)
void printFormattedTime(unsigned long ms) {
  // Convert directly to total minutes, dropping the seconds entirely
  unsigned long totalMinutes = ms / 60000ULL; 
  unsigned int hours = totalMinutes / 60;
  unsigned int minutes = totalMinutes % 60;

  if (hours < 10) display.print("0");
  display.print(hours);
  display.print(":");
  if (minutes < 10) display.print("0");
  display.print(minutes);
}

void loop() {
  unsigned long currentMicros = micros();
  unsigned long currentMillis = millis();

  // 1. NON-BLOCKING PERIOD MEASUREMENT FOR MOTOR ON 555
  int stateON = digitalRead(PIN_555_ON);
  if (stateON == HIGH && lastStateON == LOW) {
    periodON = currentMicros - lastMicrosON;
    lastMicrosON = currentMicros;
  }
  lastStateON = stateON;

  // 2. NON-BLOCKING PERIOD MEASUREMENT FOR MOTOR OFF 555
  int stateOFF = digitalRead(PIN_555_OFF);
  if (stateOFF == HIGH && lastStateOFF == LOW) {
    periodOFF = currentMicros - lastMicrosOFF;
    lastMicrosOFF = currentMicros;
  }
  lastStateOFF = stateOFF;

  // 3. TRACK RELAY STATE TO RESET ELAPSED TIME
  int relayState = digitalRead(PIN_RELAY);
  if (relayState != lastRelayState) {
    modeStartTime = currentMillis; 
    lastRelayState = relayState;
  }

  // 4. BUTTON DEBOUNCE & TOGGLE VIEW
  int reading = digitalRead(PIN_BUTTON);
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) { 
      showConfiguredTimes = !showConfiguredTimes; 
      lastDebounceTime = currentMillis + 200;     
    }
  }
  lastButtonState = reading;

  // 5. UPDATE DISPLAY 
  if (currentMillis - lastDisplayUpdate > 200) {
    lastDisplayUpdate = currentMillis;
    display.clearDisplay();

    // Convert microsecond periods to total milliseconds for full cycle
    unsigned long totalTimeON_ms = (periodON * COUNTER_MAX) / 1000ULL;
    unsigned long totalTimeOFF_ms = (periodOFF * COUNTER_MAX) / 1000ULL;

    if (showConfiguredTimes) {
      // --- VIEW 2: TOTAL CONFIGURED TIMES (HH:MM) ---
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println(F("CALCULATED TOTALS"));
      display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

      display.setTextSize(2);
      
      display.setCursor(0, 15);
      display.print(F("ON :"));
      if (periodON == 0) display.print(F("Wait")); 
      else printFormattedTime(totalTimeON_ms);

      display.setCursor(0, 40);
      display.print(F("OFF:"));
      if (periodOFF == 0) display.print(F("Wait"));
      else printFormattedTime(totalTimeOFF_ms);

    } else {
      // --- VIEW 1: DYNAMIC REMAINING TIME (HH:MM) ---
      display.setTextSize(2);
      display.setCursor(0, 10);

      unsigned long elapsedTime = currentMillis - modeStartTime;
      unsigned long remainingTime = 0;

      if (relayState == HIGH) { // Motor is ON
        display.println(F("Motor ON"));
        if (totalTimeON_ms > elapsedTime) {
          remainingTime = totalTimeON_ms - elapsedTime;
        }
      } else { // Motor is OFF
        display.println(F("Motor OFF"));
        if (totalTimeOFF_ms > elapsedTime) {
          remainingTime = totalTimeOFF_ms - elapsedTime;
        }
      }

      display.setCursor(0, 35);
      
      if ((relayState == HIGH && periodON == 0) || (relayState == LOW && periodOFF == 0)) {
         display.setTextSize(1);
         display.print(F("Scanning Pulse..."));
      } else {
         display.print(F("- "));
         printFormattedTime(remainingTime);
      }
    }
    
    display.display();
  }
}