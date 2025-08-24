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
char displayStrBuf[40];

// from PropFonts library
#include "c64enh_font.h"

#include "small4x7_font.h"

// IR constants and imports
#define IR_RECEIVER_PIN 2
#define ENABLE_LED_FEEDBACK true
#define DECODE_NEC
volatile bool irDataReceived = false;
#include <IRremote.hpp>

// Pin configs
#define RELAY_PIN 7
#define LED_PIN 4
#define THERMISTOR_PIN A0
#define RELAY_ON_LEVEL HIGH

// Auto mode
bool AUTO_MODE_ENABLED = false;
float TEMPERATURE_THRESHOLD = 23;
const float temperatureVariationTolerance = 0.5;
#define AUTO_MODE_READ_INTERVAL 360000 // 360000 ms = 6 minutes

// Thermistor configs
int T0;
float R1 = 10000;
float logR2, R2, TemperatureValue;
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
  char tempStrValue[6];

  // Serial monitor
  Serial.print("Current temperature: ");
  Serial.print(value);
  Serial.println(" C");

  // LCD display
  // lcd.clearDisplay();
  clearDisplayMiddleSection();
  dtostrf(value, 5, 2, tempStrValue);
  snprintf(displayStrBuf, 40, "Current temperature: %s", tempStrValue);
  lcd.setFont(Small4x7PL);
  lcd.printStr(ALIGN_CENTER, DISPLAY_MIDDLE_SECTION, displayStrBuf);
  lcd.display();
}

void printTemperatureThresholdValue() {
  char tempStrValue[6];

  // Serial monitor
  Serial.print("Temperature Threshold: ");
  Serial.print(TEMPERATURE_THRESHOLD);
  Serial.println(" C");

  // LCD display
  // lcd.clearDisplay();
  clearDisplayUpperSection();
  dtostrf(TEMPERATURE_THRESHOLD, 5, 2, tempStrValue);
  snprintf(displayStrBuf, 40, "Threshold temperature: %s", tempStrValue);
  lcd.setFont(Small4x7PL);
  lcd.printStr(ALIGN_CENTER, DISPLAY_UPPER_SECTION, displayStrBuf);
  lcd.display();
}

float getThermistorValue() {
  T0 = analogRead(THERMISTOR_PIN);
  R2 = R1 * (1023.0 / (float)T0 - 1.0);
  logR2 = log(R2);
  TemperatureValue = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  TemperatureValue = TemperatureValue - 273.15;
  printCurrentTemperatureValue(TemperatureValue);

  return TemperatureValue;
}

bool getShouldActivateRelay(float currentTemperature, bool relayCurrentlyActive) {
  if (relayCurrentlyActive) {
    if (currentTemperature < TEMPERATURE_THRESHOLD - temperatureVariationTolerance) {
      return false;
    }
    return true;
  }

  // relay currently inactive
  if (currentTemperature > TEMPERATURE_THRESHOLD + temperatureVariationTolerance) {
    return true;
  }

  return false;
}

void increaseTemperatureThreshold() {
  TEMPERATURE_THRESHOLD += 1;
  printTemperatureThresholdValue();
}

void decreaseTemperatureThreshold() {
  TEMPERATURE_THRESHOLD -= 1;
  printTemperatureThresholdValue();
}

void changeAutoModeStatus(bool value) {
  AUTO_MODE_ENABLED = value;
  digitalWrite(LED_PIN, value);

  // LCD display
  // lcd.clearDisplay();
  clearDisplayLowerSection();
  if (AUTO_MODE_ENABLED) {
    snprintf(displayStrBuf, 40, "Auto Mode: Enabled");
  } else {
    snprintf(displayStrBuf, 40, "Auto Mode: Disabled");
  }
  lcd.setFont(Small4x7PL);
  lcd.printStr(ALIGN_CENTER, DISPLAY_LOWER_SECTION, displayStrBuf);
  lcd.display();
}

bool getAutoModeOnOffRelayStatus() {
  const float currentTemperatureValue = getThermistorValue();
  const bool relayCurrentlyActive = digitalRead(RELAY_PIN) == RELAY_ON_LEVEL;
  const bool shouldActivateRelay = getShouldActivateRelay(currentTemperatureValue, relayCurrentlyActive);
  Serial.println("");
  printCurrentTemperatureValue(currentTemperatureValue);
  Serial.print("Variation tolerance: ");
  Serial.println(temperatureVariationTolerance);
  Serial.print("Setting enabled status: ");
  Serial.println(shouldActivateRelay);
  if (shouldActivateRelay) {
    return RELAY_ON_LEVEL;
  } else {
    return !RELAY_ON_LEVEL;
  }
}

void ReceiveCallbackHandler() {
  IrReceiver.decode();

  if (IrReceiver.decodedIRData.decodedRawData == 0xBA45FF00) {
    // Button 1
    digitalWrite(RELAY_PIN, RELAY_ON_LEVEL);
    changeAutoModeStatus(false);
  } else if (IrReceiver.decodedIRData.decodedRawData == 0xB946FF00) {
    // Button 2
    digitalWrite(RELAY_PIN, !RELAY_ON_LEVEL);
    changeAutoModeStatus(false);
  } else if (IrReceiver.decodedIRData.decodedRawData == 0xB847FF00) {
    // Button 3
    changeAutoModeStatus(!AUTO_MODE_ENABLED);
  } else if (IrReceiver.decodedIRData.decodedRawData == 0xBB44FF00) {
    // Button 4
    digitalWrite(LCD_BACKLIGHT, !digitalRead(LCD_BACKLIGHT));
  } else if (IrReceiver.decodedIRData.decodedRawData == 0xE718FF00) {
    // Button arrow up
    increaseTemperatureThreshold();
  } else if (IrReceiver.decodedIRData.decodedRawData == 0xAD52FF00) {
    // Button arrow down
    decreaseTemperatureThreshold();
  }

  // Set data received flag true
  irDataReceived = true;
 
  // Resume receiving
  IrReceiver.resume();
}

void setup() {
  Serial.println("Initializing...");

  // Relay config
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, !RELAY_ON_LEVEL);

  // Led config
  pinMode(LED_PIN, OUTPUT);

  // IR config
  Serial.begin(9600);
  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK);
  IrReceiver.registerReceiveCompleteCallback(ReceiveCallbackHandler);

  // Display config
  pinMode(LCD_BACKLIGHT, OUTPUT);
  digitalWrite(LCD_BACKLIGHT, HIGH);
  lcd.init();
  lcd.cls();
  lcd.setFont(c64enh);
  lcd.printStr(ALIGN_CENTER, 28, "Device ready");
  // lcd.drawRect(0,0,128,64,1);
  // lcd.drawRect(18,20,127-18*2,63-20*2,1);
  lcd.display();

  Serial.println("Device ready");
}

void loop() {
  if (irDataReceived) {
    irDataReceived = false;
  }

  if (AUTO_MODE_ENABLED) {
    digitalWrite(RELAY_PIN, getAutoModeOnOffRelayStatus());

    delay(AUTO_MODE_READ_INTERVAL);
  }
}
