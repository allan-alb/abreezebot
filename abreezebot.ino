#include <Arduino.h>

// Display configs
#define LCD_BACKLIGHT  6
#define LCD_CS         10
#define LCD_DC         8
#define LCD_RST        9

#define DISPLAY_UPPER_SECTION 2
#define DISPLAY_MIDDLE_SECTION 28
#define DISPLAY_LOWER_SECTION 50

#include "ST7567_FB.h"
#include <SPI.h>
ST7567_FB lcd(LCD_DC, LCD_RST, LCD_CS);
char displayStrBuf[32];

// from PropFonts library
#include "c64enh_font.h"

#include "small4x7_font.h"

// IR constants and imports
#define IR_RECEIVER_PIN 2
#define DECODE_NEC
// NEC frames need only 68 raw buffer entries; the library default (up to 200) wastes RAM
#define RAW_BUFFER_LENGTH 68
volatile bool irDataReceived = false;
volatile uint32_t lastIrCode = 0;
#include <IRremote.hpp>

// Pin configs
#define RELAY_PIN 7
#define LED_PIN 4
#define THERMISTOR_PIN A0
#define RELAY_ON_LEVEL HIGH

// Auto mode
bool AUTO_MODE_ENABLED = false;
float TEMPERATURE_THRESHOLD = 23;
const float TEMPERATURE_THRESHOLD_MIN = 0;
const float TEMPERATURE_THRESHOLD_MAX = 50;
const float temperatureVariationTolerance = 0.5;
#define AUTO_MODE_READ_INTERVAL 360000UL // 6 minutes
unsigned long lastAutoModeCheckMs = 0;
bool currentRelayActive = false;
bool autoModeNeedsImmediateCheck = false;

// Thermistor configs
const float R1 = 10000;
const float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

void clearDisplayUpperSection() {
  lcd.fillRect(0, DISPLAY_UPPER_SECTION, SCR_WD, 10, 0);
}

void clearDisplayMiddleSection() {
  lcd.fillRect(0, DISPLAY_MIDDLE_SECTION, SCR_WD, 10, 0);
}

void clearDisplayLowerSection() {
  lcd.fillRect(0, DISPLAY_LOWER_SECTION, SCR_WD, 10, 0);
}

void printCurrentTemperatureValue(float value) {
  char tempStrValue[8]; // dtostrf width is a minimum: "-273.15" needs 8 bytes with NUL

  // Serial monitor
  Serial.print(F("Current temperature: "));
  Serial.print(value);
  Serial.println(F(" C"));

  // LCD display
  // lcd.clearDisplay();
  clearDisplayMiddleSection();
  dtostrf(value, 5, 2, tempStrValue);
  snprintf_P(displayStrBuf, sizeof(displayStrBuf), PSTR("Current temperature: %s"), tempStrValue);
  lcd.setFont(Small4x7PL);
  lcd.printStr(ALIGN_CENTER, DISPLAY_MIDDLE_SECTION, displayStrBuf);
  lcd.display();
}

void printTemperatureThresholdValue() {
  char tempStrValue[8];

  // Serial monitor
  Serial.print(F("Temperature Threshold: "));
  Serial.print(TEMPERATURE_THRESHOLD);
  Serial.println(F(" C"));

  // LCD display
  // lcd.clearDisplay();
  clearDisplayUpperSection();
  dtostrf(TEMPERATURE_THRESHOLD, 5, 2, tempStrValue);
  snprintf_P(displayStrBuf, sizeof(displayStrBuf), PSTR("Threshold temperature: %s"), tempStrValue);
  lcd.setFont(Small4x7PL);
  lcd.printStr(ALIGN_CENTER, DISPLAY_UPPER_SECTION, displayStrBuf);
  lcd.display();
}

float getThermistorValue() {
  const int adcValue = analogRead(THERMISTOR_PIN);

  // ADC pegged at a rail means the thermistor is disconnected or shorted
  if (adcValue <= 0 || adcValue >= 1023) {
    return NAN;
  }

  const float R2 = R1 * (1023.0 / (float)adcValue - 1.0);
  const float logR2 = log(R2);
  return (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2)) - 273.15;
}

void setRelay(bool active) {
  currentRelayActive = active;
  digitalWrite(RELAY_PIN, active ? RELAY_ON_LEVEL : !RELAY_ON_LEVEL);
}

// Hysteresis for a cooling device (fan): ON when temp >= threshold + tolerance,
// OFF when temp <= threshold - tolerance, hold current state in between.
// (For a heating device swap the two return values.)
bool getShouldActivateRelay(float currentTemperature, bool relayCurrentlyActive) {
  if (currentTemperature >= TEMPERATURE_THRESHOLD + temperatureVariationTolerance) {
    return true;
  }

  if (currentTemperature <= TEMPERATURE_THRESHOLD - temperatureVariationTolerance) {
    return false;
  }

  return relayCurrentlyActive;
}

void increaseTemperatureThreshold() {
  if (TEMPERATURE_THRESHOLD < TEMPERATURE_THRESHOLD_MAX) TEMPERATURE_THRESHOLD += 1;
  printTemperatureThresholdValue();
  if (AUTO_MODE_ENABLED) autoModeNeedsImmediateCheck = true;
}

