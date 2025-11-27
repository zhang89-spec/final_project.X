#ifndef POT_VOLUME_H
#define POT_VOLUME_H

#include <stdint.h>

/**
 * Initialize ADC hardware to read the slide potentiometer on PC4 (ADC4).
 * - Uses AVCC as reference (5V)
 * - ADC clock = F_CPU / 128 (125 kHz at 16 MHz)
 */
void pot_volume_init(void);

/**
 * Read raw ADC value from the potentiometer.
 * @return 10-bit ADC value: 0–1023
 */
uint16_t pot_volume_read_raw(void);

/**
 * Get volume level mapped from ADC reading.
 * @return 8-bit volume: 0–255 (0 = min, 255 = max)
 */
uint8_t pot_volume_get_level(void);

#endif // POT_VOLUME_H
