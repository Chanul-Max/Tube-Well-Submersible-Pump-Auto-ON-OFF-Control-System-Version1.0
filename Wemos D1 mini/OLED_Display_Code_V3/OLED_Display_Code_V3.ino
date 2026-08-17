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

// Timing Multipliers for Binary Counters
const unsigned long COUNTER_ON_MAX = 16384;  // 2^14
const unsigned long COUNTER_OFF_MAX = 32768; // 2^15

// Moving Average Settings
const int NUM_SAMPLES = 5; // Average over the last 5 pulses

// Variables for Measuring & Smoothing 555 ON Pulses
unsigned long lastMicrosON = 0;
unsigned long periodON = 0; 
int lastStateON = LOW;
unsigned long pulsesON = 0; 
unsigned long readingsON[NUM_SAMPLES] = {0};
unsigned long totalPeriodsON = 0;
int readIndexON = 0;
unsigned long smoothedPeriodON = 0;

// Tracker for the blinking arrow (ignores tiny <20000us noise)
unsigned long lastStablePeriodON = 0; 
unsigned long lastAdjustTimeON = 0; 

// Variables for Measuring & Smoothing 555 OFF Pulses
unsigned long lastMicrosOFF = 0;
unsigned long periodOFF = 0; 
int lastStateOFF = LOW;
unsigned long pulsesOFF = 0; 
unsigned long readingsOFF[NUM_SAMPLES] = {0};
unsigned long totalPeriodsOFF = 0;
int readIndexOFF = 0;
unsigned long smoothedPeriodOFF = 0;

// Tracker for the blinking arrow (ignores tiny <20000us noise)
unsigned long lastStablePeriodOFF = 0; 
unsigned long lastAdjustTimeOFF = 0; 

// Mode Tracking (Relay State)
int lastRelayState = HIGH; 

// Button & UI State
bool showConfiguredTimes = false; 
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 100;
bool forceDisplayUpdate = true; 

// Auto-Revert Timer Settings
unsigned long lastInteractionTime = 0; 
const unsigned long VIEW_TIMEOUT = 300000; // 5 minutes (in milliseconds)

// Display refresh timer
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 250; // 4 FPS for smooth animations

void setup() {
  pinMode(PIN_555_ON, INPUT);
  pinMode(PIN_555_OFF, INPUT);
  pinMode(PIN_RELAY, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); // Halt if OLED fails
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  lastRelayState = digitalRead(PIN_RELAY);
}

