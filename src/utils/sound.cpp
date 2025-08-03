#include <Arduino.h>
#include "sound.h"

long getSoundVolume() {
    int16_t samples[NUM_SAMPLES];
    for (int i = 0; i < NUM_SAMPLES; i++) {
        samples[i] = analogRead(MIC_PIN);
    }
    
    return calculateMagnitude(samples, NUM_SAMPLES);
}

long calculateMagnitude(int16_t* samples, int numSamples) {
    if (numSamples == 0) {
        return 0;
    }
    
    long total_raw = 0;
    for (int i = 0; i < numSamples; i++) {
        total_raw += samples[i];
    }

    int dc_offset = total_raw / numSamples;

    long total_magnitude = 0;
    for (int i = 0; i < numSamples; i++) {
        total_magnitude += abs(samples[i] - dc_offset);
    }
    return total_magnitude / numSamples;
}

long getBaselineNoiseVolume() {
    Serial.println("Calibrating noise level for 3 seconds... Please be quiet.");
    long total_magnitude = 0;
    int calibration_reads = 0;
    unsigned long start_time = millis();

    while(millis() - start_time < 3000) { // Calibrate for 3 seconds
        total_magnitude += getSoundVolume();
        calibration_reads++;
    }

    if (calibration_reads == 0) return 0;
    return total_magnitude / calibration_reads;
}