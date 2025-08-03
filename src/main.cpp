#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <ArduinoJson.h>
#include <Zigbee.h>

#include "utils/sound.h"

#define ZIGBEE_BINARY_ENDPOINT 20

// The number of consecutive beeps that must be detected
const int BEEP_SEQUENCE_COUNT = 3;
// The maximum duration of a beep in milliseconds
const int MAX_BEEP_DURATION_MS = 1500;
// The maximum gap between beeps in milliseconds
const int MAX_BEEP_INTERVAL_MS = 1500;
// The minimum gap between beeps in milliseconds
const int MIN_GAP_DURATION_MS = 500;

// Multiplier for setting dynamic threshold. Increase if too sensitive, decrease if not sensitive enough.
const int NOISE_CALIBRATION_MULTIPLIER = 2;

// How many consecutive loud readings are needed to confirm a beep has started
const int CONSECUTIVE_LOUD_SAMPLES_TO_START = 3; 
// How many consecutive quiet readings are needed to confirm a gap has started
const int CONSECUTIVE_QUIET_SAMPLES_TO_START_GAP = 3;

// The interval to sample for audio volume in milliseconds
const unsigned long SAMPLE_INTERVAL_MS = 50;

// This threshold will be set automatically by the calibration function
volatile int noise_threshold = 50;

// The number of counted beeps matching the sequence constraints above
int beep_counter = 0;
// The number of counted beep loud segments matching the sequence constraints above
int loud_samples_count = 0;
// The number of counted beep quiet segments matching the sequence constraints above
int quiet_samples_count = 0;

// The time in milliseconds since the last change in state
unsigned long last_state_change_time = 0;
// The time in milliseconds since the last audio sample was recorded
unsigned long last_sample_time = 0;

enum DetectionState {
  /// @brief No beep cycle has been detected.
  IDLE,
  /// @brief A loud segment of a beep cycle has been detected.
  BEEP_CONFIRMED,
  /// @brief A quiet segment of a beep cycle has been detected.
  GAP_CONFIRMED
};

DetectionState currentState = IDLE;

// Calibrates the noise threshold on startup
void calibrateNoiseLevel() {
  long baselineNoiseLevel = getBaselineNoiseVolume();

  noise_threshold = baselineNoiseLevel * NOISE_CALIBRATION_MULTIPLIER;
  if(noise_threshold < 50) {
    // Set a reasonable minimum value
    noise_threshold = 50;
  }

  Serial.printf("Calibration complete. Baseline noise: %ld, Threshold set to: %d\n", baselineNoiseLevel, noise_threshold);
}

// There is no supported one off event type, so a binary sensor is used to toggle the completion state on and off
ZigbeeBinary zbBinary(ZIGBEE_BINARY_ENDPOINT);

void sendFinishedEvent() {
  Serial.println(">>> SUCCESS: Full beep sequence detected! <<<");

  zbBinary.setBinaryInput(true);
  zbBinary.reportBinaryInput();

  Serial.println(">>> Entering 30-second cooldown... <<<");

  delay(30000);

  // Reset the binary state
  zbBinary.setBinaryInput(false);
  zbBinary.reportBinaryInput();
}

void setup() {
  Serial.begin(115200);

  pinMode(MIC_PIN, INPUT);

  Serial.println("Configuring Zigbee");
  zbBinary.setManufacturerAndModel("ESP32-C6", "WashingMachineStatus");
  zbBinary.addBinaryInput();

  Zigbee.addEndpoint(&zbBinary);

  if (!Zigbee.begin()) {
    Serial.println("FATAL: Zigbee failed to start. Restarting...");
    ESP.restart();
  }
  Serial.println("Zigbee stack started. Waiting for connection...");

  while (!Zigbee.connected()) {
    delay(500);
  }
  Serial.println("Zigbee connected to network!");

  Serial.println("Calibrating sound level");
  calibrateNoiseLevel();

  Serial.println("\nStarting detection loop...");
}

void loop() {
  unsigned long current_time = millis();

  // Only run the detection logic if enough time has passed since the last check.
  if (current_time - last_sample_time < SAMPLE_INTERVAL_MS) {
    return;
  }

  // Update the timestamp for the next interval.
  last_sample_time = current_time;

  long average_magnitude = getSoundVolume();
  bool is_loud = average_magnitude > noise_threshold;

  switch (currentState) {
    case IDLE:
      if (is_loud) {
        loud_samples_count++;
        quiet_samples_count = 0;
      } else {
        loud_samples_count = 0;
      }

      if (loud_samples_count >= CONSECUTIVE_LOUD_SAMPLES_TO_START) {
        currentState = BEEP_CONFIRMED;
        last_state_change_time = current_time;

        beep_counter = 1;
        quiet_samples_count = 0;

        Serial.printf("[IDLE->BEEP] Beep 1 confirmed. (Level: %ld)\n", average_magnitude);
      }
      break;

    case BEEP_CONFIRMED:
      if (!is_loud) {
        quiet_samples_count++;
        loud_samples_count = 0;
      } else {
        quiet_samples_count = 0;
      }

      if (quiet_samples_count >= CONSECUTIVE_QUIET_SAMPLES_TO_START_GAP) {
        currentState = GAP_CONFIRMED;
        last_state_change_time = current_time;

        loud_samples_count = 0;

        Serial.printf("[BEEP->GAP] Gap after beep %d confirmed.\n", beep_counter);
      } else if (current_time - last_state_change_time > MAX_BEEP_DURATION_MS) {
        currentState = IDLE;

        loud_samples_count = 0;
        quiet_samples_count = 0;

        Serial.printf("[BEEP->IDLE] Beep was too long. Resetting sequence.\n");
      }
      break;

    case GAP_CONFIRMED:
      if (is_loud) {
        loud_samples_count++;
        quiet_samples_count = 0;
      } else {
        loud_samples_count = 0;
      }

      if (loud_samples_count >= CONSECUTIVE_LOUD_SAMPLES_TO_START) {
        currentState = BEEP_CONFIRMED;
        last_state_change_time = current_time;

        beep_counter++;
        quiet_samples_count = 0;

        Serial.printf("[GAP->BEEP] Beep %d confirmed. (Level: %ld)\n", beep_counter, average_magnitude);

        if (beep_counter >= BEEP_SEQUENCE_COUNT) {
          sendFinishedEvent();

          currentState = IDLE;

          loud_samples_count = 0;
          quiet_samples_count = 0;
        }
      } else if (current_time - last_state_change_time > MAX_BEEP_INTERVAL_MS) {
        currentState = IDLE;

        loud_samples_count = 0;
        quiet_samples_count = 0;
        
        Serial.printf("[GAP->IDLE] Gap was too long. Resetting sequence.\n");
      }
      break;
  }
}