void decreaseTemperatureThreshold() {
  if (TEMPERATURE_THRESHOLD > TEMPERATURE_THRESHOLD_MIN) TEMPERATURE_THRESHOLD -= 1;
  printTemperatureThresholdValue();
  if (AUTO_MODE_ENABLED) autoModeNeedsImmediateCheck = true;
}

void changeAutoModeStatus(bool value) {
  AUTO_MODE_ENABLED = value;
  digitalWrite(LED_PIN, value);
  if (value) autoModeNeedsImmediateCheck = true;

  // LCD display
  // lcd.clearDisplay();
  clearDisplayLowerSection();
  if (AUTO_MODE_ENABLED) {
    strcpy_P(displayStrBuf, PSTR("Auto Mode: Enabled"));
  } else {
    strcpy_P(displayStrBuf, PSTR("Auto Mode: Disabled"));
  }
  lcd.setFont(Small4x7PL);
  lcd.printStr(ALIGN_CENTER, DISPLAY_LOWER_SECTION, displayStrBuf);
  lcd.display();
}

void applyAutoModeRelay() {
  const float currentTemperatureValue = getThermistorValue();

  if (isnan(currentTemperatureValue)) {
    // Fail safe: never leave the relay on while the sensor is unreadable
    setRelay(false);
    Serial.println(F("Thermistor fault! Relay off"));
    clearDisplayMiddleSection();
    strcpy_P(displayStrBuf, PSTR("Sensor fault!"));
    lcd.setFont(Small4x7PL);
    lcd.printStr(ALIGN_CENTER, DISPLAY_MIDDLE_SECTION, displayStrBuf);
    lcd.display();
    return;
  }

  const bool shouldActivateRelay = getShouldActivateRelay(currentTemperatureValue, currentRelayActive);
  Serial.println();
  printCurrentTemperatureValue(currentTemperatureValue);
  Serial.print(F("Variation tolerance: "));
  Serial.println(temperatureVariationTolerance);
  Serial.print(F("Setting enabled status: "));
  Serial.println(shouldActivateRelay);
  setRelay(shouldActivateRelay);
}

void handleIrCode(uint32_t code) {
  if (code == 0xBA45FF00) {
    // Button 1
    setRelay(true);
    changeAutoModeStatus(false);
  } else if (code == 0xB946FF00) {
    // Button 2
    setRelay(false);
    changeAutoModeStatus(false);
  } else if (code == 0xB847FF00) {
    // Button 3
    changeAutoModeStatus(!AUTO_MODE_ENABLED);
  } else if (code == 0xBB44FF00) {
    // Button 4
    digitalWrite(LCD_BACKLIGHT, !digitalRead(LCD_BACKLIGHT));
  } else if (code == 0xE718FF00) {
    // Button arrow up
    increaseTemperatureThreshold();
  } else if (code == 0xAD52FF00) {
    // Button arrow down
    decreaseTemperatureThreshold();
  }
}

void ReceiveCallbackHandler() {
  // Ignore failed decodes (noise) and NEC auto-repeat frames from held buttons,
  // so one press always means one action (e.g. no rapid auto-mode toggling)
  if (IrReceiver.decode() && !(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
    lastIrCode = IrReceiver.decodedIRData.decodedRawData;
    irDataReceived = true;
  }
  IrReceiver.resume();
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("Initializing..."));

  // Relay config
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);

  // Led config
  pinMode(LED_PIN, OUTPUT);

  // IR config
  // Feedback LED disabled: it defaults to pin 13, which is the SPI SCK line
  // already owned by the LCD, so it never worked and only risks bus conflicts
  IrReceiver.begin(IR_RECEIVER_PIN, DISABLE_LED_FEEDBACK);
  IrReceiver.registerReceiveCompleteCallback(ReceiveCallbackHandler);

  // Display config
  pinMode(LCD_BACKLIGHT, OUTPUT);
  digitalWrite(LCD_BACKLIGHT, HIGH);
  lcd.init();
  lcd.cls();
  lcd.setFont(c64enh);
  strcpy_P(displayStrBuf, PSTR("Device ready"));
  lcd.printStr(ALIGN_CENTER, 28, displayStrBuf);
  // lcd.drawRect(0,0,128,64,1);
  // lcd.drawRect(18,20,127-18*2,63-20*2,1);
  lcd.display();

  Serial.println(F("Device ready"));
}

void loop() {
  if (irDataReceived) {
    uint32_t code;
    noInterrupts();
    code = lastIrCode;
    irDataReceived = false;
    interrupts();
    handleIrCode(code);
  }

  if (!AUTO_MODE_ENABLED) return;

  const unsigned long now = millis();
  const bool intervalElapsed = (now - lastAutoModeCheckMs) >= AUTO_MODE_READ_INTERVAL;

  if (autoModeNeedsImmediateCheck || intervalElapsed) {
    applyAutoModeRelay();
    lastAutoModeCheckMs = now;
    autoModeNeedsImmediateCheck = false;
  }
}
