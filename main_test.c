#include <avr/io.h>
#include <util/delay.h>
#include "./avr-printf-main/uart.h"
#include "./Keypad_detection/keypad.h"
#include "./Atmega2esp32/uart_protocol.h"
#include "./imu/imu_guitar.h"    // IMU API ???
#include "./avr-printf-main/uart.h"          // ???
#include "./pot_volume/pot_volume.h"

#define IMU_ADDR 0x6B      // IMU I2C Address

int main(void)
{
    uart_init();
    keypad_init();
    GuitarIMU_init(IMU_ADDR);  // ??? GuitarIMU API
    uart_protocol_init();
    pot_volume_init();

    while (1)
    {
        
        /** get chord and additional func---- **/
        keypad_scan();
        
        if (program_PAUSED == 1) {
            continue;
        }
        
        const char* chord = keypad_get_chord();
        // Volume
        uint8_t volume = pot_volume_get_level();
        /** IMU gesture **/
        uint8_t strum_velocity = 0;
        const char* gesture = GuitarIMU_getStrum(&strum_velocity);
        if (gesture) {
            send_chord_gesture(chord, gesture, strum_velocity, volume);
            printf("Chord=%s,  Gesture=%s, Velocity=%d, Volume=%d \r\n",chord, gesture, strum_velocity, volume); 
        }
    
}
}
