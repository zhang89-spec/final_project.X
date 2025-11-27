#include "./keypad.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include "../avr-printf-main/uart.h" // Assuming this is for debugging
#define F_CPU 16000000UL

// --- Global and Static Variables ---
static uint8_t current_group = 0;
static char next_chord[10] = "";
static uint8_t autoplay_state = 0;

// Current row being driven low (0..3), used in the ISR to identify the row.
static int8_t active_row = -1; 
// Flag indicating if a row is currently being driven low and awaiting interrupt.
volatile uint8_t scan_pending = 0; 
// program pause flag
volatile uint8_t program_PAUSED = 1;

static const char key_map[4][3] = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
};

// const char* chord_map[2][12] = {
//     { // Group 1: majors + dominant 7ths (0..11)
//         "C",  "G",  "D",  "A",  "E",  "F",
//         "C7", "G7", "D7", "A7", "E7", "B7"
//     },
//     { // Group 2: minors + sus & dim (12..23)
//         "Am",   "Em",   "Dm",   "Bm",   "F#m",  "Gm",
//         "Dsus4","Gsus4","Asus4","Esus4","Bdim","F#dim"
//     }
// };

const char* chord_map[2][12] = {
    {   // Group 1: majors + dominant 7ths (0..11)
        "F#m",  "G"    ,  "Am",  
        "C"  ,  "F#dim",  "F",
        "C7" ,  "G7"   ,  "D7", 
        "A7" ,  "E7"   ,  "B7"
    },
    {   // Group 2: minors + sus & dim (12..23)
        "C",   "G",   "Am",   
        "Em",   "Dm",  "Gm",
        "Dsus4","Gsus4","Asus4",
        "Esus4","Bdim","F#dim"
    }
    
};

// Function prototype for the key processing logic
// char process_keypad_press(uint8_t cols);


// --- Initialization Function ---
void keypad_init()
{
    // --- 3x4 Matrix Keypad Configuration ---
    // Rows (PD4-PD7): Output, start High
    ROW_DDR |= ROW_MASK;
    ROW_PORT |= ROW_MASK; 
    // Cols (PB0-PB2): Input, Pull-up
    COL_DDR &= ~COL_MASK;
    COL_PORT |= COL_MASK; 

    // --- Independent Button Configuration ---
    // Group buttons (PC0-PC2): Input, Pull-up
    BTN_DDR &= ~(BTN1_MASK | BTN2_MASK | BTN3_MASK);
    BTN4_DDR &= ~BTN4_MASK; //PD3
    // Configure pull-ups for the button pins
    BTN_PORT |= (BTN1_MASK | BTN2_MASK | BTN3_MASK); 
    BTN4_PORT |= BTN4_MASK; //PD3
    // --- Interrupt Configuration (New) ---
    
    // 1. Configure Column Pins (PB0-PB2) for Pin Change Interrupt (PCINT0 group)
    PCICR |= (1 << PCIE0);     // Enable Pin Change Interrupt 0 (for PORTB)
    PCMSK0 |= COL_MASK;        // Allow PB0, PB1, PB2 to trigger interrupt
    
    // 2. Configure Independent Button Pins (PC0-PC2) for Pin Change Interrupt (PCINT1 group)
    PCICR |= (1 << PCIE1);     // Enable Pin Change Interrupt 1 (for PORTC)
    // Allow PC0, PC1, PC2 to trigger interrupt
    PCMSK1 |= (BTN1_MASK | BTN2_MASK | BTN3_MASK); 

    // 3. Configure BTN4 Pin (PD3) for PCINT2 group
    // PCIE2 controls PCINT23..16 (for PORTD)
    PCICR |= (1 << PCIE2); 
    // PD3 corresponds to PCINT19 (3 + 16 = 19). We use BTN4_MASK to set the bit.
    PCMSK2 |= BTN4_MASK; // Assumes BTN4_MASK is (1 << 3)

    // Enable Global Interrupts
    sei();
}

// --- Scanning/Driving Function (Now only responsible for driving one row and setting the pending flag) ---
void keypad_scan()
{
    // If a previous scan is still waiting for an interrupt, skip this cycle
    // if (scan_pending) {
    //     return '\0';
    // }

    // Drive one row low sequentially and set active_row to await interrupt
    for (uint8_t r = 0; r < 4; r++) {
        // 1. Set all rows high first
        ROW_PORT |= ROW_MASK; 
        
        // 2. Drive the current row r low
        ROW_PORT &= ~(1 << (r + 4));
        
        // 3. Record the current row and set the pending flag
        active_row = r;
        scan_pending = 1;

        // Short delay to allow signal stabilization before checking
        _delay_us(50); 
    }
    
    // If all four rows have been scanned, reset active_row and all rows to high
    ROW_PORT |= ROW_MASK;
    active_row = -1;
    scan_pending = 0;
    
    // return '\0';
}


