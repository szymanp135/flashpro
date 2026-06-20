/*
 * Raspberry Pi Pico based double chip SST39SF020 automated flash memory
 * programmer.
 * Paweł Szymański 10.2025-06.2026
 *
 * This software consists of following files:
 * - main.c - main file
 * - flash.c - library of flash functions
 * - state.c - library for handling program state
 *
 * This program is capable of:
 * Automated and convinient flash programming from PC with 'flashpro'
 * software.
 */

#include "flash.h"
#include "state.h"

/* Erase whole chip
 *
 * Returns nothing
 *
 * No arguments
 */
void erase_chip() {
	flash_chip_erase();
}

/* Erase sector containing given address
 *
 * Returns 0 if succeeded and non-zero value otherwise
 *
 * Arguments:
 * address  : address contained in sector to be erased
 */
int erase_sector(int32_t address) {

	if (address < 0)
		return 1;

	flash_sector_erase(address);
	return 0;
}

/* Read data from sector containing given address
 *
 * Returns 0 if succeeded and non-zero otherwise
 *
 * Arguments:
 * address  : address contained by sector to be read
 */
int read_sector(int32_t address) {
	
	uint8_t data[BUFFER_SIZE];

	if (address < 0)
		return 1;
	
	/* Read data from sector */
	flash_read_sector(address, data);
	/* Print data */
	flash_print_buffer(data, BUFFER_SIZE);
	return 0;
}

/* Write data from 'data' buffer to sector containing giver address.
 * If readout is set then read data back after writing and check
 * whether data matches one in the buffer.
 *
 * Returns 0 if succeeded. Otherwise and returns an error value:
 * - 1 if address is invalid
 * - 2 if data buffer is invalid
 * - 3 if readout value is invalid
 * - (0x8000 | sector address) if readout data does not match buffer data
 *
 * Arguments:
 * address  : address contained by sector to be written to
 * data     : buffer with data to be written to the sector
 * readout  : decides about data match check
 */
int write_sector(int32_t address, uint8_t *data, int readout) {
	
	int i;
	uint8_t check_data[BUFFER_SIZE];

	if (address < 0)
		return 1;
	if (!data)
		return 2;
	if (readout < 0)
		return 3;

	/* Erase sector before write */
	flash_sector_erase(address);
	/* Write data to the sector */
	flash_write_sector(address, data);

	/* If readout is set then check data*/
	if (readout) {
		/* Read data from sector */
		flash_read_sector(address, check_data);
		/* Iterate through data and compare */
		for (i = 0; i < BUFFER_SIZE; ++i) {
			if (data[i] ^ check_data[i]) {
				return 0x8000 | i;
			}
		}
	}

	return 0;
}

/* Set memory endianness
 *
 * Returns 0 if succeeded and 4 otherwise.
 *
 * Arguments:
 * endianness   : new memory endianness
 */
int set_endianness(int endianness) {
	
	if (endianness < 0)
		return 4;
	
	ENDIAN = endianness;
	return 0;
}

/* Executes action associated with state or change in state
 *
 * Returns 0 if succeeded. Otherwise an error occured:
 * - 1 if address is invalid
 * - 2 if data is invalid
 * - 3 if readout is not set
 * - 4 if endianness is not set
 * - values greater than 0x8000 indicate readout error from write_sector
 *   function with invalid data address embedded into return value
 *
 * Arguments:
 * state        : current program state
 * prev_state   : previous program state
 */
int handle_state(enum state_program state, enum state_program prev_state) {

	/* Static variables to be remembered across different function calls */
	static int32_t address           = -1;
	static uint8_t data[BUFFER_SIZE] = { 0 };
	static uint8_t *data_pointer     = 0;
	static int readout               = -1;
	static int endianness            = -1;

	int res = 0;
	
	/* Detect exit from command state (to data frame).
	 * Execute command that was exited.
	 */
	if (state == s_data_frame) {
		switch (prev_state) {
			case s_init:
				address = readout = endianness = -1;
				data_pointer = 0;
			case s_data_frame:
				break;
			case s_erase_chip:
				erase_chip();
				break;
			case s_erase_sector:
				res = erase_sector(address);
				address = -1;
				break;
			case s_write_sector:
				res = write_sector(address, data_pointer, readout);
				address = readout = -1;
				data_pointer = 0;
				break;
			case s_read_sector:
				res = read_sector(address);
				address = -1;
				break;
			case s_set_endianness:
				res = set_endianness(endianness);
				endianness = -1;
				break;
		}
	}   
	else {
		/* Read command argument */
		switch (state) {
			/* Address argument */
			case s_erase_sector_address:
			case s_write_sector_address:
			case s_read_sector_address:
				address = parse_int(*getchar);
				break;
			/* Data argument */
			case s_write_sector_data:
				parse_data(data_pointer = data);
				break;
			/* Readout argument */
			case s_write_sector_readout:
				readout = parse_int(*getchar);
				break;
			/* Endianness argument */
			case s_set_endianness_type:
				endianness = parse_int(*getchar);
				break;
		}
	}

	return res;
}

int main() {

	int c;
	
	/* This 'state' variable describes state in which program
	 * currently is
	 */
	enum state_program state = 0;
	/* Program's state of previous character */
	enum state_program prev_state;
	    
	/* Initialize board and memory */
	stdio_init_all();
	stdio_usb_init();
	flash_init();
	
	/* Wait a little for initialization */
	sleep_ms(2000);
	
	while(2137) {
	
		/* Read character */
		c = getchar();
		
		/* Update previous state */
		prev_state = state;
		
		/* Process read character and move program to next state */
		state_handle_char(c, &state);
		
		/* Execute action associated with state */
		handle_state(state, prev_state);
	}
	
	return 0;
}

