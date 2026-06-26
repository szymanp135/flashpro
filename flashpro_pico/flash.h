/*
 * Raspberry Pi Pico based double chip SST39SF020 automated flash memory
 * programmer.
 * Paweł Szymański 06.2026
 *
 * This is a header file of 'flash.c' file. Refer to this file for any
 * explanations.
 */

#ifndef FLASH_H
#define FLASH_H

#ifndef PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS
#define PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS (500)
#endif

#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"

#define SECTOR_SIZE 2 * 4096
#define MEMORY_ADDRESS_SPACE 0x0007FFFF

/* Global variable
 * Variable indicating memory endianess used
 * 0 for little endian / 1 for big endian
 * Global variable usage is justified by the increased ease of use and
 * legibility of code. Otherwise the value would be passed as an argument
 * in almost every function in the file despite not being used directly
 * by functions. As such the decision was taken to shamelessly use
 * a global variable.
 */
extern int ENDIAN;

/* Function declarations */
void flash_gpio_init(void);
void flash_init(void);
void flash_write_address_buffer(uint8_t);
void flash_enable_address(uint32_t);
void flash_disable_address(void);
void flash_put_byte(uint32_t, uint8_t);
void flash_chip_erase(void);
void flash_sector_erase(uint32_t);
void flash_write_byte(uint32_t, uint8_t);
uint8_t flash_read_byte(uint32_t);
void flash_write_sector(uint32_t, uint8_t*);
void flash_read_sector(uint32_t, uint8_t*);
void flash_print_buffer(uint8_t*, int);

#endif
