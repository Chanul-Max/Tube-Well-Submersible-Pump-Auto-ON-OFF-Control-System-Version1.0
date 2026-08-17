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

// Variables for Measuring & Counting 555 ON Pulses
unsigned long lastMicrosON = 0;
unsigned long periodON = 0; 
int lastStateON = LOW;
unsigned long pulsesON = 0; 

// Variables for Measuring & Counting 555 OFF Pulses
unsigned long lastMicrosOFF = 0;
unsigned long periodOFF = 0; 
int lastStateOFF = LOW;
unsigned long pulsesOFF = 0; 

// Mode Tracking (Relay State)
int lastRelayState = HIGH; // Default to HIGH (Motor OFF in Active-LOW logic)

// Button & UI State
bool showConfiguredTimes = false; 
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Display & Serial refresh timers
unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialUpdate = 0;

void setup() {
  Serial.begin(115200);
  delay(1000); // Give Serial Monitor time to connect
  Serial.println(F("\n--- SYSTEM BOOT ---"));

  pinMode(PIN_555_ON, INPUT);
  pinMode(PIN_555_OFF, INPUT);
  pinMode(PIN_RELAY, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("CRITICAL: OLED failed to initialize"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  lastRelayState = digitalRead(PIN_RELAY);
  Serial.print(F("Initial Relay State: "));
  // INVERTED LOGIC: LOW is Motor ON
  Serial.println(lastRelayState == LOW ? F("MOTOR ON") : F("MOTOR OFF"));
}

// Helper: Format and print time to OLED strictly in HH:MM
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
    Serial.println(F("\n>>> RELAY STATE CHANGE DETECTED <<<"));
    Serial.print(F("New Mode: "));
    // INVERTED LOGIC: LOW is Motor ON
    Serial.println(relayState == LOW ? F("MOTOR ON") : F("MOTOR OFF"));
    Serial.println(F("Resetting pulse counters to 0."));
    
    pulsesON = 0;  
    pulsesOFF = 0;
    lastRelayState = relayState;
  }

  // 2. MEASURE PERIOD & COUNT PULSES FOR MOTOR ON 555
  int stateON = digitalRead(PIN_555_ON);
  if (stateON == HIGH && lastStateON == LOW) {
    periodON = currentMicros - lastMicrosON;
    lastMicrosON = currentMicros;
    if (pulsesON < COUNTER_ON_MAX) pulsesON++; 
    
    Serial.print(F("[555 ON] Pulse Detected! Count: "));
    Serial.print(pulsesON);
    Serial.print(F(" | Period: "));
    Serial.print(periodON);
    Serial.println(F(" us"));
  }
  lastStateON = stateON;

  // 3. MEASURE PERIOD & COUNT PULSES FOR MOTOR OFF 555
  int stateOFF = digitalRead(PIN_555_OFF);
  if (stateOFF == HIGH && lastStateOFF == LOW) {
    periodOFF = currentMicros - lastMicrosOFF;
    lastMicrosOFF = currentMicros;
    if (pulsesOFF < COUNTER_OFF_MAX) pulsesOFF++; 
    
    Serial.print(F("[555 OFF] Pulse Detected! Count: "));
    Serial.print(pulsesOFF);
    Serial.print(F(" | Period: "));
    Serial.print(periodOFF);
    Serial.println(F(" us"));
  }
  lastStateOFF = stateOFF;

  // 4. BUTTON DEBOUNCE & TOGGLE VIEW
  int reading = digitalRead(PIN_BUTTON);
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) { 
      showConfiguredTimes = !showConfiguredTimes; 
      lastDebounceTime = currentMillis + 200;     
      
      Serial.print(F("Button Pressed! View changed to: "));
      Serial.println(showConfiguredTimes ? F("TOTAL TIMES") : F("REMAINING TIME"));
    }
  }
  lastButtonState = reading;

  // 5. PERIODIC SERIAL HEARTBEAT (Every 2 seconds)
  if (currentMillis - lastSerialUpdate > 2000) {
    lastSerialUpdate = currentMillis;
    Serial.println(F("--- SYSTEM STATUS ---"));
    Serial.print(F("Active Mode: "));
    // INVERTED LOGIC: LOW is Motor ON
    Serial.println(relayState == LOW ? F("MOTOR ON") : F("MOTOR OFF"));
    Serial.print(F("ON Timer  -> Pulses: ")); Serial.print(pulsesON); Serial.print(F("/")); Serial.print(COUNTER_ON_MAX); Serial.print(F(" | Period: ")); Serial.println(periodON);
    Serial.print(F("OFF Timer -> Pulses: ")); Serial.print(pulsesOFF); Serial.print(F("/")); Serial.print(COUNTER_OFF_MAX); Serial.print(F(" | Period: ")); Serial.println(periodOFF);
    Serial.println(F("---------------------"));
  }

  // 6. UPDATE DISPLAY (Every 200ms)
  if (currentMillis - lastDisplayUpdate > 200) {
    lastDisplayUpdate = currentMillis;
    display.clearDisplay();

    if (showConfiguredTimes) {
      unsigned long long totalTimeON_ms = ((unsigned long long)COUNTER_ON_MAX * periodON) / 1000ULL;
      unsigned long long totalTimeOFF_ms = ((unsigned long long)COUNTER_OFF_MAX * periodOFF) / 1000ULL;

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
      display.setTextSize(2);
      display.setCursor(0, 10);

      unsigned long long remainingTime_ms = 0;

      // INVERTED LOGIC: LOW is Motor ON
      if (relayState == LOW) { 
        display.println(F("Motor ON"));
        remainingTime_ms = ((unsigned long long)(COUNTER_ON_MAX - pulsesON) * periodON) / 1000ULL;
      } else { 
        display.println(F("Motor OFF"));
        remainingTime_ms = ((unsigned long long)(COUNTER_OFF_MAX - pulsesOFF) * periodOFF) / 1000ULL;
      }

      display.setCursor(0, 35);
      
      // INVERTED LOGIC: Check appropriate states before scanning finishes
      if ((relayState == LOW && periodON == 0) || (relayState == HIGH && periodOFF == 0)) {
         display.setTextSize(1);
         display.print(F("Scanning Pulse..."));
      } else {
         display.print(F("- "));
         printFormattedTime(remainingTime_ms);
      }
    }
    
    display.display();
  }
}