#include <Arduino.h>
#include <driver/i2s.h>

long calculateAverageMagnitude(int16_t samplesBuffer[], size_t size) {
    if (size == 0) {
        return 0;
    }
    
    long total_raw = 0;
    long total_magnitude = 0;

    int num_samples = size / sizeof(int16_t);

    for (int i = 0; i < num_samples; i++) {
        total_raw += samplesBuffer[i];
    }

    int dc_offset = total_raw / num_samples;

    for (int i = 0; i < num_samples; i++) {
        total_magnitude += abs(samplesBuffer[i] - dc_offset);
    }

    return total_magnitude / num_samples;
}

long getBaselineNoiseLevel(i2s_port_t i2sPort, int16_t samples[], size_t size) {
    Serial.println("Calibrating noise level for 5 seconds... Please be quiet.");

    long total_magnitude = 0;
    int calibration_reads = 0;

    size_t bytes_read;
    unsigned long start_time = millis();

    while(millis() - start_time < 5000) {
        i2s_read(i2sPort, samples, size, &bytes_read, portMAX_DELAY);

        long averageMagnitude = calculateAverageMagnitude(samples, bytes_read);
        
        total_magnitude += averageMagnitude;
        calibration_reads++;
    }

    // Return the magnitude averaged over the total duration
    return total_magnitude / calibration_reads;
}
