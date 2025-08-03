#include <Arduino.h>
#include <driver/i2s.h>

/// @brief Defines the GPIO pin for the microphone's analog output.
#define MIC_PIN 5

/// @brief The number of samples to take for each magnitude calculation.
#define NUM_SAMPLES 256

/// @brief Calculate the current average volume of detected audio.
/// @return The average volume across all samples.
long getSoundVolume();

/// @brief Gets the baseline noise volume over a 3 second period.
/// @return The baseline noise volume.
long getBaselineNoiseVolume();
