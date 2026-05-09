// SPDX-License-Identifier: GPL-2.0-only
/*
 * carl/drivers/timer.c
 *
 * Copyright (C) 2026 MDFJ
 *
 * PS/2 Keyboard driver
 * Handles keyboard input and key event processing
 */

#include <carl/drivers/keyboard.h>

// PS/2 Controller I/O port addresses
#define KB_DATA         0x60  // Data port - read scancodes, write commands
#define KB_STATUS       0x64  // Status port - read controller status
#define KB_CMD          0x64  // Command port - write controller commands

// PS/2 Controller status port flags
#define KB_OUTPUT_READY 0x01  // Output buffer has data (scancode) to read
#define KB_INPUT_FULL   0x02  // Input buffer is full (can't send command yet)

// Interrupt vector assignment (IRQ1 maps to vector 33 in x86 PIC)
#define KEYBOARD_IRQ    1

// Scancode to ASCII lookup table (simplified - US keyboard layout)
// Index = scancode, Value = ASCII character
// Note: This is incomplete and only covers basic characters
static unsigned char scancode_to_ascii[] = {
    0,    0,   '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '-',  '=',  0,    0,
    'q',  'w', 'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  0,    0,    'a',  's',
    'd',  'f', 'g',  'h',  'j',  'k',  'l',  ';',  '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
    'b',  'n', 'm',  ',',  '.',  '/',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0,
};

// Last key scancode received
static volatile unsigned char last_key = 0;

// Flag indicating a key was pressed (not released)
static volatile int key_pressed = 0;

/**
 * keyboard_handler() - Interrupt handler for PS/2 keyboard (IRQ1)
 * 
 * Called by the CPU when the keyboard interrupts (IRQ1).
 * Reads the scancode from the keyboard controller and determines
 * if a key was pressed or released.
 * 
 * Scancode format:
 * - Bits 0-6: Key code
 * - Bit 7: Release flag (0 = pressed, 1 = released)
 * 
 * Note: Does not send EOI (End Of Interrupt) signal to PIC
 * (that should be done by the PIC driver or main IRQ handler)
 */
void keyboard_handler(void) {
    // Read scancode from keyboard controller data port
    unsigned char scancode = 0;
    __asm__("inb %1, %0" : "=a"(scancode) : "Nd"(KB_DATA));
    
    // Check if key pressed (bit 7 = 0) or released (bit 7 = 1)
    if (!(scancode & 0x80)) {
        // Key pressed (bit 7 is clear)
        last_key = scancode;
        key_pressed = 1;
    } else {
        // Key released (bit 7 is set) - subtract 0x80 to get key code
        key_pressed = 0;
    }
    
    // Send End-of-Interrupt signal to PIC (would be done by PIC driver)
    // picb_send_eoi(KEYBOARD_IRQ);
}

/**
 * keyboard_getchar() - Non-blocking keyboard input
 * 
 * @return: ASCII character if a key is pressed, or 0 if no key
 * 
 * Reads the last key pressed and converts its scancode to ASCII.
 * Returns immediately (non-blocking).
 * Returns 0 if no key is currently pressed.
 * 
 * Typically used in a polling loop that checks frequently for input.
 */
unsigned char keyboard_getchar(void) {
    unsigned char result = 0;
    
    // Check if a key is pressed and we have a valid scancode
    if (key_pressed && last_key < sizeof(scancode_to_ascii)) {
        result = scancode_to_ascii[last_key];
        key_pressed = 0;  // Clear flag so we don't read same key twice
    }
    
    return result;
}

/**
 * keyboard_wait_char() - Blocking keyboard input
 * 
 * @return: ASCII character of the key pressed
 * 
 * Blocks (waits) until a key is pressed, then returns the ASCII character.
 * Uses CPU halt (hlt) instruction to sleep between checks for power efficiency.
 * 
 * Useful for interactive prompts where you need to wait for user input.
 */
unsigned char keyboard_wait_char(void) {
    unsigned char ch = 0;
    
    // Loop until we get a valid character
    while (ch == 0) {
        ch = keyboard_getchar();
        __asm__("hlt");  // Sleep CPU until next interrupt
    }
    
    return ch;
}

/**
 * keyboard_status() - Read keyboard controller status byte
 * 
 * @return: Status byte from keyboard controller
 * 
 * Status byte format:
 * - Bit 0: Output buffer has data (output buffer full)
 * - Bit 1: Input buffer has command/data (input buffer full)
 * - Bit 2: System flag
 * - Bit 3: Command/Data flag (0 = data, 1 = command)
 * - Other bits: Various status indicators
 */
static unsigned char keyboard_status(void) {
    unsigned char status = 0;
    __asm__("inb %1, %0" : "=a"(status) : "Nd"(KB_STATUS));
    return status;
}

/**
 * keyboard_wait_output() - Wait for keyboard output buffer to have data
 * 
 * Polls the output buffer status until data is available.
 * Used when expecting a response from the keyboard controller.
 * 
 * Busy-waits (not ideal for real kernels - should use interrupts or timeouts)
 */
static void keyboard_wait_output(void) {
    while (!(keyboard_status() & KB_OUTPUT_READY)) {
        __asm__("nop");  // No-op instruction, minimal delay
    }
}

/**
 * keyboard_install() - Initialize PS/2 keyboard driver
 * 
 * Performs basic setup of the PS/2 keyboard controller:
 * - Clears any pending data
 * - Registers interrupt handler
 * - Unmasks IRQ1 in the Programmable Interrupt Controller (PIC)
 * 
 * Full PS/2 controller initialization would include:
 * 1. Disable keyboard (0xAD command)
 * 2. Flush output buffer
 * 3. Set controller configuration byte
 * 4. Enable keyboard (0xAE command)
 * 5. Reset keyboard (0xFF command)
 * 6. Set keyboard LED state
 * 
 * This simplified version assumes the BIOS has already configured the
 * keyboard, so we just set up interrupt handling.
 */
void keyboard_install(void) {
    // Clear any pending key state
    last_key = 0;
    key_pressed = 0;
    
    // Register interrupt handler for keyboard (IRQ1 -> vector 33)
    // idt_set_handler(33, (unsigned int)keyboard_handler);
    
    // Unmask IRQ1 in the Programmable Interrupt Controller
    // This allows keyboard interrupts to reach the CPU
    // picb_set_mask(~(1 << KEYBOARD_IRQ));  // Clear bit 1 to enable IRQ1
}
