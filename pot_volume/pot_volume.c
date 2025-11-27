#include <avr/io.h>
#include "pot_volume.h"

// Using PC3 / ADC3
#define POT_ADC_CHANNEL   3

void pot_volume_init(void)
{
    // -------- ADC multiplexer setup --------
    // REFS1:0 = 01 -> AVCC with external capacitor at AREF pin
    // MUX[3:0] = 0100 -> ADC3 (PC3)
    ADMUX = (1 << REFS0) | (POT_ADC_CHANNEL & 0x0F);

    // -------- ADC control & status --------
    // ADEN: enable ADC
    // ADPS2:0 = 111 -> prescaler 128 (for 16 MHz -> 125 kHz ADC clock)
    ADCSRA = (1 << ADEN) |
             (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    // Optional: discard the first conversion (some people like to)
    ADCSRA |= (1 << ADSC);           // start dummy conversion
    while (ADCSRA & (1 << ADSC));   // wait for completion
}

uint16_t pot_volume_read_raw(void)
{
    // Select channel again in case other code changed ADMUX
    ADMUX = (ADMUX & 0xF0) | (POT_ADC_CHANNEL & 0x0F);

    // Start conversion
    ADCSRA |= (1 << ADSC);

    // Wait for conversion complete
    while (ADCSRA & (1 << ADSC));

    // 10-bit result (ADCL must be read first, but using ADC macro is fine)
    return ADC;
}

uint8_t pot_volume_get_level(void)
{
    uint16_t raw = pot_volume_read_raw();  // 0–1023

    // Map 0–1023 -> 0–127 by shifting (approximate /8)
    // 1023 >> 3 = 127
    uint8_t volume = (uint8_t)(raw >> 3);

    return volume;
}
