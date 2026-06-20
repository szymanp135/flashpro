/*
 * Raspberry Pi Pico based double chip SST39SF020 automated flash memory
 * programmer.
 * Paweł Szymański 06.2026
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

#include "state.h"

#define ERASE_CHIP_COMMAND_SYMBOL     'e'
#define ERASE_SECTOR_COMMAND_SYMBOL   's'
#define WRITE_SECTOR_COMMAND_SYMBOL   'w'
#define SET_ENDIANNESS_COMMAND_SYMBOL 'n'

#define COMMAND_ARG_ADDRESS_SYMBOL    'a'
#define COMMAND_ARG_DATA_SYMBOL       'd'
#define COMMAND_ARG_READOUT_SYMBOL    'r'
#define COMMAND_ARG_ENDIANNESS_SYMBOL 'e'

#define INT_END_SYMBOL                '$'

/* Enum describing all program states */
enum state_program {
	/* Initial states */
	s_init                  = 0x00,
	s_data_frame            = 0x10,

	/* Chip-erase command */
	s_erase_chip            = 0x20,

	/* Sector-erase command */
	s_erase_sector          = 0x30,
	s_erase_sector_address  = 0x31,

	/* Write sector command */
	s_write_sector          = 0x40,
	s_write_sector_address  = 0x41,
	s_write_sector_data     = 0x42,
	s_write_sector_readout  = 0x43,

	/* Read sector command */
	s_read_sector           = 0x50,
	s_read_sector_address   = 0x51,

	/* Set endianness command */
	s_set_endianness        = 0x80,
	s_set_endianness_type   = 0x81,
	
	/* Syntax error state */
	s_error                 = 0xff
};

/* Check if c is a whitespace
 *
 * Returns 1 if c is a whitespace, 0 otherwise.
 *
 * Arguments:
 * c        : character to be checked
 */
int is_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n';
}

/* Convert character to hexadecimal digit value.
 *
 * Returns digit value (0-15) if succeeded and -1 otherwise
 *
 * Arguments:
 * c        : character with digit to be converted
 */
int char_to_digit(int c) {

    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a';
    if (c >= 'A' && c <= 'F')
        return c - 'A';

    return -1;
}

/* Parse text and read integer value in hexadecimal.
 * Reads characters, skips any whitespaces and constructs an integer
 * number until INT_END_SYMBOL sign is read.
 *
 * Returns parsed integer if succeeded and -1 otherwise.
 * Program does not predict usage of negative integers, thus -1 indicates
 * error.
 *
 * Arguments:
 * getchar  : pointer to function reading characters
 */
int parse_int(int *getchar(void)) {

    int c;
    int value = 0;
    int digit;

    do {
        /* Read character */
        c = *getchar();
        /* Skip white spaces */
        if (is_whitespace(c))
            continue;

        /* Convert character to digit */
        digit = char_to_digit(c);
    
        /* Digit is invalid */
        if (digit < 0) {
            /* If it is symbol indicating end of integer then return */
            if (c == INT_END_SYMBOL)
                return value;
            /* Otherwise return rror */
            else
                return -1; 
        }   
        /* Digit is valid - update value */
        else
            value = (value << 4) | digit;

    } while (c != INT_END_SYMBOL);
}

int parse_data(uint8_t *data) {
}

/* Helper function to shorten state_char function. It does the same as
 * state_char function but is dedicated for erase sector states.
 *
 * Returns nothing
 *
 * Arguments:
 * c        : read character
 * state    : pointer to program state
 */
void state_erase_sector(char c, enum state_program *state) {

	switch (*state) {
		/* Erase sector command */
		case s_erase_sector:
			if (c == COMMAND_ARG_ADDRESS_SYMBOL)
				*state = s_erase_sector_address;
			else if (c == ';')
				*state = s_data_frame;
			else
				*state = s_error;
			break;
		/* Erase sector address argument */
		case s_erase_sector_address:
			/* ';' is expected as an end of command since argument
			 * should be read and processed earlier */
			if (c == ';') *state = s_erase_sector; break;
			*state = s_error;
			break;
		/* Other states are forbidden (how they even got here?) */
		default:
			*state = s_error;
	}
}

/* Helper function to shorten state_char function. It does the same as
 * state_char function but is dedicated for write sector states.
 *
 * Returns nothing
 *
 * Arguments:
 * c        : read character
 * state    : pointer to program state
 */
