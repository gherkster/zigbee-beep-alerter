#include <Arduino.h>
#include <driver/i2s.h>

/// @brief Calculate the average magnitude of a audio sample buffer.
/// @param samples The buffer of audio samples.
/// @param bytesRead The size of the buffer in bytes.
/// @return The average magnitude across all samples.
long calculateAverageMagnitude(int16_t samples[], size_t size);

/// @brief Gets the baseline noise level over a 5 second period.
/// @param i2sPort The I2S port to read.
/// @param samples The buffer holding audio samples.
/// @param size The size of the buffer in bytes.
/// @return The baseline noise level.
long getBaselineNoiseLevel(i2s_port_t i2sPort, int16_t samples[], size_t size);