// --- Helper Function: Process Key Press ---
char process_keypad_press(uint8_t cols) {
    
    // Software debounce
    _delay_ms(15);
    cols = (COL_PIN & COL_MASK);
    if (cols == COL_MASK) { // If key released after debounce, ignore
        return '\0';
    }
    
    // Find the pressed column
    for (uint8_t c = 0; c < 3; c++) {
        if (!(cols & (1 << c))) {
            // Found key at (active_row, c)
            char key = key_map[active_row][c];

            // Reset rows to high (idle state)
            ROW_PORT |= ROW_MASK;
            active_row = -1;
            scan_pending = 0;
            autoplay_state = 0;
            
            return key;
        }
    }
    
    // Should not happen
    ROW_PORT |= ROW_MASK;
    active_row = -1;
    scan_pending = 0;
    return '\0';
}


// --- Interrupt Service Routines (ISRs) ---

// PCINT0 Interrupt Handler (for 3x4 Keypad Columns PB0-PB2)
ISR(PCINT0_vect) {
    
    // 1. Clear the interrupt pending flag
    scan_pending = 0;
    
    // 2. Read column pin states
    uint8_t cols = (COL_PIN & COL_MASK);
    
    // 3. Check if the interrupt was triggered by a press (High-to-Low transition)
    // If pressed, at least one bit in cols will be low (0)
    if (cols != COL_MASK) { 
        // 4. Process the key: the current active_row is the pressed row
        char key = process_keypad_press(cols);
        if (key != '\0') {
            keypad_process(key);
        }
    } 
    // Note: A key release (Low-to-High) will also trigger the interrupt, 
    // but we only process the press (low state).
}

// PCINT1 Interrupt Handler (for Independent Buttons PC0-PC2)
ISR(PCINT1_vect) {
    
    // 1. Read button pin states
    uint8_t btns = (BTN_PIN & (BTN1_MASK | BTN2_MASK | BTN3_MASK));
    
    // 2. Check which button is pulled low (pressed)
    
    // Button1 -> Group 0
    if (!(btns & BTN1_MASK)) {
        _delay_ms(15); // Debounce
        if (!(BTN_PIN & BTN1_MASK)) { // Re-check
            current_group = 0;
            printf("Button1 pressed, current group: %d\r\n", current_group);
        }
    }
    
    // Button2 -> Group 1
    else if (!(btns & BTN2_MASK)) {
        _delay_ms(15); // Debounce
        if (!(BTN_PIN & BTN2_MASK)) { // Re-check
            current_group = 1;
            printf("Button2 pressed, current group: %d\r\n", current_group);
        }
    }
    
    // Button3 -> Autoplay
    else if (!(btns & BTN3_MASK)) {
        _delay_ms(15); // Debounce
        if (!(BTN_PIN & BTN3_MASK)) { // Re-check
            autoplay_state = 1;
            strcpy(next_chord, "AUTOKEY");
        }
    }
    
    // Note: The ISR is triggered by both press and release; logic only executes on low state (press).
}

ISR(PCINT2_vect) {
    if (!(BTN4_PIN & BTN4_MASK)) {
        _delay_ms(90); // Small non-blocking delay for initial stability check
        if (!(BTN4_PIN & BTN4_MASK)) { 
            // Toggle the program state flag
            program_PAUSED = 1 - program_PAUSED; // Toggles between 0 and 1
        }
    }
}

// --- Other Functions Remain Unchanged ---

void keypad_process(char key)
{
    if (!key) return;

    int index = -1;
    switch (key) {
    case '1': index = 0; break;
    case '2': index = 1; break;
    case '3': index = 2; break;
    case '4': index = 3; break;
    case '5': index = 4; break;
    case '6': index = 5; break;
    case '7': index = 6; break;
    case '8': index = 7; break;
    case '9': index = 8; break;
    case '*': index = 9; break;
    case '0': index = 10; break;
    case '#': index = 11; break;
    default: index = -1; break;
    }

    if (index >= 0) {
        strcpy(next_chord, chord_map[current_group][index]);
    }
}

const char* keypad_get_chord()
{
    return next_chord;
}