void printFormattedTime(unsigned long long ms) {
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

  // 1. TRACK RELAY STATE TO RESET PULSE COUNTERS
  int relayState = digitalRead(PIN_RELAY);
  if (relayState != lastRelayState) {
    pulsesON = 0;  
    pulsesOFF = 0;
    
    memset(readingsON, 0, sizeof(readingsON));
    totalPeriodsON = 0;
    readIndexON = 0;
    smoothedPeriodON = 0;

    memset(readingsOFF, 0, sizeof(readingsOFF));
    totalPeriodsOFF = 0;
    readIndexOFF = 0;
    smoothedPeriodOFF = 0;
    
    lastRelayState = relayState;
    forceDisplayUpdate = true; 
  }

  // 2. MEASURE & SMOOTH MOTOR ON 555
  int stateON = digitalRead(PIN_555_ON);
  if (stateON == HIGH && lastStateON == LOW) {
    periodON = currentMicros - lastMicrosON;
    lastMicrosON = currentMicros;
    if (pulsesON < COUNTER_ON_MAX) pulsesON++; 

    // Standard Moving Average
    totalPeriodsON = totalPeriodsON - readingsON[readIndexON];
    readingsON[readIndexON] = periodON;
    totalPeriodsON = totalPeriodsON + readingsON[readIndexON];
    readIndexON = (readIndexON + 1) % NUM_SAMPLES;

    if (pulsesON < NUM_SAMPLES) {
      smoothedPeriodON = totalPeriodsON / pulsesON; 
    } else {
      smoothedPeriodON = totalPeriodsON / NUM_SAMPLES; 
    }

    // Check if the value is being actively changed by the potentiometer
    unsigned long diffON = (smoothedPeriodON > lastStablePeriodON) ? (smoothedPeriodON - lastStablePeriodON) : (lastStablePeriodON - smoothedPeriodON);
    if (diffON > 20000) { // 20000us (20ms) threshold ignores small analog noise
      lastAdjustTimeON = currentMillis;
      lastStablePeriodON = smoothedPeriodON;
      lastInteractionTime = currentMillis; // Reset the 5-minute timeout clock
    }
  }
  lastStateON = stateON;

  // 3. MEASURE & SMOOTH MOTOR OFF 555
  int stateOFF = digitalRead(PIN_555_OFF);
  if (stateOFF == HIGH && lastStateOFF == LOW) {
    periodOFF = currentMicros - lastMicrosOFF;
    lastMicrosOFF = currentMicros;
    if (pulsesOFF < COUNTER_OFF_MAX) pulsesOFF++; 

    // Standard Moving Average
    totalPeriodsOFF = totalPeriodsOFF - readingsOFF[readIndexOFF];
    readingsOFF[readIndexOFF] = periodOFF;
    totalPeriodsOFF = totalPeriodsOFF + readingsOFF[readIndexOFF];
    readIndexOFF = (readIndexOFF + 1) % NUM_SAMPLES;

    if (pulsesOFF < NUM_SAMPLES) {
      smoothedPeriodOFF = totalPeriodsOFF / pulsesOFF; 
    } else {
      smoothedPeriodOFF = totalPeriodsOFF / NUM_SAMPLES; 
    }

    // Check if the value is being actively changed by the potentiometer
    unsigned long diffOFF = (smoothedPeriodOFF > lastStablePeriodOFF) ? (smoothedPeriodOFF - lastStablePeriodOFF) : (lastStablePeriodOFF - smoothedPeriodOFF);
    if (diffOFF > 20000) { // 20000us (20ms) threshold ignores small analog noise
      lastAdjustTimeOFF = currentMillis;
      lastStablePeriodOFF = smoothedPeriodOFF;
      lastInteractionTime = currentMillis; // Reset the 5-minute timeout clock
    }
  }
  lastStateOFF = stateOFF;

  // 4. ROBUST BUTTON DEBOUNCE
  int reading = digitalRead(PIN_BUTTON);
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis; 
  }

  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        showConfiguredTimes = !showConfiguredTimes; 
        lastInteractionTime = currentMillis; // Reset the 5-minute timeout clock
        forceDisplayUpdate = true; 
      }
    }
  }
  lastButtonState = reading;

  // 5. AUTO-REVERT VIEW AFTER 5 MINUTES OF INACTIVITY
  if (showConfiguredTimes && (currentMillis - lastInteractionTime > VIEW_TIMEOUT)) {
    showConfiguredTimes = false;
    forceDisplayUpdate = true;
  }

  // 6. UPDATE DISPLAY 
  if (forceDisplayUpdate || (currentMillis - lastDisplayUpdate > DISPLAY_INTERVAL)) {
    lastDisplayUpdate = currentMillis;
    forceDisplayUpdate = false; 
    
    // Core animation clock updating every 250ms
    bool blinkArrow = (currentMillis / 250) % 2; 
    
    display.clearDisplay();

    if (showConfiguredTimes) {
      // --- VIEW 2: SET THE TIME ---
      unsigned long long totalTimeON_ms = ((unsigned long long)COUNTER_ON_MAX * smoothedPeriodON) / 1000ULL;
      unsigned long long totalTimeOFF_ms = ((unsigned long long)COUNTER_OFF_MAX * smoothedPeriodOFF) / 1000ULL;

      display.setTextSize(1);
      display.setCursor(28, 0); 
      display.println(F("SET THE TIME"));

      // Solid Underline
      display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

      display.setTextSize(2);
      
      // Motor ON Time
      if ((currentMillis - lastAdjustTimeON < 2000) && blinkArrow) {
        display.setCursor(0, 18);
        display.print(F(">"));
      }
      display.setCursor(10, 18);
      display.print(F("ON :"));
      if (smoothedPeriodON == 0) display.print(F("Wait")); 
      else printFormattedTime(totalTimeON_ms);

      // Motor OFF Time
      if ((currentMillis - lastAdjustTimeOFF < 2000) && blinkArrow) {
        display.setCursor(0, 42);
        display.print(F(">"));
      }
      display.setCursor(10, 42);
      display.print(F("OFF:"));
      if (smoothedPeriodOFF == 0) display.print(F("Wait")); 
      else printFormattedTime(totalTimeOFF_ms);

    } else {
      // --- VIEW 1: REMAINING TIME ---
      display.setTextSize(1);
      display.setCursor(22, 0); 
      display.println(F("REMAINING TIME"));
      display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

      display.setTextSize(2);
      display.setCursor(10, 20);

      unsigned long long remainingTime_ms = 0;
      int progressPercent = 0; 

      if (relayState == LOW) { // Motor ON mode
        display.print(F("ON :"));
        if (smoothedPeriodON == 0) {
          display.print(F("Wait"));
        } else {
          remainingTime_ms = ((unsigned long long)(COUNTER_ON_MAX - pulsesON) * smoothedPeriodON) / 1000ULL;
          printFormattedTime(remainingTime_ms);
          progressPercent = ((COUNTER_ON_MAX - pulsesON) * 100) / COUNTER_ON_MAX;
        }
      } else { // Motor OFF mode
        display.print(F("OFF:"));
        if (smoothedPeriodOFF == 0) {
          display.print(F("Wait"));
        } else {
          remainingTime_ms = ((unsigned long long)(COUNTER_OFF_MAX - pulsesOFF) * smoothedPeriodOFF) / 1000ULL;
          printFormattedTime(remainingTime_ms);
          progressPercent = ((COUNTER_OFF_MAX - pulsesOFF) * 100) / COUNTER_OFF_MAX;
        }
      }

      // Progress Bar Animation
      int barWidth = 112;
      int barHeight = 12;
      int barX = 8;
      int barY = 46;
      
      display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);
      
      int fillWidth = (progressPercent * (barWidth - 4)) / 100;
      if (fillWidth > 0) {
        display.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, SSD1306_WHITE);
      }
    }
    
    display.display();
  }
}