void state_write_sector(char c, enum state_program *state) {
	
	switch (*state) {
		/* Write sector command */
		case s_write_sector:
			switch (c) {
				case COMMAND_ARG_ADDRESS_SYMBOL:
					*state = s_write_sector_address;
					break;
				case COMMAND_ARG_DATA_SYMBOL:
					*state = s_write_sector_data;
					break;
				case COMMAND_ARG_READOUT_SYMBOL:
					*state = s_write_sector_readout;
					break;
				case ';':
					*state = s_data_frame;
					break;
				default:
					*state = s_error;
			}
			break;
		/* Commands arguments */
		case s_write_sector_address:
		case s_write_sector_data:
		case s_write_sector_readout:
			if (c == ';') *state = s_write_sector; break;
			*state = s_error;
			break;
		/* Other states are forbidden (how they even got here?) */
		default:
			*state = s_error;
	}
}

/* Helper function to shorten state_char function. It does the same as
 * state_char function but is dedicated for read sector states.
 *
 * Returns nothing
 *
 * Arguments:
 * c        : read character
 * state    : pointer to program state
 */
void state_read_sector(char c, enum state_program *state) {
	
	switch(*state) {
		/* Read sector command */
		case s_read_sector:
			switch(c) {
				case COMMAND_ARG_ADDRESS_SYMBOL:
					*state = s_read_sector_address;
					break;
				case ';':
					*state = s_data_frame;
					break;
				default:
					*status = s_error;
			}
			break;
		/* Read sector address argument */
		case s_read_sector_address:
			if (c == ';') *state = s_read_sector; break;
			*state = s_error;
			break;
		/* Other states are forbidden (how they even got here?) */
		default:
			*state = s_error;
	}
}

/* Helper function to shorten state_char function. It does the same as
 * state_char function but is dedicated for set endianness states.
 *
 * Returns nothing
 *
 * Arguments:
 * c        : read character
 * state    : pointer to program state
 */
void state_set_endianness(char c, enum state_program *state) {
	
	switch(*state) {
		/* Set endianness command */
		case s_set_endianness:
			switch(c) {
				case COMMAND_ARG_ENDIANNESS_SYMBOL:
					*state = s_set_endianness_type;
					break;
				case ';':
					*state = s_data_frame;
					break;
				default:
					*status = s_error;
			}
			break;
		/* Set endianness type argument */
		case s_set_endianness_type:
			if (c == ';') *state = s_set_endianness; break;
			*state = s_error;
			break;
		/* Other states are forbidden (how they even got here?) */
		default:
			*state = s_error;	
	}
}

/* Main function for character handling in context of state change.
 * Depending on current program state and given character decides what
 * to do with program's state
 *
 * Returns nothing
 *
 * Arguments:
 * c        : character upon which something about the state should be
 *            done
 * state    : pointer to program's state
 */
void state_handle_char(char c, enum state_program *state) {

	if(is_whitespace(c))
		return;

	switch (*state & 0xf0) {
		/* Init state */
		case s_init: *state = c == '{' ? s_data_frame : s_error; break;
		/* Data frame state */
		case s_data_frame:
			switch (c) {
				case ERASE_CHIP_COMMAND_SYMBOL:
					*state = s_erase_chip; break;
				case ERASE_SECTOR_COMMAND_SYMBOL:
					*state = s_erase_sector; break;
				case WRITE_SECTOR_COMMAND_SYMBOL:
					*state = s_write_sector; break;
				case READ_SECTOR_COMMAND_SYMBOL:
					*state = s_read_sector; break;
				case SET_ENDIANNESS_COMMAND_SYMBOL:
					*state = s_set_endianness; break;
				default: *state = s_error;
			}
			break;
		/* Erase chip state*/
		case s_erase_chip:
			if (c == ';') *state = s_data_frame; break;
			*state = s_error;
			break;
		/* Erase sector state*/
		case s_erase_sector:
			state_erase_sector(c, state);
			break;
		/* Write sector state */
		case s_write_sector:
			state_write_sector(c, state);
			break;
		/* Read sector state */
		case s_read_sector:
			state_read_sector(c, state);
			break;
		/* Set endianness state */
		case s_set_endianness:
			state_set_endianness(c, state);
			break;

		/* There are no other state groups to consider */
		default:
			*state = s_error;
	}
}

