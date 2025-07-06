#include <Arduino.h>

// IR constants and imports
#define IR_RECEIVER_PIN 2
#define ENABLE_LED_FEEDBACK true
#define DECODE_NEC
volatile bool irDataReceived = false;
#include <IRremote.hpp>

// Pin configs
#define RELAY_PIN 7
#define LED_PIN 6
#define THERMISTOR_PIN A0
#define RELAY_ON_LEVEL HIGH

// Auto mode
bool AUTO_MODE_ENABLED = false;
const float TEMPERATURE_THRESHOLD = 19;
const float temperatureVariationTolerance = 0.5;
#define AUTO_MODE_READ_INTERVAL 360000 // 360000 ms = 6 minutes

// Thermistor configs
int T0;
float R1 = 10000;
float logR2, R2, TemperatureValue;
const float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

void printTemperatureValue(float value) {
  Serial.print("Temperature: ");
  Serial.print(value);
  Serial.println(" C");
}

float getThermistorValue() {
  T0 = analogRead(THERMISTOR_PIN);
  R2 = R1 * (1023.0 / (float)T0 - 1.0);
  logR2 = log(R2);
  TemperatureValue = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  TemperatureValue = TemperatureValue - 273.15;
  printTemperatureValue(TemperatureValue);

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

void changeAutoModeStatus(bool value) {
  AUTO_MODE_ENABLED = value;
  digitalWrite(LED_PIN, value);
}

bool getAutoModeOnOffRelayStatus() {
  const float currentTemperatureValue = getThermistorValue();
  const bool relayCurrentlyActive = digitalRead(RELAY_PIN) == RELAY_ON_LEVEL;
  const bool shouldActivateRelay = getShouldActivateRelay(currentTemperatureValue, relayCurrentlyActive);
  Serial.println("");
  printTemperatureValue(currentTemperatureValue);
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
