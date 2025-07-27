#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <driver/i2s.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

#include "utils/mqtt.h"
#include "utils/sound.h"

// MQTT Configuration
const char* mqtt_client_id = "ESP32-washing-machine-notifier";

// Microphone and Beep Detection Configuration
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
#define I2S_PORT I2S_NUM_0
#define I2S_ADC_UNIT ADC_UNIT_1
#define I2S_ADC_CHANNEL ADC1_CHANNEL_7 // GPIO35

const int BEEP_SEQUENCE_COUNT = 2;         // Number of beeps in the finished sequence.
const int MAX_BEEP_DURATION_MS = 1500;      // A single beep should not be longer than this.
const int MAX_BEEP_INTERVAL_MS = 1500;      // The gap between beeps should not be longer than this.
const int MIN_GAP_DURATION_MS = 500;         // The minimum gap between beeps

const int NOISE_CALIBRATION_MULTIPLIER = 5; // Multiplier for setting dynamic threshold. Increase if too sensitive, decrease if not sensitive enough.

const int SAMPLE_RATE = 16000;

// This threshold will be set automatically by the calibration function
volatile int noise_threshold = 100;

enum DetectionState { IDLE, BEEP_DETECTED, GAP_DETECTED };
DetectionState currentState = IDLE;

int beep_counter = 0;
unsigned long last_state_change_time = 0;
unsigned long potential_gap_start_time = 0;

unsigned long last_health_check_time = 0;

// Global Variables
WiFiClient espClient;
PubSubClient client(espClient);

const int I2S_BUFFER_SIZE = 1024;
int16_t audioSamples[I2S_BUFFER_SIZE];

// Function to setup I2S for microphone input
void setupI2S() {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_adc_mode(I2S_ADC_UNIT, I2S_ADC_CHANNEL);
}

// Function to connect to WiFi
void setupWifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Function to reconnect to the MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      // Re-publish discovery message on reconnecting
      publishDiscoveryMessage(client);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Calibrates the noise threshold on startup
void calibrateNoiseLevel() {
  long baselineNoiseLevel = getBaselineNoiseLevel(I2S_PORT, audioSamples, sizeof(audioSamples));

  noise_threshold = baselineNoiseLevel * NOISE_CALIBRATION_MULTIPLIER;
  if(noise_threshold < 100) {
    // Set a reasonable minimum value
    noise_threshold = 100;
  }

  Serial.printf("Calibration complete. Baseline noise: %ld, Threshold set to: %d\n", baselineNoiseLevel, noise_threshold);
}

void setupOTA() {
  ArduinoOTA.setPassword(OTA_UPLOAD_PASSWORD);
  ArduinoOTA.setPort(OTA_UPLOAD_PORT);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);
  setupI2S();

  setupWifi();
  setupOTA();

  client.setServer(MQTT_SERVER_URL, 1883);
  client.setBufferSize(512);

  calibrateNoiseLevel();

  Serial.println("\nStarting detection loop...");
}

void loop() {
  // Handle OTA update requests
  ArduinoOTA.handle();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long current_time = millis();

  // 1. Read audio and calculate average magnitude
  size_t bytes_read = 0;

  i2s_read(I2S_PORT, audioSamples, sizeof(audioSamples), &bytes_read, portMAX_DELAY);

  if (bytes_read == 0) {
    return;
  }

  long average_magnitude = calculateAverageMagnitude(audioSamples, bytes_read);

  bool is_loud = average_magnitude > noise_threshold;

  // 2. Process with the State Machine
  switch (currentState) {
    case IDLE:
      if (is_loud) {
        currentState = BEEP_DETECTED;

        last_state_change_time = current_time;
        beep_counter = 1;

        Serial.printf("[IDLE->BEEP] Beep 1 started. (Level: %ld)\n", average_magnitude);
      }
      break;

    case BEEP_DETECTED:
      if (!is_loud) {
        // If this is the first moment of silence, start the gap timer.
        if (potential_gap_start_time == 0) {
          potential_gap_start_time = current_time;
        }
        // If the silence has lasted long enough, confirm it's a real gap.
        if (current_time - potential_gap_start_time > MIN_GAP_DURATION_MS) {
          currentState = GAP_DETECTED;

          last_state_change_time = current_time;
          potential_gap_start_time = 0; // Reset timer

          Serial.printf("[BEEP->GAP] Beep %d ended (Gap confirmed). Waiting for next beep...\n", beep_counter);
        }
      } else {
        potential_gap_start_time = 0;

        // Also check if the beep has been continuous for too long.
        if (current_time - last_state_change_time > MAX_BEEP_DURATION_MS) {
          currentState = IDLE;

          Serial.printf("[BEEP->IDLE] Noise too long. Resetting sequence.\n");
        }
      }
      break;

    case GAP_DETECTED:
      if (is_loud) {
        currentState = BEEP_DETECTED;

        last_state_change_time = current_time;
        beep_counter++;

        Serial.printf("[GAP->BEEP] Beep %d started. (Level: %ld)\n", beep_counter, average_magnitude);

        if (beep_counter >= BEEP_SEQUENCE_COUNT) {
          Serial.println(">>> SUCCESS: Full beep sequence detected! <<<");

          publishDoneMessage(client);
          
          currentState = IDLE;
          Serial.println(">>> Entering 60-second cooldown... <<<");

          delay(60000);
        }
      } else if (current_time - last_state_change_time > MAX_BEEP_INTERVAL_MS) {
        currentState = IDLE;

        Serial.printf("[GAP->IDLE] Gap was too long. Resetting sequence.\n");
      }
      
      break;
  }
}
